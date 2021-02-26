/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"

regr_t csa_w_allow[] = {
        { .offset = offsetof(csa_t, magic_code), .size = offsetof(csa_t, tc_state) - offsetof(csa_t, magic_code) },
        { .offset = offsetof(csa_t, string_test), .size = 10 }
};

csa_hook_t csa_w_hook[] = {
        {
            .range = { .offset = offsetof(csa_t, tc_pos), .size = offsetof(csa_t, tc_state) - offsetof(csa_t, tc_pos) },
            .after = motor_w_hook
        }
};

csa_hook_t csa_r_hook[] = {};

int csa_w_allow_num = sizeof(csa_w_allow) / sizeof(regr_t);
int csa_w_hook_num = sizeof(csa_w_hook) / sizeof(csa_hook_t);
int csa_r_hook_num = sizeof(csa_r_hook) / sizeof(csa_hook_t);


const csa_t csa_dft = {
        .magic_code = 0xcdcd,
        .conf_ver = APP_CONF_VER,

        .bus_net = 0,
        .bus_cfg = CDCTL_CFG_DFT(0xfe),
        .dbg_en = false,
        .dbg_dst = { .addr = {0x80, 0x00, 0x00}, .port = 9 },

        .qxchg_set = {
                { .offset = offsetof(csa_t, tc_pos), .size = 4 * 3 }
        },
        .qxchg_ret = {
                { .offset = offsetof(csa_t, cur_pos), .size = 4 * 2 }
        },

        .dbg_raw_dst = { .addr = {0x80, 0x00, 0x00}, .port = 0xa },
        .dbg_raw_msk = 0,
        .dbg_raw_th = 200,
        .dbg_raw = {
                {
                        { .offset = offsetof(csa_t, time_cnt), .size = 4 },

                        { .offset = offsetof(csa_t, tc_pos), .size = 4 },
                        { .offset = offsetof(csa_t, tc_speed), .size = 4 },
                        { .offset = offsetof(csa_t, tc_state), .size = 1 },
                        { .offset = offsetof(csa_t, cur_pos), .size = 4 * 2 } // + tc_vc
                }
        },

        .tc_speed = 10000, // max speed is about 200000
        .tc_accel = 5000,
        .tc_speed_min = 100,

        .string_test = "hello"
};

csa_t csa;


void load_conf(void)
{
    csa_t app_tmp;
    memcpy(&app_tmp, (void *)APP_CONF_ADDR, offsetof(csa_t, state));
    memset(&app_tmp.conf_from, 0, 4);
    app_tmp.tc_pos = 0;

    if (app_tmp.magic_code == 0xcdcd && app_tmp.conf_ver == APP_CONF_VER) {
        memcpy(&csa, &app_tmp, offsetof(csa_t, state));
        csa.conf_from = 1;
    } else {
        csa = csa_dft;
    }
}

int save_conf(void)
{
    uint8_t ret;
    uint32_t err_page = 0;
    FLASH_EraseInitTypeDef f;
    f.TypeErase = FLASH_TYPEERASE_PAGES;
    f.Banks = FLASH_BANK_1;
    f.Page = 63; // last page
    f.NbPages = 1;

    ret = HAL_FLASH_Unlock();
    if (ret == HAL_OK)
        ret = HAL_FLASHEx_Erase(&f, &err_page);

    if (ret != HAL_OK)
        d_info("conf: failed to erase flash\n");

    uint64_t *dst_dat = (uint64_t *)APP_CONF_ADDR;
    uint64_t *src_dat = (uint64_t *)&csa;
    int cnt = (offsetof(csa_t, state) + 7) / 8;

    for (int i = 0; ret == HAL_OK && i < cnt; i++)
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(dst_dat + i), *(src_dat + i));
    ret |= HAL_FLASH_Lock();

    if (ret == HAL_OK) {
        d_info("conf: save to flash successed\n");
        return 0;
    } else {
        d_error("conf: save to flash error\n");
        return 1;
    }
}


#define t_name(expr)  \
        (_Generic((expr), \
                int8_t: "b", uint8_t: "B", \
                int16_t: "h", uint16_t: "H", \
                int32_t: "i", uint32_t: "I", \
                int: "i", \
                bool: "b", \
                float: "f", \
                char *: "[c]", \
                uint8_t *: "[B]", \
                regr_t *: "{H,H}", \
                default: "-"))


#define CSA_SHOW(_p, _x, _desc) \
        d_debug("  [ 0x%04x, %d, \"%s\", " #_p ", \"" #_x "\", \"%s\" ],\n", \
                offsetof(csa_t, _x), sizeof(csa._x), t_name(csa._x), _desc);

#define CSA_SHOW_SUB(_p, _x, _y_t, _y, _desc) \
        d_debug("  [ 0x%04x, %d, \"%s\", " #_p ", \"" #_x "_" #_y "\", \"%s\" ],\n", \
                offsetof(csa_t, _x) + offsetof(_y_t, _y), sizeof(csa._x._y), t_name(csa._x._y), _desc);

void csa_list_show(void)
{
    d_info("csa_list_show:\n\n"); debug_flush(true);

    CSA_SHOW(1, magic_code, "Magic code: 0xcdcd");
    CSA_SHOW(1, conf_ver, "Config version");
    CSA_SHOW(0, conf_from, "0: default config, 1: load from flash");
    CSA_SHOW(0, do_reboot, "Write 1 to reboot");
    CSA_SHOW(0, save_conf, "Write 1 to save current config to flash");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW_SUB(1, bus_cfg, cdctl_cfg_t, mac, "RS-485 port id, range: 0~254");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, baud_l, "RS-485 baud rate for first byte");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, baud_h, "RS-485 baud rate for follow bytes");
    CSA_SHOW_SUB(1, bus_cfg, cdctl_cfg_t, filter, "Multicast address");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, mode, "0: Arbitration, 1: Break Sync");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, tx_permit_len, "Allow send wait time");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, max_idle_len, "Max idle wait time for BS mode");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, tx_pre_len, " Active TX_EN before TX");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(0, dbg_en, "1: Report debug message to host, 0: do not report");
    CSA_SHOW_SUB(2, dbg_dst, cdn_sockaddr_t, addr, "Send debug message to this address");
    CSA_SHOW_SUB(1, dbg_dst, cdn_sockaddr_t, port, "Send debug message to this port");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(1, qxchg_set, "Config the write data components for quick-exchange channel");
    CSA_SHOW(1, qxchg_ret, "Config the return data components for quick-exchange channel");
    CSA_SHOW(1, qxchg_ro, "Config the return data components for the read only quick-exchange channel");
    d_info("\n"); debug_flush(true);

    CSA_SHOW_SUB(2, dbg_raw_dst, cdn_sockaddr_t, addr, "Send raw debug data to this address");
    CSA_SHOW_SUB(1, dbg_raw_dst, cdn_sockaddr_t, port, "Send raw debug data to this port");
    CSA_SHOW(1, dbg_raw_msk, "Config which raw debug data to be send");
    CSA_SHOW(0, dbg_raw_th, "Config raw debug data package size");
    CSA_SHOW(1, dbg_raw[0], "Config raw debug for plot0");
    CSA_SHOW(1, dbg_raw[1], "Config raw debug for plot1");
    d_info("\n"); debug_flush(true);

    CSA_SHOW(0, tc_pos, "Set target position");
    CSA_SHOW(0, tc_speed, "Set target speed");
    CSA_SHOW(0, tc_accel, "Set target accel");
    CSA_SHOW(0, tc_speed_min, "Set the minimum speed");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(0, state, "0: disable drive, 1: enable drive");
    d_debug("\n"); debug_flush(true);
    d_debug("   // --------------- Follows are not writable: -------------------\n");
    CSA_SHOW(0, tc_state, "t_curve: 0: stop, 1: run, 2: tailer");
    CSA_SHOW(0, cur_pos, "Motor current position");
    CSA_SHOW(0, tc_vc, "Motor current speed");
    CSA_SHOW(0, tc_ac, "Motor current accel");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(0, string_test, "String test");
    d_debug("\n"); debug_flush(true);
}
