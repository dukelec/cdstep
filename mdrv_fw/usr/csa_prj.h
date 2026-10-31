/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CSA_PRJ_H__
#define __CSA_PRJ_H__

// Project private register areas, see the kpcd protocol document.
// A product branch fills this file in, the common code stays untouched:
//
//   saved config [0x0300, 0x03d0): prj_conf_ver at 0x0300, csa_prj_conf_t from 0x0304,
//   stored to flash with the common config, back to its defaults when the stored
//   CSA_PRJ_CONF_VER differs from the firmware's
//
//     #define CSA_PRJ_CONF_EN
//     #define CSA_PRJ_CONF_VER    1           // bump when csa_prj_conf_t changes
//     typedef struct { ... } csa_prj_conf_t;  // size: a multiple of 4, up to 204 bytes
//     #define CSA_PRJ_CONF_DFT    { ... }     // its part of csa_dft
//
//   status from 0x0500: csa_prj_stat_t, not saved, read-only unless csa_w_allow lists a field
//
//     #define CSA_PRJ_STAT_EN
//     typedef struct { ... } csa_prj_stat_t;
//
// With either one enabled, config.c defines csa_list_show_prj() with the
// CSA_SHOW_PRJ_CONF / CSA_SHOW_PRJ_STAT macros.

#endif
