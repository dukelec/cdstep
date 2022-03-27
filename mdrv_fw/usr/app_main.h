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
#include "pid_i.h"

#define P_2F(x) (int)(x), abs(((x)-(int)(x))*100)  // "%d.%.2d"
#define P_3F(x) (int)(x), abs(((x)-(int)(x))*1000) // "%d.%.3d"


#define APP_CONF_ADDR       0x0801f800 // page 63, the last page
#define APP_CONF_VER        0x0106

#define FRAME_MAX           10
#define PACKET_MAX          60

#define LOOP_FREQ   (64000000 / 64 / 200) // 5 KHz


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
    uint16_t        magic_code;     // 0xcdcd
    uint16_t        conf_ver;
    uint8_t         conf_from;      // 0: default, 1: all from flash, 2: partly from flash
    bool            do_reboot;
    bool            _reserved_bl;   // keep_in_bl for bl
    bool            save_conf;

    uint8_t         bus_net;
    cdctl_cfg_t     bus_cfg;
    bool            dbg_en;
    cdn_sockaddr_t  dbg_dst;
    #define         _end_common qxchg_mcast

    regr_t          qxchg_mcast;     // for multicast
    regr_t          qxchg_set[5];
    regr_t          qxchg_ret[5];
    regr_t          qxchg_ro[5];

    uint8_t         _reserved1[10];
    //uint8_t       dbg_str_msk;
    //uint16_t      dbg_str_skip;    // for period print debug

    cdn_sockaddr_t  dbg_raw_dst;
    uint8_t         dbg_raw_msk;
    uint8_t         dbg_raw_th;      // len threshold (+ 1 samples < pkt size)
    regr_t          dbg_raw[2][6];

    uint16_t        ref_volt;
    uint8_t         md_val;
    bool            set_home;

    uint16_t        lim_micro;
    bool            lim_cali;
    uint8_t         _reserved2[6];

    int32_t         tc_pos;
    uint32_t        tc_speed;
    uint32_t        tc_accel;
    uint8_t         _reserved3[10];

    pid_i_t         pid_pos;
    #define         _end_save cal_pos   // end of flash
    int32_t         cal_pos;
    float           cal_speed;

    uint8_t         state;          // 0: drv not enable, 1: drv enable
    uint8_t         tc_state;       // t_curve: 0: stop, 1: run
    int             cur_pos;
    float           tc_vc;
    float           tc_ac;
    uint8_t         _reserved4[10];

    uint32_t        loop_cnt;
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

int flash_erase(uint32_t addr, uint32_t len);
int flash_write(uint32_t addr, uint32_t len, const uint8_t *buf);

void app_main(void);
void load_conf(void);
int save_conf(void);
void csa_list_show(void);

void common_service_init(void);
void common_service_routine(void);

uint8_t motor_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat);
uint8_t ref_volt_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat);
void app_motor_routine(void);
void app_motor_init(void);
void raw_dbg(int idx);
void raw_dbg_init(void);
void raw_dbg_routine(void);
void limit_det_isr(void);

extern gpio_t led_r;
extern gpio_t led_g;
extern cdn_ns_t dft_ns;
extern list_head_t frame_free_head;

#endif
