/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CSA_PRJ_H__
#define __CSA_PRJ_H__

// cdstep-z: the z axis of cdpnp. The force readings come from the r axis
// (cdstep-r) over cdnet port 0x0b, see force_rx in app_main.c; the thresholds
// here are in units of 100 raw sensor counts.
//
// saved config, registers 0x0300..:
//   0x0300  u16  prj_conf_ver
//   0x0304  u16  force_protection
//   0x0306  u16  force_threshold
// status, registers 0x0500..:
//   0x0500  u8   force_trigger_en

#define CSA_PRJ_CONF_EN
#define CSA_PRJ_CONF_VER    1

typedef struct {
    uint16_t    force_protection;   // x100, the force fell this far below the first move's baseline:
                                    // retreat to position 0 with tp_accel_emg, limit switch ignored
    uint16_t    force_threshold;    // x100, armed by force_trigger_en, the force fell this far below
                                    // the baseline taken at move start: stop in place
} csa_prj_conf_t;

#define CSA_PRJ_CONF_DFT { \
    .force_protection = 700, \
    .force_threshold = 100 \
}

#define CSA_PRJ_STAT_EN

typedef struct {
    bool        force_trigger_en;   // write 1 to arm the force trigger for the next move,
                                    // cleared when it fires or when the move arrives
} csa_prj_stat_t;

#endif
