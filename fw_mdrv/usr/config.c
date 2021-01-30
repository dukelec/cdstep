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
        { .offset = offsetof(csa_t, magic_code), .size = offsetof(csa_t, tc_state) - offsetof(csa_t, magic_code) }
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

        .bus_mac = 254,
        .bus_baud_low = 115200,
        .bus_baud_high = 115200,
        .dbg_en = true,
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
        .dbg_raw_skip = { 0 },
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
        .tc_speed_min = 100
};

csa_t csa;


void load_conf(void)
{
    csa_t app_tmp;
    memcpy(&app_tmp, (void *)APP_CONF_ADDR, offsetof(csa_t, state));
    memset(&app_tmp.conf_from, 0, 4);

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
    f.PageAddress = APP_CONF_ADDR;
    f.NbPages = 1;

    ret = HAL_FLASH_Unlock();
    if (ret == HAL_OK)
        ret = HAL_FLASHEx_Erase(&f, &err_page);

    if (ret != HAL_OK)
        printf("conf: failed to erase flash\n");

    uint32_t *dst_dat = (uint32_t *)APP_CONF_ADDR;
    uint32_t *src_dat = (uint32_t *)&csa;
    int cnt = (offsetof(csa_t, state) + 3) / 4;

    for (int i = 0; ret == HAL_OK && i < cnt; i++) {
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(dst_dat + i), *(src_dat + i));
        printf("w: csa @ %08lx, addr %08lx <- val: %08lx\n",
                (uint32_t)(src_dat + i), (uint32_t)(dst_dat + i), *(src_dat + i));
    }

    ret |= HAL_FLASH_Lock();

    if (ret == HAL_OK) {
        printf("conf: save to flash successed\n");
        return 0;
    } else {
        printf("conf: save to flash error\n");
        return 1;
    }
}


#define t_name(expr)  \
        (_Generic((expr), \
                int8_t: "int8_t", uint8_t: "uint8_t", \
                int16_t: "int16_t", uint16_t: "uint16_t", \
                int32_t: "int32_t", uint32_t: "uint32_t", \
                int: "int32_t", \
                bool: "bool", \
                float: "float", \
                uint8_t *: "uint8_t *", \
                regr_t *: "regr_t *", \
                void *: "void *", \
                default: "--"))


#define CSA_SHOW(_x, _desc) \
        d_debug("   R_" #_x " = 0x%04x # len: %d, %s | %s\n", \
                offsetof(csa_t, _x), sizeof(csa._x), t_name(csa._x), _desc);

#define CSA_SHOW_SUB(_x, _y_t, _y, _desc) \
        d_debug("   R_" #_x "_" #_y " = 0x%04x # len: %d, %s | %s\n", \
                offsetof(csa_t, _x) + offsetof(_y_t, _y), sizeof(csa._x._y), t_name(csa._x._y), _desc);

void csa_list_show(void)
{
    d_debug("csa_list_show:\n");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(conf_ver, "Magic Code: 0xcdcd");
    CSA_SHOW(conf_from, "0: default config, 1: load from flash");
    CSA_SHOW(do_reboot, "Write 1 to reboot");
    CSA_SHOW(save_conf, "Write 1 to save current config to flash");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(bus_mac, "RS-485 port id, range: 0~254");
    CSA_SHOW(bus_baud_low, "RS-485 baud rate for first byte");
    CSA_SHOW(bus_baud_high, "RS-485 baud rate for follow bytes");
    CSA_SHOW(dbg_en, "1: Report debug message to host, 0: do not report");
    CSA_SHOW_SUB(dbg_dst, cdn_sockaddr_t, addr, "Send debug message to this address");
    CSA_SHOW_SUB(dbg_dst, cdn_sockaddr_t, port, "Send debug message to this port");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(qxchg_set, "Config the write data components for quick-exchange channel");
    CSA_SHOW(qxchg_ret, "Config the return data components for quick-exchange channel");
    CSA_SHOW(qxchg_ro, "Config the return data components for the read only quick-exchange channel");
    d_info("\n"); debug_flush(true);

    CSA_SHOW_SUB(dbg_raw_dst, cdn_sockaddr_t, addr, "Send raw debug data to this address");
    CSA_SHOW_SUB(dbg_raw_dst, cdn_sockaddr_t, port, "Send raw debug data to this port");
    CSA_SHOW(dbg_raw_msk, "Config which raw debug data to be send");
    CSA_SHOW(dbg_raw_th, "Config raw debug data package size");
    CSA_SHOW(dbg_raw_skip, "Reduce raw debug data");
    CSA_SHOW(dbg_raw, "Config raw debug data components");
    d_info("\n"); debug_flush(true);

    CSA_SHOW(tc_pos, "Set target position");
    CSA_SHOW(tc_speed, "Set target speed");
    CSA_SHOW(tc_accel, "Set target accel");
    CSA_SHOW(tc_speed_min, "Set the minimum speed");
    d_debug("\n"); debug_flush(true);

    CSA_SHOW(state, "0: disable drive, 1: enable drive");
    d_debug("\n"); debug_flush(true);
    d_debug("   #--------------- Follows are not writable: -------------------\n");
    CSA_SHOW(tc_state, "t_curve: 0: stop, 1: run, 2: tailer");
    CSA_SHOW(cur_pos, "Motor current position");
    CSA_SHOW(tc_vc, "Motor current speed");
    CSA_SHOW(tc_ac, "Motor current accel");
    d_debug("\n"); debug_flush(true);
}
