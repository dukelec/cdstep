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
#include "cdctl_it.h"

#define P_2F(x) (int)(x), abs(((x)-(int)(x))*100)  // "%d.%.2d"
#define P_3F(x) (int)(x), abs(((x)-(int)(x))*1000) // "%d.%.3d"

#define APP_CONF_ADDR       0x0801F800 // last page
#define APP_CONF_VER        0x0001

#define FRAME_MAX           10
#define PACKET_MAX          10


typedef enum {
    LED_POWERON = 0,
    LED_WARN,
    LED_ERROR
} led_state_t;

typedef struct {
    uint16_t        offset;
    uint16_t        size;
} regr_t; // reg range


typedef struct {
    uint16_t        magic_code; // 0xcdcd
    uint16_t        conf_ver;
    bool            conf_from;  // 0: default, 1: load from flash
    bool            do_reboot;
    bool            _reserved;  // keep_in_bl for bl
    bool            save_conf;

    //uint8_t       bus_mode; // a, bs, trad
    uint8_t         bus_net;
    uint8_t         bus_mac;
    uint32_t        bus_baud_low;
    uint32_t        bus_baud_high;
    //uint16_t      bus_tx_premit_len;
    //uint16_t      bus_max_idle_len;

    bool            dbg_en;
    cdn_sockaddr_t  dbg_dst;

    regr_t          qxchg_set[10];
    regr_t          qxchg_ret[10];
    regr_t          qxchg_ro[10];

    //uint8_t       dbg_str_msk;
    //uint16_t      dbg_str_skip;   // for period string debug

    cdn_sockaddr_t  dbg_raw_dst;
    uint8_t         dbg_raw_msk;
    uint8_t         dbg_raw_th;      // len threshold (+ 1 samples < pkt size)
    uint8_t         dbg_raw_skip[2]; // take samples every few times
    regr_t          dbg_raw[2][10];

    int32_t         tc_pos;
    uint32_t        tc_speed;
    uint32_t        tc_accel;
    uint32_t        tc_speed_min;

    // end of flash

    uint8_t         state;      // 0: drv not enable, 1: drv enable
    uint8_t         tc_state;   // t_curve: 0: stop, 1: run, 2: tailer
    int             cur_pos;
    float           tc_vc;
    float           tc_ac;

    uint32_t        time_cnt;

} csa_t; // config status area

extern csa_t csa;
extern regr_t regr_wa[]; // writable list
extern int regr_wa_num;

extern list_head_t frame_free_head;
extern cdn_ns_t dft_ns;


void app_main(void);
void load_conf(void);
int save_conf(void);
void csa_list_show(void);
void common_service_init(void);
void common_service_routine(void);

void set_led_state(led_state_t state);

void app_motor(void);
void app_motor_init(void);
void raw_dbg(int idx);
void raw_dbg_init(void);
void raw_dbg_routine(void);
void limit_det_isr(void);

#endif
