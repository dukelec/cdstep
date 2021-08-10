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


static void microstep_acc_chk(void)
{
    // microstep accuracy checking
    bool mo_val = gpio_get_value(&drv_mo);
    uint16_t sub_step = csa.cur_pos & ((4 << csa.md_val) - 1);
    if (!mo_val && sub_step)
        d_warn("mo0 @%d\n", csa.cur_pos);
    if (mo_val && !sub_step)
        d_warn("mo1 @%d\n", csa.cur_pos);
}

static void set_pwm(int value)
{
    if (value == 0) {
        if (!is_last_0) {
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
            __HAL_TIM_SET_AUTORELOAD(&htim3, 0);
            __HAL_TIM_SET_PRESCALER(&htim3, 4-1);
            gpio_set_value(&drv_dir, (value >= 0));
            __HAL_TIM_SET_COUNTER(&htim2, 0);
            pos_at_cnt0 = csa.cur_pos;
            microstep_acc_chk();
        }
        is_last_0 = true;
        return;
    }
    if (gpio_get_value(&drv_dir) != (value >= 0)) {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_AUTORELOAD(&htim3, 0);
        __HAL_TIM_SET_PRESCALER(&htim3, 4-1);
        gpio_set_value(&drv_dir, (value >= 0));
        __HAL_TIM_SET_COUNTER(&htim2, 0);
        pos_at_cnt0 = csa.cur_pos;
    }

    // 16: 250ns, auto-reload: 16*2-1
    value = clip(abs(value), 16*2-1, 65536*4-1);
    int div = value / 65536;
    value = value / (div + 1);
    __HAL_TIM_SET_PRESCALER(&htim3, div);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 16);
    __HAL_TIM_SET_AUTORELOAD(&htim3, value);
    htim3.Instance->EGR = TIM_EGR_UG;
    is_last_0 = false;
}

uint8_t motor_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat)
{
    uint32_t flags;
    static uint8_t last_csa_state = 0;
    gpio_set_value(&drv_en, csa.state);

    local_irq_save(flags);
    if (csa.state && csa.tc_state == 2)
        csa.tc_state = 1;

    if (csa.state && !csa.tc_state && csa.tc_pos != csa.cal_pos) {
        local_irq_restore(flags);

        d_debug("run motor ...\n");
        csa.tc_state = 1;

        //__HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
        //__HAL_TIM_ENABLE(&htim1);
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
    pid_i_init(&csa.pid_pos, true);

    __HAL_TIM_ENABLE(&htim1);
    __HAL_TIM_ENABLE(&htim2);

    d_info("init pwm...\n");
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_AUTORELOAD(&htim3, 0);
    __HAL_TIM_SET_PRESCALER(&htim3, 4-1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    set_pwm(0); // stop pwm

    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

void app_motor_routine(void)
{
    uint32_t flags;

    if (csa.set_home && is_last_0) {
        // retain microstep to improve repetition accuracy
        local_irq_save(flags);
        uint16_t micro = csa.cur_pos & ((4 << csa.md_val) - 1);
        csa.cur_pos = micro & (2 << csa.md_val) ? micro - (4 << csa.md_val) : micro;
        pos_at_cnt0 = csa.cur_pos;
        csa.tc_pos = 0;
        pid_i_reset(&csa.pid_pos, csa.cur_pos, 0);
        pid_i_set_target(&csa.pid_pos, csa.cur_pos);
        csa.cal_pos = csa.cur_pos;
        csa.cal_speed = 0;
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


static inline void t_curve_compute(void)
{
    static double p64f = (double)INFINITY;

    float v_step = (float)csa.tc_accel / LOOP_FREQ;
    float v_stop = v_step * 1.2f;

    if (!csa.tc_state) {
        csa.tc_vc = 0;
        csa.tc_ac = 0;
        p64f = (double)INFINITY;
        return;
    } else if (p64f == (double)INFINITY) {
        p64f = csa.cal_pos;
    }

    int direction = sign(csa.tc_pos - csa.cal_pos);
    if (direction == 0) {
        direction = -sign(csa.tc_vc);
        csa.tc_state = 3;
    } else if (csa.tc_state == 3 && fabsf(csa.tc_vc) <= v_stop) {
        csa.tc_state = 1;
    }

    // slightly more than allowed, such as 1.2 times
    if (csa.tc_state != 3) {
        csa.tc_ac = ((/* tc_ve + */ csa.tc_vc) / 2.0f) * (/* tc_ve */ - csa.tc_vc) / (csa.tc_pos - csa.cal_pos);
        csa.tc_ac = min(fabsf(csa.tc_ac), csa.tc_accel * 1.2f);

    } else {
        csa.tc_ac = csa.tc_accel * 2.0f; // reduce overrun
    }

    if (csa.tc_state == 1) {
        if (fabsf(csa.tc_ac) < csa.tc_accel) {
            csa.tc_vc += direction * v_step;
            csa.tc_vc = clip(csa.tc_vc, -(float)csa.tc_speed, (float)csa.tc_speed);
        } else {
            csa.tc_state = 2;
        }
    }
    if (csa.tc_state != 1) {
        float delta_v = csa.tc_ac / LOOP_FREQ;
        csa.tc_vc += -sign(csa.tc_vc) * delta_v;
    }

    float dt_pos = csa.tc_vc / LOOP_FREQ;
    if (fabsf(dt_pos) < min(v_step, 1.0f))
        dt_pos = direction * min(v_step, 1.0f);
    p64f += (double)dt_pos;
    int32_t p32i = lround(p64f);

    if (fabsf(csa.tc_vc) <= v_step * 4.4f) { // avoid overrun
        csa.cal_pos = (csa.tc_pos >= csa.cal_pos) ? min(p32i, csa.tc_pos) : max(p32i, csa.tc_pos);
        if (csa.cal_pos == csa.tc_pos) {
            csa.tc_state = 0;
            csa.tc_vc = 0;
            csa.tc_ac = 0;
        }
    } else {
        csa.cal_pos = p32i;
    }

}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (!csa.state) {
        pid_i_reset(&csa.pid_pos, csa.cur_pos, 0);
        pid_i_set_target(&csa.pid_pos, csa.cur_pos);
        csa.cal_pos = csa.cur_pos;
        csa.cal_speed = 0;

    } else {
        int counter_dir = gpio_get_value(&drv_dir) ? 1 : -1;
        csa.cur_pos = pos_at_cnt0 + __HAL_TIM_GET_COUNTER(&htim2) * counter_dir;

        t_curve_compute();
        pid_i_set_target(&csa.pid_pos, csa.cal_pos);
        csa.cal_speed = pid_i_compute(&csa.pid_pos, csa.cur_pos);

        if (fabsf(csa.cal_speed) <= max((float)csa.tc_accel / LOOP_FREQ, 50)) {
            set_pwm(0);
        } else {
            // tc_vc: step / sec; tim3 unit: 1 / 64000000Hz
            int tim_val = lroundf(64000000 / csa.cal_speed);
            set_pwm(tim_val);
        }
    }

    raw_dbg(0);
    raw_dbg(1);
    csa.loop_cnt++;
}

void limit_det_isr(void)
{
    d_debug("lim: detected, %d\n", limit_disable);
    if (limit_disable)
        return;
    limit_disable = true;

    // It will go to different direction if tc_vc >= min speed.

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
