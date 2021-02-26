/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"
#include "math.h"

const csa_t csa_dft = {
        .magic_code = 0xcdcd,
        .conf_ver = APP_CONF_VER,

        .bus_net = 0,
        .bus_cfg = CDCTL_CFG_DFT(0xfe),
        .dbg_en = false,
        .dbg_dst = { .addr = {0x80, 0x00, 0x00}, .port = 9 }
};

csa_t csa;


void load_conf(void)
{
    csa_t app_tmp;
    memcpy(&app_tmp, (void *)APP_CONF_ADDR, sizeof(csa_t));
    memset(&app_tmp.conf_from, 0, 4);

    if (app_tmp.magic_code == 0xcdcd && (app_tmp.conf_ver & 0xff00) == (APP_CONF_VER & 0xff00)) {
        memcpy(&csa, &app_tmp, sizeof(csa_t));
        csa.conf_from = 1;
    } else {
        csa = csa_dft;
    }
}
