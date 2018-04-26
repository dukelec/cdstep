/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __APP_MAIN_H__
#define __APP_MAIN_H__

#include "common.h"
#include "cdnet.h"

typedef struct {
    uint16_t        magic_code; // 0xcdcd
    bool            stay_in_bl; // stay in bootloader

    cdnet_addr_t    rs485_addr;
    uint32_t        rs485_baudrate_low;
    uint32_t        rs485_baudrate_high;
} app_conf_t;

typedef enum {
    ST_OFF = 0,
    ST_STOP,
    ST_WAIT,
    ST_RUN
} state_t;

typedef struct {
    list_node_t node;

    int pos;
    int period;
    int accel;
    int time;
} cmd_t;

#define FLASH_PORT          10 // save to flash

extern app_conf_t app_conf;
extern cdnet_intf_t n_intf;

void p1_service(cdnet_packet_t *pkt);
void p2_service(cdnet_packet_t *pkt);
void p3_service(cdnet_packet_t *pkt);

#endif
