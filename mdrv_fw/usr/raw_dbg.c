/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"

static cdn_sock_t sock_raw_dbg = { .port = 0xa, .ns = &dft_ns, .tx_only = true }; // raw debug
static list_head_t raw_pend = { 0 };
static cdn_pkt_t *pkt_raw[2] = { NULL };


void raw_dbg(int idx)
{
    static bool pkt_less = false;

    if (!(csa.dbg_raw_msk & (1 << idx))) {
        if (pkt_raw[idx]) {
            cdn_pkt_free(&dft_ns, pkt_raw[idx]);
            pkt_raw[idx] = NULL;
        }
        return;
    }

    if (pkt_less) {
        if (raw_pend.len == 0) {
            pkt_less = false;
        }
    }

    if (!pkt_less && !pkt_raw[idx]) {
        if (dft_ns.free_pkt->len < 5 || dft_ns.free_frm->len < 5) {
            pkt_less = true;
            return;

        } else {
            pkt_raw[idx] = cdn_pkt_alloc(sock_raw_dbg.ns);
            pkt_raw[idx]->dst = csa.dbg_raw_dst;
            cdn_pkt_prepare(&sock_raw_dbg, pkt_raw[idx]);
            pkt_raw[idx]->dat[0] = 0x40 | idx;
            put_unaligned32(csa.loop_cnt, pkt_raw[idx]->dat + 1);
            pkt_raw[idx]->len = 5;
        }
    }
    if (!pkt_raw[idx])
        return;

    for (int i = 0; i < 6; i++) { // len of csa.dbg_raw
        regr_t *regr = &csa.dbg_raw[idx][i];
        if (!regr->size)
            break;
        uint8_t *dst_dat = pkt_raw[idx]->dat + pkt_raw[idx]->len;
        memcpy(dst_dat, ((void *) &csa) + regr->offset, regr->size);
        pkt_raw[idx]->len += regr->size;
    }

    if (pkt_raw[idx]->len >= csa.dbg_raw_th) {
        cdn_list_put(&raw_pend, pkt_raw[idx]);
        pkt_raw[idx] = NULL;
    }
}

void raw_dbg_init(void)
{
    cdn_sock_bind(&sock_raw_dbg);
}

void raw_dbg_routine(void)
{
    if (frame_free_head.len > 1) {
        cdn_pkt_t *pkt = cdn_list_get(&raw_pend);
        if (pkt)
            cdn_sock_sendto(&sock_raw_dbg, pkt);
    }
}
