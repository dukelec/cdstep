/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "app_main.h"

app_conf_t app_conf = {
        .magic_code = 0xcdcd,
        .stay_in_bl = false,

        .rs485_addr = { .net = 0, .mac = 254 },
        .rs485_baudrate_low = 1000000,
        .rs485_baudrate_high = 10000000
};


void load_conf(void)
{

}

void save_conf(void)
{

}

void save_to_flash_service(cdnet_packet_t *pkt)
{
    if (pkt->len != 0) {
        list_put(n_intf.free_head, &pkt->node);
        return;
    }

    save_conf();
    pkt->len = 0;
    cdnet_exchg_src_dst(&n_intf, pkt);
    list_put(&n_intf.tx_head, &pkt->node);
}
