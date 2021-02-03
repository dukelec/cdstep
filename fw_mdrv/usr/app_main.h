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

#define APP_CONF_ADDR       0x0800FC00 // last 1k page
#define APP_CONF_VER        0x0001

#define FRAME_MAX           10
#define PACKET_MAX          30


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

    //uint8_t       bus_mode;
    uint8_t         bus_net;
    uint8_t         bus_mac;
    uint32_t        bus_baud_low;
    uint32_t        bus_baud_high;
    //uint16_t      bus_tx_premit_len;
    //uint16_t      bus_max_idle_len;

    bool            dbg_en;
    cdn_sockaddr_t  dbg_dst;

    regr_t          qxchg_mcast;     // for multicast
    regr_t          qxchg_set[5];
    regr_t          qxchg_ret[5];
    regr_t          qxchg_ro[5];

    //uint8_t       dbg_str_msk;
    //uint16_t      dbg_str_skip;    // for period print debug

    cdn_sockaddr_t  dbg_raw_dst;
    uint8_t         dbg_raw_msk;
    uint8_t         dbg_raw_th;      // len threshold (+ 1 samples < pkt size)
    regr_t          dbg_raw[2][6];

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
    char            string_test[10]; // for cdbus_gui tool test

} csa_t; // config status area


typedef uint8_t (*hook_func_t)(uint16_t sub_offset, uint8_t len, uint8_t *dat);

typedef struct {
    regr_t          range;
    hook_func_t     before;
    hook_func_t     after;
} csa_hook_t;


extern csa_t csa;
extern const csa_t csa_dft;

extern regr_t csa_w_allow[]; // writable list
extern int csa_w_allow_num;

extern csa_hook_t csa_w_hook[];
extern int csa_w_hook_num;
extern csa_hook_t csa_r_hook[];
extern int csa_r_hook_num;

extern list_head_t frame_free_head;
extern cdn_ns_t dft_ns;


void app_main(void);
void load_conf(void);
int save_conf(void);
void csa_list_show(void);
void common_service_init(void);
void common_service_routine(void);

void set_led_state(led_state_t state);

uint8_t motor_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat);
void app_motor_init(void);
void raw_dbg(int idx);
void raw_dbg_init(void);
void raw_dbg_routine(void);
void limit_det_isr(void);

#endif
