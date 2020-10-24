/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"

regr_t regr_wa[] = {
        { .offset = offsetof(csa_t, magic_code), .size = offsetof(csa_t, cur_pos) - offsetof(csa_t, magic_code) }
};

int regr_wa_num = sizeof(regr_wa) / sizeof(regr_t);


csa_t csa = {
        .magic_code = 0xcdcd,
        .conf_ver = APP_CONF_VER,

        .bus_mac = 254,
        .bus_baud_low = 1000000,
        .bus_baud_high = 1000000,
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


void load_conf(void)
{
    csa_t app_tmp;
    memcpy(&app_tmp, (void *)APP_CONF_ADDR, offsetof(csa_t, state));
    memset(&app_tmp.conf_from, 0, 4);

    if (app_tmp.magic_code == 0xcdcd && app_tmp.conf_ver == APP_CONF_VER) {
        memcpy(&csa, &app_tmp, offsetof(csa_t, state));
        csa.conf_from = 1;
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


#define CSA_SHOW(_x) \
        d_debug("   R_" #_x " = 0x%04x # len: %d\n", offsetof(csa_t, _x), sizeof(csa._x));

#define CSA_SHOW_SUB(_x, _y_t, _y) \
        d_debug("   R_" #_x "_" #_y " = 0x%04x # len: %d\n", offsetof(csa_t, _x) + offsetof(_y_t, _y), sizeof(csa._x._y));

void csa_list_show(void)
{
    d_debug("csa_list_show:\n\n");

    CSA_SHOW(conf_ver);
    CSA_SHOW(conf_from);
    CSA_SHOW(do_reboot);
    CSA_SHOW(save_conf);
    d_debug("\n");

    CSA_SHOW(bus_mac);
    CSA_SHOW(bus_baud_low);
    CSA_SHOW(bus_baud_high);
    CSA_SHOW(dbg_en);
    CSA_SHOW_SUB(dbg_dst, cdn_sockaddr_t, addr);
    CSA_SHOW_SUB(dbg_dst, cdn_sockaddr_t, port);
    d_debug("\n");

    CSA_SHOW(qxchg_set);
    CSA_SHOW(qxchg_ret);
    CSA_SHOW(qxchg_ro);
    d_info("\n");

    CSA_SHOW_SUB(dbg_raw_dst, cdn_sockaddr_t, addr);
    CSA_SHOW_SUB(dbg_raw_dst, cdn_sockaddr_t, port);
    CSA_SHOW(dbg_raw_msk);
    CSA_SHOW(dbg_raw_th);
    CSA_SHOW(dbg_raw_skip);
    CSA_SHOW(dbg_raw);
    d_info("\n");

    CSA_SHOW(tc_pos);
    CSA_SHOW(tc_speed);
    CSA_SHOW(tc_accel);
    CSA_SHOW(tc_speed_min);
    d_debug("\n");

    CSA_SHOW(state);
    CSA_SHOW(tc_state);
    CSA_SHOW(cur_pos);
    CSA_SHOW(tc_vc);
    CSA_SHOW(tc_ac);
    d_debug("\n");
}
