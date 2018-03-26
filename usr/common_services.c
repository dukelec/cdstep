/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdctl_bx_it.h"
#include "app_main.h"

extern cdctl_intf_t r_intf;

static void get_uid(char *buf)
{
    const char tlb[] = "0123456789abcdef";
    int i;

    for (i = 0; i < 12; i++) {
        uint8_t val = *((char *)UID_BASE + i);
        buf[i * 2 + 0] = tlb[val & 0xf];
        buf[i * 2 + 1] = tlb[val >> 4];
    }
    buf[24] = '\0';
}

// make sure local mac != 255 before call any service

// device info
void p1_service(cdnet_packet_t *pkt)
{
    char cpu_id[25];
    char info_str[100];

    // M: model; S: serial string; HW: hardware version; SW: software version
    get_uid(cpu_id);
    sprintf(info_str, "M: cdbus_bridge; S: %s; SW: %s", cpu_id, SW_VER);

    // filter string by input data
    if (pkt->len != 0 && strstr(info_str, (char *)pkt->dat) == NULL) {
        list_put(n_intf.free_head, &pkt->node);
        return;
    }

    strcpy((char *)pkt->dat, info_str);
    pkt->len = strlen(info_str);
    cdnet_exchg_src_dst(&n_intf, pkt);
    list_put(&n_intf.tx_head, &pkt->node);
}

// device baud rate
void p2_service(cdnet_packet_t *pkt)
{
    list_put(n_intf.free_head, &pkt->node);
}

// device addr
void p3_service(cdnet_packet_t *pkt)
{
    // set mac
    if (pkt->len == 2 && pkt->dat[0] == 0x00) {
        r_intf.cd_intf.set_filter(&r_intf.cd_intf, pkt->dat[1]);
        n_intf.addr.mac = pkt->dat[1];
        pkt->len = 0;
        d_debug("set filter: %d...\n", n_intf.addr.mac);
        goto out_send;
    }
    // set net
    if (pkt->len == 2 && pkt->dat[0] == 0x01) {
        n_intf.addr.net = pkt->dat[1];
        pkt->len = 0;
        d_debug("set net: %d...\n", n_intf.addr.net);
        goto out_send;
    }
    // check net id
    if (pkt->len == 1 && pkt->dat[0] == 0x01) {
        pkt->len = 1;
        pkt->dat[0] = n_intf.addr.net;
        goto out_send;
    }
    goto err_free; // TODO: add mac address auto allocation

out_send:
    cdnet_exchg_src_dst(&n_intf, pkt);
    list_put(&n_intf.tx_head, &pkt->node);
    return;

err_free:
    list_put(n_intf.free_head, &pkt->node);
}
