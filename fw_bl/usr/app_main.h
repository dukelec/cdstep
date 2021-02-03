/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __APP_MAIN_H__
#define __APP_MAIN_H__

#include "cdnet_dispatch.h"
#include "cd_debug.h"
#include "cdbus_uart.h"
#include "cdctl.h"

#define P_2F(x) (int)(x), abs(((x)-(int)(x))*100)  // "%d.%.2d"
#define P_3F(x) (int)(x), abs(((x)-(int)(x))*1000) // "%d.%.3d"

#define APP_CONF_ADDR       0x0800FC00 // last 1k page
#define APP_CONF_VER        0x0001

#define FRAME_MAX           10
#define PACKET_MAX          30


typedef struct {
    uint16_t        magic_code; // 0xcdcd
    uint16_t        conf_ver;
    bool            conf_from;  // 0: default, 1: load from flash
    bool            do_reboot;
    bool            keep_in_bl;
    bool            _reserved;

    //uint8_t       bus_mode;
    uint8_t         bus_net;
    uint8_t         bus_mac;
    uint32_t        bus_baud_low;
    uint32_t        bus_baud_high;
    //uint16_t      bus_tx_premit_len;
    //uint16_t      bus_max_idle_len;

    bool            dbg_en;
    cdn_sockaddr_t  dbg_dst;

} csa_t; // config status area

extern csa_t csa;
extern const csa_t csa_dft;

extern cdn_ns_t dft_ns;

void app_main(void);
void load_conf(void);
void common_service_init(void);
void common_service_routine(void);

#endif
