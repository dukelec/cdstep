/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"

csa_t csa = {
        .magic_code = 0xcdcd,
        .conf_ver = APP_CONF_VER,

        .bus_mac = 254,
        .bus_baud_low = 1000000,
        .bus_baud_high = 1000000,
        .dbg_en = true,
        .dbg_dst = { .addr = {0x80, 0x00, 0x00}, .port = 9 },
};


void load_conf(void)
{
    csa_t app_tmp;
    memcpy(&app_tmp, (void *)APP_CONF_ADDR, sizeof(csa_t));
    memset(&app_tmp.conf_from, 0, 4);

    if (app_tmp.magic_code == 0xcdcd && app_tmp.conf_ver == APP_CONF_VER) {
        memcpy(&csa, &app_tmp, sizeof(csa_t));
        csa.conf_from = 1;
    }
}

