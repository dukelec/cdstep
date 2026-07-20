/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include <math.h>
#include "app_main.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern DAC_HandleTypeDef hdac1;

static gpio_t drv_en = { .group = DRV_EN_GPIO_Port, .num = DRV_EN_Pin };
static gpio_t drv_md1 = { .group = DRV_MD1_GPIO_Port, .num = DRV_MD1_Pin };
static gpio_t drv_md2 = { .group = DRV_MD2_GPIO_Port, .num = DRV_MD2_Pin };
static gpio_t drv_md3 = { .group = DRV_MD3_GPIO_Port, .num = DRV_MD3_Pin };
static gpio_t drv_dir = { .group = DRV_DIR_GPIO_Port, .num = DRV_DIR_Pin };
static gpio_t drv_mo = { .group = DRV_MO_GPIO_Port, .num = DRV_MO_Pin };
static bool limit_disable = false;

static step_pwm_t step_pwm = {
        .dir_pin = &drv_dir,
        .tim_cnt = &htim2,
        .tim_pwm = &htim3,
        .tim_pwm_ch = TIM_CHANNEL_1
};
static trap_planner_t trap_planner = {
        .dt = 1.0f / LOOP_FREQ
};
static pid_i_t pid_pos = {
        .dt = 1.0f / LOOP_FREQ
};


uint8_t motor_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat)
{
    uint32_t flags;
    static uint8_t last_csa_state = 0;
    bool running = csa.state == ST_POS_TP;
    bool was_running = last_csa_state == ST_POS_TP;
    gpio_set_val(&drv_en, running);

    local_irq_save(flags);
    csa2pid_pos(&pid_pos);
    csa2trap_planner_mt(&trap_planner);
    local_irq_restore(flags);

    if (running) {
        local_irq_save(flags);
        if (csa.tp_pos != csa.tgt_pos)
            trap_planner.state = 1;
        local_irq_restore(flags);
    }

    if (!running && was_running) {
        local_irq_save(flags);
        csa.tp_pos = csa.meas_pos;
        local_irq_restore(flags);
    }

    last_csa_state = csa.state;
    return 0;
}

uint8_t ref_volt_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat)
{
    d_debug("set reference voltage: %d mv\n", csa.ref_volt);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (csa.ref_volt / 1000.0f) * 0x0fff / 3.3f);
    return 0;
}

uint8_t drv_mo_r_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat)
{
    csa.drv_mo = gpio_get_val(&drv_mo);
    d_debug("read drv_mo: %d\n", csa.drv_mo);
    return 0;
}


void app_motor_init(void)
{
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (csa.ref_volt / 1000.0f) * 0x0fff / 3.3f);

    gpio_set_val(&drv_md1, csa.md_val & 1);
    gpio_set_val(&drv_md2, csa.md_val & 2);
    gpio_set_val(&drv_md3, csa.md_val & 4);
    gpio_set_val(&drv_en, csa.state == ST_POS_TP);
    csa2pid_pos(&pid_pos);
    csa2trap_planner_mt(&trap_planner);
    pid_i_reset(&pid_pos, 0);

    d_info("init pwm...\n");
    step_pwm_init(&step_pwm);

    __HAL_TIM_ENABLE(&htim1);
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

void app_motor_maintain(void)
{
    uint32_t flags;

    if (csa.set_home == 0x80 && step_pwm.last_is_0) {
        // after setting home, do not set tc_pos too close to 0 to avoid impacting the mechanical limits
        local_irq_save(flags);
        csa.tp_pos = csa.tgt_pos = csa.meas_pos = 0;
        step_pwm_clr_cnt(&step_pwm);
        trap_planner_reset(&trap_planner, 0, 0);
        pid_i_reset(&pid_pos, 0);
        pid_i_set_target(&pid_pos, csa.meas_pos);
        csa.tgt_speed = 0;
        local_irq_restore(flags);
        d_debug("after set home meas_pos: %ld\n", csa.meas_pos);
        csa.set_home = 0;
    }

    if (csa.state != ST_POS_TP && !csa.tp_state)
        step_pwm_clr_cnt(&step_pwm);

    if (!csa.tp_state)
        limit_disable = false;
}


void timer_isr(void)
{
    if (csa.state != ST_POS_TP) {
        trap_planner_reset(&trap_planner, csa.meas_pos, 0);
        trap_planner2csa_rst(&trap_planner);

        //pid_i_reset(&pid_pos, 0);
        pid_i_set_target(&pid_pos, csa.meas_pos);
        csa.tgt_pos = csa.meas_pos;
        csa.tgt_speed = 0;

    } else {
        csa.meas_pos = step_pwm_get_cnt(&step_pwm);

        csa2trap_planner(&trap_planner, limit_disable);
        trap_planner_update(&trap_planner, csa.meas_pos);
        trap_planner2csa(&trap_planner);

        pid_i_set_target(&pid_pos, csa.tgt_pos);
        csa.tgt_speed = lroundf(pid_i_update_p_only(&pid_pos, csa.meas_pos)) + csa.tp_vel_out;

        int32_t stop_speed = max((int32_t)(csa.tp_accel / LOOP_FREQ), 50);
        if (!csa.tgt_speed || (csa.meas_pos == csa.tgt_pos && abs(csa.tgt_speed) <= stop_speed)) {
            step_pwm_set(&step_pwm, 0);
        } else {
            // tc_vc: step / sec; tim3 unit: 1 / 64000000Hz
            int tim_val = DIV_ROUND_CLOSEST(64000000, csa.tgt_speed);
            step_pwm_set(&step_pwm, tim_val);
        }
    }

    if (csa.dbg_raw_en <= 1)
        raw_dbg(0);
    else
        raw_dbg(1);
    csa.loop_cnt++;
}

void limit_det_isr(void)
{
    d_debug("lim: detected, %d\n", limit_disable);
    if (!csa.lim_en || limit_disable)
        return;
    limit_disable = true;

    // when performing detection, it is recommended to set tc_speed smaller and tc_accel larger
    csa.tp_pos = csa.meas_pos;
}
