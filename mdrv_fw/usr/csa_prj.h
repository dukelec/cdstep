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

// cdstep-r: the r axis of cdpnp, carries the nozzle force sensor (24-bit serial
// adc on SEN_SCK / SEN_SDO) and reports it to the z axis, see read_force in
// app_main.c.
//
// saved config, registers 0x0300..:
//   0x0300  u16    prj_conf_ver
//   0x0304  u8     force_rpt_en
//   0x0306  u8[3]  force_rpt_dst.addr
//   0x030a  u16    force_rpt_dst.port

#define CSA_PRJ_CONF_EN
#define CSA_PRJ_CONF_VER    1

typedef struct {
    bool            force_rpt_en;   // report the force (the average of 8 samples) to force_rpt_dst
    uint8_t         _reserved0;
    cdn_sockaddr_t  force_rpt_dst;  // packet: [0x40, i32 force]
} csa_prj_conf_t;

#define CSA_PRJ_CONF_DFT { \
    .force_rpt_en = false, \
    .force_rpt_dst = { .addr = { 0x00, 0x00, 0xe1 }, .port = 0x0b } \
}

#endif
