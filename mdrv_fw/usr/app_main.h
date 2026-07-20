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

#include "cdnet_core.h"
#include "cd_debug.h"
#include "cdbus_uart.h"
#include "cdctl_it.h"
#include "step_pwm.h"
#include "pid_i.h"
#include "trap_planner.h"

#define P_2F(x) (int)(x), abs(((x)-(int)(x))*100)  // "%d.%.2d"
#define P_3F(x) (int)(x), abs(((x)-(int)(x))*1000) // "%d.%.3d"


#define BL_ARGS             0x20000000 // first word
#define APP_CONF_ADDR       0x0801f800 // page 63, the last page
#define APP_CONF_VER        0x0300
#define CSA_SET_HOME        0x03fe
#define CSA_EXT_ST          0x0400

#define FRAME_MAX           80
#define PACKET_MAX          80
#define CDN_MAX_PAYLOAD     251

#define LOOP_FREQ   (64000000 / 64 / 200) // 5 KHz

typedef enum {
    ST_STOP = 0,
    ST_VOLTAGE,
    ST_CURRENT,
    ST_SPEED,
    ST_POSITION,
    ST_POS_TP
} state_t;


typedef struct {
    uint16_t        offset;
    uint8_t         size;
} regr_t; // reg range

typedef struct {
    uint16_t        offset;
    uint16_t        size;
} reg2r_t; // reg range


typedef struct {
    uint16_t        magic_code;     // 0xcdcd
    uint16_t        conf_ver;
    uint8_t         conf_from;      // 0: default, 1: all from flash, 2: partly from flash
    uint8_t         do_reboot;      // 1: reboot to bl, 2: reboot to app
    bool            keep_bl;
    bool            save_conf;
    uint8_t         _reserved0[7];

    uint8_t         mac;
    uint32_t        baud_rate_l;
    uint32_t        baud_rate_h;
    uint8_t         bus_filter_m[2];
    uint8_t         _reserved01[2];
    uint8_t         bus_mode;
    uint8_t         bus_idle_wait_len;
    uint16_t        bus_tx_permit_len;
    uint16_t        bus_max_idle_len;
    uint8_t         bus_tx_pre_len;
    uint8_t         _reserved1[13];

    uint8_t         dbg_en;
    #define         _end_common dbg_raw_en
    uint8_t         dbg_raw_en;
    regr_t          dbg_raw[6];
    uint8_t         _reserved2[22];

    regr_t          qxchg_mcast;     // for multicast
    regr_t          qxchg_set[4];
    regr_t          qxchg_ret[5];
    uint8_t         _reserved3[24];

    uint8_t         _reserved4[48];
    uint8_t         _reserved5[32];
    uint8_t         _reserved6[48];
    uint8_t         _reserved7[48];

    int32_t         tp_pos;
    uint32_t        tp_speed;
    uint32_t        tp_accel;
    uint32_t        tp_accel_emg;
    uint32_t        tp_max_err;
    uint8_t         _reserved8[27];
    int8_t          tp_state;       // -1: disable, 0: idle, 1: planning
    int32_t         tp_vel_out;
    uint8_t         _reserved9[44];

    float           pid_pos_kp;
    uint8_t         _reserved10[8];
    int32_t         pid_pos_out_min;
    int32_t         pid_pos_out_max;

    uint8_t         _reserved11[204];
    uint16_t        ref_volt;
    uint8_t         md_val;
    bool            lim_en;

    #define         _end_save _reserved16
    uint8_t         _reserved16[2];
    uint8_t         set_home;
    uint8_t         _reserved160;

    uint8_t         state;          // 0: stop, 5: position with trap planner
    uint8_t         _reserved17[63];

    int32_t         tgt_pos;        // planned position, position-loop target
    int32_t         tgt_speed;      // position-loop output speed
    uint8_t         _reserved18[76];

    int32_t         meas_pos;
    uint8_t         _reserved19[72];

    uint32_t        loop_cnt;
    uint8_t         _reserved20;
    bool            drv_mo;         // mo pin state of drv chip
    char            string_test[10]; // for cdbus_gui tool test

} csa_t; // config status area

#define CSA_OFS_ST          (CSA_EXT_ST - offsetof(csa_t, state))
#define CSA_OFS(_x)         (offsetof(csa_t, _x) + \
        (offsetof(csa_t, _x) >= offsetof(csa_t, set_home) ? CSA_OFS_ST : 0))

_Static_assert(offsetof(csa_t, mac) == 0x000f, "CSA mac offset");
_Static_assert(offsetof(csa_t, baud_rate_l) == 0x0010, "CSA baud_rate offset");
_Static_assert(offsetof(csa_t, bus_mode) == 0x001c, "CSA bus_mode offset");
_Static_assert(offsetof(csa_t, dbg_en) == 0x0030, "CSA dbg_en offset");
_Static_assert(offsetof(csa_t, dbg_raw) == 0x0032, "CSA dbg_raw offset");
_Static_assert(offsetof(csa_t, qxchg_mcast) == 0x0060, "CSA qxchg offset");
_Static_assert(offsetof(csa_t, qxchg_set) == 0x0064, "CSA qxchg_set offset");
_Static_assert(offsetof(csa_t, qxchg_ret) == 0x0074, "CSA qxchg_ret offset");
_Static_assert(offsetof(csa_t, tp_pos) == 0x0150, "CSA tp_pos offset");
_Static_assert(offsetof(csa_t, tp_speed) == 0x0154, "CSA tp_speed offset");
_Static_assert(offsetof(csa_t, tp_accel) == 0x0158, "CSA tp_accel offset");
_Static_assert(offsetof(csa_t, tp_accel_emg) == 0x015c, "CSA tp_accel_emg offset");
_Static_assert(offsetof(csa_t, tp_max_err) == 0x0160, "CSA tp_max_err offset");
_Static_assert(offsetof(csa_t, tp_state) == 0x017f, "CSA tp_state offset");
_Static_assert(offsetof(csa_t, tp_vel_out) == 0x0180, "CSA tp_vel_out offset");
_Static_assert(offsetof(csa_t, pid_pos_kp) == 0x01b0, "CSA pid_pos offset");
_Static_assert(offsetof(csa_t, pid_pos_out_min) == 0x01bc, "CSA pid_pos_out_min offset");
_Static_assert(offsetof(csa_t, pid_pos_out_max) == 0x01c0, "CSA pid_pos_out_max offset");
_Static_assert(offsetof(csa_t, ref_volt) == 0x0290, "CSA ref_volt offset");
_Static_assert(offsetof(csa_t, md_val) == 0x0292, "CSA md_val offset");
_Static_assert(offsetof(csa_t, lim_en) == 0x0293, "CSA lim_en offset");
_Static_assert(offsetof(csa_t, _end_save) == 0x0294, "CSA saved config end");
_Static_assert(offsetof(csa_t, set_home) == 0x0296, "CSA internal set_home offset");
_Static_assert(CSA_OFS(set_home) == 0x03fe, "CSA set_home offset");
_Static_assert(offsetof(csa_t, state) == 0x0298, "CSA internal state offset");
_Static_assert(CSA_OFS_ST == 0x0168, "CSA extension offset");
_Static_assert(CSA_OFS(state) == 0x0400, "CSA state offset");
_Static_assert(CSA_OFS(tgt_pos) == 0x0440, "CSA target offset");
_Static_assert(CSA_OFS(tgt_speed) == 0x0444, "CSA tgt_speed offset");
_Static_assert(CSA_OFS(meas_pos) == 0x0494, "CSA meas_pos offset");
_Static_assert(CSA_OFS(loop_cnt) == 0x04e0, "CSA loop_cnt offset");

static inline bool csa_range_to_internal(uint16_t protocol_offset, uint16_t size, uint16_t *internal_offset)
{
    uint32_t offset = protocol_offset;
    if (offset >= CSA_SET_HOME) {
        offset -= CSA_OFS_ST;
    } else if (offset + size > offsetof(csa_t, _end_save)) {
        return false;
    }

    if (offset + size > sizeof(csa_t))
        return false;
    *internal_offset = offset;
    return true;
}


typedef uint8_t (*hook_func_t)(uint16_t sub_offset, uint8_t len, uint8_t *dat);

typedef struct {
    reg2r_t         range;
    hook_func_t     before;
    hook_func_t     after;
} csa_hook_t;


extern csa_t csa;
extern const csa_t csa_dft;

extern reg2r_t csa_w_allow[]; // writable list
extern int csa_w_allow_num;

extern csa_hook_t csa_w_hook[];
extern int csa_w_hook_num;
extern csa_hook_t csa_r_hook[];
extern int csa_r_hook_num;

extern uint32_t end; // end of bss

int flash_erase(uint32_t addr, uint32_t len);
int flash_write(uint32_t addr, uint32_t len, const uint8_t *buf);

void app_main(void);
void load_conf(void);
int save_conf(void);
void csa_list_show(void);

void comm_service_init(void);
void comm_service_poll(void);

uint8_t motor_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat);
uint8_t ref_volt_w_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat);
uint8_t drv_mo_r_hook(uint16_t sub_offset, uint8_t len, uint8_t *dat);
void app_motor_maintain(void);
void app_motor_init(void);
void raw_dbg(int idx);
void limit_det_isr(void);
void timer_isr(void);

extern gpio_t led_r;
extern gpio_t led_g;
extern cdn_ns_t dft_ns;
extern list_head_t frame_free_head;
extern cdctl_dev_t r_dev;

#include "csa_sync.h"
#endif
