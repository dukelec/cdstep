/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"


void raw_dbg(int idx)
{
    static cd_frame_t *frm_raw = NULL;
    static bool frm_less = false;

    if (!csa.dbg_raw_en) {
        if (frm_raw) {
            cd_list_put(&frame_free_head, frm_raw);
            frm_raw = NULL;
        }
        return;
    }

    if (frm_less && frame_free_head.len >= FRAME_MAX - 5)
        frm_less = false;

    if (!frm_less && !frm_raw) {
        if (frame_free_head.len < 5) {
            frm_less = true;
            return;

        } else {
            frm_raw = cd_list_get(&frame_free_head);
            frm_raw->dat[0] = csa.mac;
            frm_raw->dat[1] = 0x0;
            frm_raw->dat[2] = 4;
            frm_raw->dat[3] = 0x40 | idx;
            frm_raw->dat[4] = 0xa;
            put_unaligned16(csa.loop_cnt, frm_raw->dat + 5);
        }
    }
    if (!frm_raw)
        return;

    uint8_t len_bk = frm_raw->dat[2];
    for (int i = 0; i < 6; i++) { // len of csa.dbg_raw
        regr_t *regr = &csa.dbg_raw[i];
        if (!regr->size)
            break;
        uint8_t *dst_dat = frm_raw->dat + frm_raw->dat[2] + 3;
        uint16_t offset;
        if (!csa_range_to_internal(regr->offset, regr->size, &offset))
            break;
        memcpy(dst_dat, ((void *) &csa) + offset, regr->size);
        frm_raw->dat[2] += regr->size;
    }

    uint8_t len_delta = frm_raw->dat[2] - len_bk;
    if (frm_raw->dat[2] + len_delta > CDN_MAX_PAYLOAD) {
        cdctl_send_frame(&r_dev.cd_dev, frm_raw);
        frm_raw = NULL;
    }
}
