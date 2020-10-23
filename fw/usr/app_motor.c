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

static gpio_t drv_en = { .group = DRV_EN_GPIO_Port, .num = DRV_EN_Pin };
static gpio_t drv_md1 = { .group = DRV_MD1_GPIO_Port, .num = DRV_MD1_Pin };
static gpio_t drv_md2 = { .group = DRV_MD2_GPIO_Port, .num = DRV_MD2_Pin };
static gpio_t drv_md3 = { .group = DRV_MD3_GPIO_Port, .num = DRV_MD3_Pin };
static gpio_t drv_step = { .group = DRV_STEP_GPIO_Port, .num = DRV_STEP_Pin };
static gpio_t drv_dir = { .group = DRV_DIR_GPIO_Port, .num = DRV_DIR_Pin };

gpio_t limit_det = { .group = LIMIT_DET_GPIO_Port, .num = LIMIT_DET_Pin };


void app_motor_init(void)
{
    //HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    set_led_state(LED_POWERON);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 450);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);

    // 200 steps per 360 'C
    gpio_set_value(&drv_md1, 0);
    gpio_set_value(&drv_md2, 0);
    gpio_set_value(&drv_md3, 1);
    gpio_set_value(&drv_en, csa.state);
}

void app_motor(void)
{
    gpio_set_value(&drv_en, csa.state);

    if (csa.state && !csa.tc_state && csa.tc_pos != csa.cur_pos) {
        int tim_val = abs(lroundf(1000000.0f / csa.tc_speed_min));

        gpio_set_value(&drv_dir, csa.tc_pos >= csa.cur_pos); // 0: -, 1: +
        csa.tc_state = 1;

        __HAL_TIM_SET_AUTORELOAD(&htim1, tim_val);
        __HAL_TIM_ENABLE(&htim1);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    //d_debug("tim: t: %ld, cur_pos: %d, cur_period: %d\n", get_systick(), cur_pos, cur_period);
    gpio_get_value(&drv_dir) ? csa.cur_pos++ :  csa.cur_pos--;
    gpio_set_value(&drv_step, 1);
    gpio_set_value(&drv_step, 0);

    if (csa.cur_pos == csa.tc_pos && fabsf(csa.tc_vc) < csa.tc_speed_min) {
        d_debug("tim: arrive pos %d, period: %d.%.2d\n",  csa.tc_pos,  P_2F(csa.tc_vc));
        csa.tc_state = 0;
        csa.tc_vc = 0;
        csa.tc_ac = 0;
        csa.time_cnt += 10000;
        return;
    }

    if (csa.tc_pos != csa.cur_pos && csa.tc_vc >= csa.tc_speed_min
            && sign(csa.tc_pos - csa.cur_pos) != sign(csa.tc_vc)) { // different direction

        csa.tc_ac = sign(csa.tc_pos - csa.cur_pos) * min(csa.tc_accel, fabsf(csa.tc_vc));
        csa.tc_state = 1;

    } else {
        csa.tc_ac = sign(csa.tc_pos - csa.cur_pos) * csa.tc_accel_max;

        if (csa.tc_pos != csa.cur_pos)
            csa.tc_ac = ((/* tc_ve + */ csa.tc_vc) / 2.0f) * (/* tc_ve */ - csa.tc_vc) / (csa.tc_pos - csa.cur_pos);

        csa.tc_ac = sign(csa.tc_ac) * min(fabsf(csa.tc_ac), csa.tc_accel_max);
    }

    if (fabsf(csa.tc_ac) <= csa.tc_accel) {
        if (csa.tc_state == 1) {
            float delta_v = csa.tc_accel / max(csa.tc_speed_min, csa.tc_vc);
            csa.tc_vc += sign(csa.tc_pos - csa.cur_pos) * delta_v;
            csa.tc_vc = clip(csa.tc_vc, -csa.tc_speed, csa.tc_speed);
        }
    } else {
        float delta_v = csa.tc_ac / max(csa.tc_speed_min, csa.tc_vc);
        csa.tc_vc += delta_v;
        csa.tc_state = 2;
    }

    if (fabsf(csa.tc_vc) < csa.tc_speed_min)
        csa.tc_vc = sign(csa.tc_pos - csa.cur_pos) * csa.tc_speed_min;

    // tc_vc: step / sec; tim1 unit: 1 / 1000000Hz
    int tim_val = abs(lroundf(1000000 / csa.tc_vc));

    gpio_set_value(&drv_dir, csa.tc_vc >= 0); // 0: -, 1: +
    __HAL_TIM_SET_AUTORELOAD(&htim1, tim_val);
    __HAL_TIM_ENABLE(&htim1);

    raw_dbg(0);
    csa.time_cnt += tim_val;
}

void limit_det_isr(void)
{
    d_debug("lim: detected\n");

    // will go different direction if tc_vc >= tc_speed_min:
    csa.cur_pos = 0;
    csa.tc_pos = 0;
}
