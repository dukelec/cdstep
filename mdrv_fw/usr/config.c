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
        }, {
            .range = { .offset = offsetof(csa_t, ref_volt), .size = sizeof(((csa_t *)0)->ref_volt) },
            .after = ref_volt_w_hook
        }
};

csa_hook_t csa_r_hook[] = {
        {
            .range = { .offset = offsetof(csa_t, drv_mo), .size = 1 },
            .before = drv_mo_r_hook
        }
};

int csa_w_allow_num = sizeof(csa_w_allow) / sizeof(regr_t);
int csa_w_hook_num = sizeof(csa_w_hook) / sizeof(csa_hook_t);
int csa_r_hook_num = sizeof(csa_r_hook) / sizeof(csa_hook_t);


const csa_t csa_dft = {
        .magic_code = 0xcdcd,
        .conf_ver = APP_CONF_VER,

        .bus_net = 0,
        .bus_cfg = CDCTL_CFG_DFT(0xfe),
        .dbg_en = false,

        .qxchg_mcast = { .offset = 0, .size = 4 * 3},
        .qxchg_set = {
                { .offset = offsetof(csa_t, tc_pos), .size = 4 * 3 }
        },
        .qxchg_ret = {
                { .offset = offsetof(csa_t, cur_pos), .size = 4 * 2 }
        },

        .force_protection = 700,    // x100
        .force_threshold = 100,     // x100

        .dbg_raw_msk = 0,
        .dbg_raw = {
                {
                        { .offset = offsetof(csa_t, tc_pos), .size = 4 },
                        { .offset = offsetof(csa_t, tc_state), .size = 1 },
                        { .offset = offsetof(csa_t, cal_pos), .size = 4 },
                        { .offset = offsetof(csa_t, cur_pos), .size = 4 * 3 } // + tc_vc, tc_va
                }, {
                        { .offset = offsetof(csa_t, pid_pos) + offsetof(pid_i_t, target), .size = 4 * 3 },
                        { .offset = offsetof(csa_t, cal_speed), .size = 4 },
                }
        },

        .ref_volt = 500,
        .md_val = 7,       // 3'b111
        .lim_en = true,

        .tc_speed = 100000,
        .tc_accel = 200000,
        .tc_accel_emg = 8000000,

        .pid_pos = {
                .kp = 50, .ki = 5000, .kd = 0.02,
                .out_min = -2000000,    // 64000000/32
                .out_max = 2000000,     // limit output speed
                .period = 1.0f / LOOP_FREQ
        },

        .string_test = "hello"
};

csa_t csa;


void load_conf(void)
{
    uint16_t magic_code = *(uint16_t *)APP_CONF_ADDR;
    uint16_t conf_ver = *(uint16_t *)(APP_CONF_ADDR + 2);
    csa = csa_dft;

    if (magic_code == 0xcdcd && conf_ver == APP_CONF_VER) {
        memcpy(&csa, (void *)APP_CONF_ADDR, offsetof(csa_t, _end_save));
        csa.conf_from = 1;
    } else if (magic_code == 0xcdcd && (conf_ver >> 8) == (APP_CONF_VER >> 8)) {
        memcpy(&csa, (void *)APP_CONF_ADDR, offsetof(csa_t, _end_common));
        csa.conf_from = 2;
        csa.conf_ver = APP_CONF_VER;
    }
    if (csa.conf_from) {
        memset(&csa.do_reboot, 0, 3);
        csa.tc_pos = 0;
        csa.pid_pos.out_max = csa_dft.pid_pos.out_max;
        csa.pid_pos.out_min = csa_dft.pid_pos.out_min;
        if (csa.force_protection < csa.force_threshold)
            csa.force_protection = csa.force_threshold;
    }
}

int save_conf(void)
{
    uint8_t ret = flash_erase(APP_CONF_ADDR, 2048);
    if (ret != HAL_OK)
        d_info("conf: failed to erase flash\n");
    ret = flash_write(APP_CONF_ADDR, offsetof(csa_t, _end_save), (uint8_t *)&csa);

    if (ret == HAL_OK) {
        d_info("conf: save to flash successed, size: %d\n", offsetof(csa_t, _end_save));
        return 0;
    } else {
        d_error("conf: save to flash error\n");
        return 1;
    }
}


int flash_erase(uint32_t addr, uint32_t len)
{
    int ret = -1;
    uint32_t err_sector = 0xffffffff;
    FLASH_EraseInitTypeDef f;

    uint32_t ofs = addr & ~0x08000000;
    if (ofs <= 0x6000 && 0x6000 < ofs + len) {
        d_error("nvm erase: avoid erasing self\n");
        return ret;
    }

    f.TypeErase = FLASH_TYPEERASE_PAGES;
    f.Banks = FLASH_BANK_1;
    f.Page = ofs / 2048;
    f.NbPages = (ofs + len) / 2048 - f.Page;
    if ((ofs + len) % 2048)
        f.NbPages++;

    ret = HAL_FLASH_Unlock();
    if (ret == HAL_OK)
        ret = HAL_FLASHEx_Erase(&f, &err_sector);
    ret |= HAL_FLASH_Lock();
    d_debug("nvm erase: %08lx +%08lx (%ld %ld), %08lx, ret: %d\n", addr, len, f.Page, f.NbPages, err_sector, ret);
    return ret;
}

int flash_write(uint32_t addr, uint32_t len, const uint8_t *buf)
{
    int ret = -1;

    uint64_t *dst_dat = (uint64_t *) addr;
    int cnt = (len + 7) / 8;
    uint64_t *src_dat = (uint64_t *)buf;

    ret = HAL_FLASH_Unlock();
    for (int i = 0; ret == HAL_OK && i < cnt; i++) {
        uint64_t dat = get_unaligned32((uint8_t *)(src_dat + i));
        dat |= (uint64_t)get_unaligned32((uint8_t *)(src_dat + i) + 4) << 32;
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)(dst_dat + i), dat);
    }
    ret |= HAL_FLASH_Lock();

    d_verbose("nvm write: %p %ld(%d), ret: %d\n", dst_dat, len, cnt, ret);
    return ret;
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
                regr_t: "H,H", \
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
    d_info("csa_list_show:\n\n");
    while (frame_free_head.len < FRAME_MAX - 5);

    CSA_SHOW(1, magic_code, "Magic code: 0xcdcd");
    CSA_SHOW(1, conf_ver, "Config version");
    CSA_SHOW(0, conf_from, "0: default config, 1: all from flash, 2: partly from flash");
    CSA_SHOW(0, do_reboot, "1: reboot to bl, 2: reboot to app");
    CSA_SHOW(0, save_conf, "Write 1 to save current config to flash");
    d_info("\n");

    CSA_SHOW_SUB(1, bus_cfg, cdctl_cfg_t, mac, "RS-485 port id, range: 0~254");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, baud_l, "RS-485 baud rate for first byte");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, baud_h, "RS-485 baud rate for follow bytes");
    CSA_SHOW_SUB(1, bus_cfg, cdctl_cfg_t, filter_m, "Multicast address");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, mode, "0: Arbitration, 1: Break Sync");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, tx_permit_len, "Allow send wait time");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, max_idle_len, "Max idle wait time for BS mode");
    CSA_SHOW_SUB(0, bus_cfg, cdctl_cfg_t, tx_pre_len, " Active TX_EN before TX");
    d_debug("\n");

    CSA_SHOW(0, dbg_en, "1: Report debug message to host, 0: do not report");
    d_info("\n");

    CSA_SHOW(1, qxchg_mcast, "Quick-exchange multicast data slice");
    CSA_SHOW(1, qxchg_set, "Config the write data components for quick-exchange channel");
    CSA_SHOW(1, qxchg_ret, "Config the return data components for quick-exchange channel");
    d_info("\n");

    CSA_SHOW(0, force_trigger_en, "Force trigger enable");
    CSA_SHOW(0, force_protection, "Set force protection");
    CSA_SHOW(0, force_threshold, "Set force threshold");
    d_debug("\n");

    CSA_SHOW(1, dbg_raw_msk, "Config which raw debug data to be send");
    CSA_SHOW(1, dbg_raw[0], "Config raw debug for plot0");
    CSA_SHOW(1, dbg_raw[1], "Config raw debug for plot1");
    d_info("\n");

    CSA_SHOW(0, ref_volt, "Motor driver reference voltage, unit: mV");
    CSA_SHOW(0, md_val, "Motor driver md[2:0] pin value");
    CSA_SHOW(0, set_home, "Write 1 set home position");
    CSA_SHOW(0, drv_mo, "MO pin state of drv chip, for debug");
    CSA_SHOW(0, lim_en, "Enable limit switch");
    d_debug("\n");

    CSA_SHOW(0, tc_pos, "Set target position");
    CSA_SHOW(0, tc_speed, "Set target speed");
    CSA_SHOW(0, tc_accel, "Set target accel");
    CSA_SHOW(0, tc_accel_emg, "Set emergency accel");
    d_debug("\n");

    CSA_SHOW_SUB(0, pid_pos, pid_i_t, kp, "");
    CSA_SHOW_SUB(0, pid_pos, pid_i_t, ki, "");
    CSA_SHOW_SUB(0, pid_pos, pid_i_t, kd, "");
    //CSA_SHOW_SUB(0, pid_pos, pid_i_t, out_min, "");
    //CSA_SHOW_SUB(0, pid_pos, pid_i_t, out_max, "");
    CSA_SHOW(0, cal_pos, "PID input position");
    CSA_SHOW(0, cal_speed, "PID output speed");
    d_info("\n");

    CSA_SHOW(0, state, "0: disable drive, 1: enable drive");
    d_debug("\n");

    d_debug("   // --------------- Follows are not writable: -------------------\n");
    CSA_SHOW(0, tc_state, "t_curve: 0: stop, 1: run");
    CSA_SHOW(0, cur_pos, "Motor current position");
    CSA_SHOW(0, tc_vc, "Motor current speed");
    CSA_SHOW(0, tc_ac, "Motor current accel");
    d_debug("\n");

    CSA_SHOW(0, loop_cnt, "Count for plot");
    CSA_SHOW(0, string_test, "String test");
    d_debug("\n");
    while (frame_free_head.len < FRAME_MAX - 5);
}
