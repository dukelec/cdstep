/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2016, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/
 * Modified by: Duke Fong <duke@dukelec.com>
 */

#include "cd_utils.h"
#include "pid_i.h"
#include "app_main.h"

#define HIST_LEN 3
static int32_t hist[HIST_LEN] = { 0 };

float pid_i_compute(pid_i_t *pid, int input)
{
    int error, delta_input;
    float output;

    error = pid->target - input;
    pid->i_term += pid->_ki * error;

    float v_step = (float)csa.tc_accel / LOOP_FREQ;
    if (abs(csa.tc_pos - csa.cal_pos) < v_step * 100) { // reduce to out_max/10 and lower when close to end
        float i_lim = (float)(abs(csa.tc_pos - csa.cal_pos) + v_step) * (pid->out_max / (v_step * 1000));
        pid->i_term = clip(pid->i_term, -i_lim, i_lim);
    } else {
        pid->i_term = clip(pid->i_term, pid->out_min, pid->out_max);
    }

    float kp_term = pid->kp * error;

    delta_input = input - pid->last_input; // delta_input = -delta_error
    pid->last_input = input;

    for (int i = 0; i < HIST_LEN - 1; i++)
        hist[i] = hist[i + 1];
    hist[HIST_LEN - 1] = delta_input;

    float di_avg = 0;
    for (int i = 0; i < HIST_LEN; i++)
        di_avg += hist[i];
    di_avg = di_avg / (float)HIST_LEN;

    output = kp_term + pid->i_term - pid->_kd * di_avg;
    output = clip(output, pid->out_min, pid->out_max);
    return output;
}

float pid_i_compute_no_d(pid_i_t *pid, int input)
{
    int error;
    float output;

    error = pid->target - input;

    pid->i_term += pid->_ki * error;
    pid->i_term = clip(pid->i_term, pid->out_min, pid->out_max);
    pid->last_input = input;

    output = pid->kp * error + pid->i_term;
    output = clip(output, pid->out_min, pid->out_max);
    return output;
}


void pid_i_reset(pid_i_t *pid, int input, float output)
{
    pid->last_input = input;
    pid->i_term = clip(output, pid->out_min, pid->out_max);
    for (int i = 0; i < HIST_LEN; i++)
        hist[i] = 0;
}

void pid_i_init(pid_i_t *pid, bool reset)
{
    pid->_ki = pid->ki * pid->period;
    pid->_kd = pid->kd / pid->period;

    if (reset)
        pid_i_reset(pid, 0, 0);
}
