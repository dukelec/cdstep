/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"
#include "step_pwm.h"


int step_pwm_get_cnt(step_pwm_t *sp)
{
    int counter_dir = gpio_get_val(sp->dir_pin) ? 1 : -1;
    uint16_t tmr_val = __HAL_TIM_GET_COUNTER(sp->tim_cnt);

    if (tmr_val < sp->tim_cnt_bk)
        sp->base_pos += 0x10000 * counter_dir;
    sp->tim_cnt_bk = tmr_val;
    return sp->base_pos + tmr_val * counter_dir;
}


void step_pwm_clr_cnt(step_pwm_t *sp)
{
    //if (!sp->last_is_0)
    //    step_pwm_set(sp, 0);
    sp->tim_cnt_bk = 0;
    sp->base_pos = 0;
    __HAL_TIM_SET_COUNTER(sp->tim_cnt, 0);
    sp->dir_chg_wait_head = false;
    sp->dir_chg_wait_tail = false;
}


void step_pwm_set(step_pwm_t *sp, int period)
{
    if (sp->dir_chg_wait_head) {
        gpio_set_val(sp->dir_pin, (period >= 0));
        sp->dir_chg_wait_head = false;
        sp->dir_chg_wait_tail = true;
        return;
    }
    if (sp->dir_chg_wait_tail) {
        sp->dir_chg_wait_tail = false;
        return;
    }

    if (period == 0 || gpio_get_val(sp->dir_pin) != (period >= 0)) {
        if (!sp->last_is_0) {
            __HAL_TIM_SET_COMPARE(sp->tim_pwm, sp->tim_pwm_ch, 0); // pause pwm
            __HAL_TIM_SET_AUTORELOAD(sp->tim_pwm, 65535);
            __HAL_TIM_SET_PRESCALER(sp->tim_pwm, 4-1);
            // after pausing pwm the counter must be read again and then clear
            sp->base_pos = step_pwm_get_cnt(sp);
            __HAL_TIM_SET_COUNTER(sp->tim_cnt, 0);
            sp->tim_cnt_bk = 0;
        }
        sp->last_is_0 = true;
        sp->dir_chg_wait_head = true;
        return;
    }

    period = clip(abs(period), STEP_PWM_PERIOD_MIN - 1, STEP_PWM_PERIOD_MAX - 1);
    int div = period / 65536;
    period = period / (div + 1);
    __HAL_TIM_SET_PRESCALER(sp->tim_pwm, div);
    __HAL_TIM_SET_AUTORELOAD(sp->tim_pwm, period);
    __HAL_TIM_SET_COMPARE(sp->tim_pwm, sp->tim_pwm_ch, STEP_PWM_PULSE);
    sp->last_is_0 = false;
}


void step_pwm_init(step_pwm_t *sp)
{
    __HAL_TIM_ENABLE(sp->tim_cnt);

    //__HAL_TIM_SET_COMPARE(sp->tim_pwm, sp->tim_pwm_ch, 16);
    //__HAL_TIM_SET_AUTORELOAD(sp->tim_pwm, 65535);
    //__HAL_TIM_SET_PRESCALER(sp->tim_pwm, 4-1);
    HAL_TIM_PWM_Start(sp->tim_pwm, sp->tim_pwm_ch);
    __HAL_TIM_SET_COMPARE(sp->tim_pwm, sp->tim_pwm_ch, 0); // pause pwm
    step_pwm_clr_cnt(sp);
    step_pwm_set(sp, 0); // init flags
}

