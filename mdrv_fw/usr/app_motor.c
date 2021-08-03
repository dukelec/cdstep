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
extern DAC_HandleTypeDef hdac1;

static gpio_t drv_en = { .group = DRV_EN_GPIO_Port, .num = DRV_EN_Pin };
static gpio_t drv_md1 = { .group = DRV_MD1_GPIO_Port, .num = DRV_MD1_Pin };
static gpio_t drv_md2 = { .group = DRV_MD2_GPIO_Port, .num = DRV_MD2_Pin };
static gpio_t drv_md3 = { .group = DRV_MD3_GPIO_Port, .num = DRV_MD3_Pin };
static gpio_t drv_step = { .group = DRV_STEP_GPIO_Port, .num = DRV_STEP_Pin };
static gpio_t drv_dir = { .group = DRV_DIR_GPIO_Port, .num = DRV_DIR_Pin };
static gpio_t drv_mo = { .group = DRV_MO_GPIO_Port, .num = DRV_MO_Pin };
static bool limit_disable = false;


uint8_t motor_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat)
{
    uint32_t flags;
    static uint8_t last_csa_state = 0;
    gpio_set_value(&drv_en, csa.state);

    local_irq_save(flags);
    if (csa.state && csa.tc_state == 2)
        csa.tc_state = 1;

    if (csa.state && !csa.tc_state && csa.tc_pos != csa.cur_pos) {
        local_irq_restore(flags);

        d_debug("run motor ...\n");
        csa.tc_vc = sign(csa.tc_pos - csa.cur_pos) * (float)csa.tc_speed_min;
        int tim_val = abs(lroundf(1000000.0f / csa.tc_vc));

        gpio_set_value(&drv_dir, csa.tc_pos >= csa.cur_pos); // 0: -, 1: +
        csa.tc_state = 1;

        __HAL_TIM_SET_AUTORELOAD(&htim1, tim_val);
        __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
        __HAL_TIM_ENABLE(&htim1);
    } else {
        local_irq_restore(flags);
    }

    if (!csa.state && last_csa_state) {
        d_debug("disable motor ...\n");
        local_irq_save(flags);
        csa.tc_pos = csa.cur_pos;
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


void app_motor_init(void)
{
    set_led_state(LED_POWERON);

    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, (csa.ref_volt / 1000.0f) * 0x0fff / 3.3f);

    gpio_set_value(&drv_md1, csa.md_val & 1);
    gpio_set_value(&drv_md2, csa.md_val & 2);
    gpio_set_value(&drv_md3, csa.md_val & 4);
    gpio_set_value(&drv_en, csa.state);

    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

void app_motor_routine(void)
{
    uint32_t flags;

    if (csa.set_home) {
        // retain microstep to improve repetition accuracy
        local_irq_save(flags);
        uint16_t micro = csa.cur_pos & ((4 << csa.md_val) - 1);
        csa.cur_pos = micro & (2 << csa.md_val) ? micro - (4 << csa.md_val) : micro;
        csa.tc_pos = 0;
        local_irq_restore(flags);
        d_debug("after set home cur_pos: %d\n", csa.cur_pos);
        csa.set_home = false;
        motor_w_hook(0, 0, NULL);
    }

    // disable motor driver reset microstep
    if (!csa.state && !csa.tc_state) {
        uint16_t micro = csa.cur_pos & ((4 << csa.md_val) - 1);
        int delta = micro & (2 << csa.md_val) ? micro - (4 << csa.md_val) : micro;
        csa.cur_pos -= delta;
    }

    if (!csa.tc_state)
        limit_disable = false;
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    //d_debug("tim: t: %ld, cur_pos: %d\n", get_systick(), csa.cur_pos);
    gpio_get_value(&drv_dir) ? csa.cur_pos++ :  csa.cur_pos--;
    gpio_set_value(&drv_step, 1);
    gpio_set_value(&drv_step, 0);

    // microstep accuracy checking
    bool mo_val = gpio_get_value(&drv_mo);
    if (mo_val == 0) {
        if ((csa.cur_pos & ((4 << csa.md_val) - 1)) != 0)
            d_warn("mo @%d\n", csa.cur_pos);
    }

    float v_stop = max((float)csa.tc_accel / (float)csa.tc_speed_min, (float)csa.tc_speed_min) * 1.2f;

    if (csa.cur_pos == csa.tc_pos && fabsf(csa.tc_vc) <= v_stop) {
        __HAL_TIM_DISABLE(&htim1);
        __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
        d_debug("tim: arrive pos %d, period: %d.%.2d\n",  csa.tc_pos,  P_2F(csa.tc_vc));
        csa.tc_state = 0;
        csa.tc_vc = 0;
        csa.tc_ac = 0;
        raw_dbg(0);
        csa.time_cnt += 10000;
        return;
    }

    int direction = sign(csa.tc_pos - csa.cur_pos);
    if (direction == 0) {
        direction = -sign(csa.tc_vc);
        csa.tc_state = 3;
    } else if (csa.tc_state == 3 && fabsf(csa.tc_vc) <= v_stop) {
        csa.tc_state = 1;
    }

    // slightly more than allowed, such as 1.2 times
    if (csa.tc_state != 3) {
        csa.tc_ac = ((/* tc_ve + */ csa.tc_vc) / 2.0f) * (/* tc_ve */ - csa.tc_vc) / (csa.tc_pos - csa.cur_pos);
        csa.tc_ac = min(fabsf(csa.tc_ac), (float)csa.tc_accel * 1.2f);
    } else {
        csa.tc_ac = csa.tc_accel * 2.0f; // reduce overrun
    }

    if (csa.tc_state == 1) {
        if (fabsf(csa.tc_ac) < csa.tc_accel) {
            float delta_v = csa.tc_accel / max((float)csa.tc_speed_min, fabsf(csa.tc_vc));
            csa.tc_vc += direction * delta_v;
            csa.tc_vc = clip(csa.tc_vc, -(float)csa.tc_speed, (float)csa.tc_speed);
        } else {
            csa.tc_state = 2;
        }
    }
    if (csa.tc_state != 1) {
        float delta_v = csa.tc_ac / max((float)csa.tc_speed_min, fabsf(csa.tc_vc));
        csa.tc_vc += -sign(csa.tc_vc) * delta_v;
    }

    if (fabsf(csa.tc_vc) < (float)csa.tc_speed_min)
        csa.tc_vc = direction * (float)csa.tc_speed_min;
    if (fabsf(csa.tc_vc) > 13800) // too quick may block main loop
        csa.tc_vc = sign(csa.tc_vc) * 13800;

    // tc_vc: step / sec; tim1 unit: 1 / 1000000Hz
    int tim_val = abs(lroundf(1000000 / csa.tc_vc));

    gpio_set_value(&drv_dir, csa.tc_vc >= 0); // 0: -, 1: +
    __HAL_TIM_SET_AUTORELOAD(&htim1, tim_val);

    raw_dbg(0);
    csa.time_cnt += tim_val;
}

void limit_det_isr(void)
{
    d_debug("lim: detected, %d\n", limit_disable);
    if (limit_disable)
        return;
    limit_disable = true;

    // It will go to different direction if tc_vc >= tc_speed_min.

    // To improve the repetition accuracy,
    // go to the closest microstep value triggered by the limit switch in the last calibration,
    // and then run the set_home command.
    if (!csa.lim_cali) {
        csa.lim_micro = csa.cur_pos & ((4 << csa.md_val) - 1);
        csa.lim_cali = true;
        csa.tc_pos = csa.cur_pos;

    } else {
        uint16_t micro = csa.cur_pos & ((4 << csa.md_val) - 1);
        micro = micro >= csa.lim_micro ? micro : micro + (4 << csa.md_val);
        int delta = micro - csa.lim_micro;
        delta = delta & (2 << csa.md_val) ? delta - (4 << csa.md_val) : delta;
        csa.tc_pos = csa.cur_pos - delta;
    }
}
