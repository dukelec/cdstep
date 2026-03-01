/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __STEP_PWM_H__
#define __STEP_PWM_H__

#define STEP_PWM_PULSE          16                      // 250ns @ 64MHz
#define STEP_PWM_PERIOD_MIN     (STEP_PWM_PULSE * 2)
#define STEP_PWM_PERIOD_MAX     (65536 * 4)             // 4ms @ 64MHz

typedef struct {
    gpio_t              *dir_pin;
    TIM_HandleTypeDef   *tim_cnt;
    TIM_HandleTypeDef   *tim_pwm;
    uint32_t            tim_pwm_ch;

    uint16_t            tim_cnt_bk;
    int32_t             base_pos;

    bool                last_is_0;

    // add time space before and after dir_pin switch
    bool                dir_chg_wait_head;
    bool                dir_chg_wait_tail;
} step_pwm_t;


void step_pwm_set(step_pwm_t *sp, int period);

int step_pwm_get_cnt(step_pwm_t *sp);
void step_pwm_clr_cnt(step_pwm_t *sp);

void step_pwm_init(step_pwm_t *sp);

#endif
