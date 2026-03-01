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

static inline void csa2trap_planner(trap_planner_t *tp, bool emg)
{
    memcpy(&tp->pos_tgt, &csa.tp_pos, 4 * 3);
    if (emg)
        tp->acc_tgt = csa.tp_accel_emg;
}

static inline void trap_planner2csa_rst(trap_planner_t *tp)
{
    //csa.tp_pos = tp->pos_tgt;

    csa.tp_state = tp->state;
    csa.tp_vel_out = tp->vel_out;
    csa.tp_acc_brake = tp->acc_brake;
}

static inline void trap_planner2csa(trap_planner_t *tp)
{
    csa.cal_pos = tp->pos_out;

    csa.tp_state = tp->state;
    csa.tp_vel_out = tp->vel_out;
    csa.tp_acc_brake = tp->acc_brake;
}

#endif
