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

static int32_t pos_at_cnt0 = 0; // backup cur_pos at start
static bool is_last_0 = false;  // for set_pwm
// add time space before and after drv_dir switch (not necessary)
static bool wait_before_dir_chg = false;
static bool wait_after_dir_chg = false;

static trap_planner_t trap_planner = {
        .dt = 1.0f / LOOP_FREQ
};


static void set_pwm(int value)
{
    if (wait_before_dir_chg) {
        gpio_set_val(&drv_dir, (value >= 0));
        wait_before_dir_chg = false;
        wait_after_dir_chg = true;
        return;
    }
    if (wait_after_dir_chg) {
        wait_after_dir_chg = false;
        return;
    }

    if (value == 0 || gpio_get_val(&drv_dir) != (value >= 0)) {
        if (!is_last_0) {
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0); // pause pwm
            __HAL_TIM_SET_AUTORELOAD(&htim3, 65535);
            __HAL_TIM_SET_PRESCALER(&htim3, 4-1);
            // after pausing pwm the counter must be read again and then cleared
            int counter_dir = gpio_get_val(&drv_dir) ? 1 : -1;
            csa.cur_pos = pos_at_cnt0 + __HAL_TIM_GET_COUNTER(&htim2) * counter_dir;
            __HAL_TIM_SET_COUNTER(&htim2, 0);
            pos_at_cnt0 = csa.cur_pos;
        }
        is_last_0 = true;
        wait_before_dir_chg = true;
        return;
    }

    // 16: 250ns, auto-reload: 16*2-1, 65536*4: 4ms
    value = clip(abs(value), 16*2-1, 65536*4-1);
    int div = value / 65536;
    value = value / (div + 1);
    __HAL_TIM_SET_PRESCALER(&htim3, div);
    __HAL_TIM_SET_AUTORELOAD(&htim3, value);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 16);
    is_last_0 = false;
}

uint8_t motor_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat)
{
    uint32_t flags;
    static uint8_t last_csa_state = 0;
    gpio_set_val(&drv_en, csa.state);

    if (csa.state) {
        local_irq_save(flags);
        if (csa.tp_pos != csa.cal_pos)
            trap_planner.state = 1;
        local_irq_restore(flags);
    }

    if (!csa.state && last_csa_state) {
        local_irq_save(flags);
        csa.tp_pos = csa.cur_pos;
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
    gpio_set_val(&drv_en, csa.state);
    pid_i_reset(&csa.pid_pos, 0);

    __HAL_TIM_ENABLE(&htim1);
    __HAL_TIM_ENABLE(&htim2);

    d_info("init pwm...\n");
    //__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 16);
    //__HAL_TIM_SET_AUTORELOAD(&htim3, 65535);
    //__HAL_TIM_SET_PRESCALER(&htim3, 4-1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0); // pause pwm
    __HAL_TIM_SET_COUNTER(&htim2, 0); // tim2 count at pos edge of tim3 ch1 pwm
    set_pwm(0); // init flags

    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

void app_motor_maintain(void)
{
    uint32_t flags;

    if (csa.set_home && is_last_0) {
        // after setting home, do not set tc_pos too close to 0 to avoid impacting the mechanical limits
        local_irq_save(flags);
        pos_at_cnt0 = csa.cal_pos = csa.tp_pos = csa.cur_pos = 0;
        pid_i_reset(&csa.pid_pos, 0);
        pid_i_set_target(&csa.pid_pos, csa.cur_pos);
        csa.cal_speed = 0;
        local_irq_restore(flags);
        d_debug("after set home cur_pos: %d\n", csa.cur_pos);
        csa.set_home = false;
    }

    if (!csa.state && !csa.tp_state) {
        pos_at_cnt0 = csa.cur_pos;
        __HAL_TIM_SET_COUNTER(&htim2, 0);
    }

    if (!csa.tp_state)
        limit_disable = false;
}


void timer_isr(void)
{
    if (!csa.state) {
        trap_planner_reset(&trap_planner, csa.cur_pos, 0);
        trap_planner2csa_rst(&trap_planner);

        //pid_i_reset(&csa.pid_pos, csa.cur_pos, 0);
        pid_i_set_target(&csa.pid_pos, csa.cur_pos);
        csa.cal_pos = csa.cur_pos;
        csa.cal_speed = 0;

    } else {
        int counter_dir = gpio_get_val(&drv_dir) ? 1 : -1;
        csa.cur_pos = pos_at_cnt0 + __HAL_TIM_GET_COUNTER(&htim2) * counter_dir;

        csa2trap_planner(&trap_planner, limit_disable);
        trap_planner_update(&trap_planner, csa.cur_pos);
        trap_planner2csa(&trap_planner);

        pid_i_set_target(&csa.pid_pos, csa.cal_pos);
        csa.cal_speed = pid_i_update_p_only(&csa.pid_pos, csa.cur_pos) + csa.tp_vel_out;
    }

    if (fabsf(csa.cal_speed) <= max((float)csa.tp_accel / LOOP_FREQ, 50.0f)) {
        set_pwm(0);
    } else {
        // tc_vc: step / sec; tim3 unit: 1 / 64000000Hz
        int tim_val = lroundf(64000000 / csa.cal_speed);
        set_pwm(tim_val);
    }

    raw_dbg(0);
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
    csa.tp_pos = csa.cur_pos;
}
