/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CSA_SYNC_H__
#define __CSA_SYNC_H__


// trap_planner

static inline void csa2pid_pos(pid_i_t *pid)
{
    pid->kp = csa.pid_pos_kp;
    pid->out_min = csa.pid_pos_out_min;
    pid->out_max = csa.pid_pos_out_max;
}

static inline void csa2trap_planner_mt(trap_planner_t *tp)
{
    tp->max_err = csa.tp_max_err;
}

static inline void csa2trap_planner(trap_planner_t *tp, bool emg)
{
    tp->pos_tgt = csa.tp_pos;
    tp->vel_tgt = csa.tp_speed;
    tp->acc_tgt = emg ? csa.tp_accel_emg : csa.tp_accel;
}

static inline void trap_planner2csa_rst(trap_planner_t *tp)
{
    //csa.tp_pos = tp->pos_tgt;

    csa.tp_state = tp->state;
    csa.tp_vel_out = lroundf(tp->vel_out);
}

static inline void trap_planner2csa(trap_planner_t *tp)
{
    csa.tgt_pos = tp->pos_out;

    csa.tp_state = tp->state;
    csa.tp_vel_out = lroundf(tp->vel_out);
}

#endif
