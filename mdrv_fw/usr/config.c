/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "app_main.h"

reg2r_t csa_w_allow[] = {
        { .offset = offsetof(csa_t, magic_code), .size = 4 },
        { .offset = offsetof(csa_t, do_reboot),
                .size = offsetof(csa_t, tp_max_err) + sizeof(csa.tp_max_err) - offsetof(csa_t, do_reboot) },
        { .offset = offsetof(csa_t, pid_pos_kp), .size = offsetof(csa_t, _end_save) - offsetof(csa_t, pid_pos_kp) },
        { .offset = offsetof(csa_t, set_home), .size = 1 },
        { .offset = offsetof(csa_t, state), .size = 1 },
        { .offset = offsetof(csa_t, string_test), .size = sizeof(csa.string_test) }
};

csa_hook_t csa_w_hook[] = {
        {
            .range = { .offset = offsetof(csa_t, tp_pos),
                    .size = offsetof(csa_t, tp_max_err) + sizeof(csa.tp_max_err) - offsetof(csa_t, tp_pos) },
            .after = motor_w_hook
        }, {
            .range = { .offset = offsetof(csa_t, pid_pos_kp),
                    .size = offsetof(csa_t, pid_pos_out_max) + sizeof(csa.pid_pos_out_max) - offsetof(csa_t, pid_pos_kp) },
            .after = motor_w_hook
        }, {
            .range = { .offset = offsetof(csa_t, state), .size = 1 },
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

int csa_w_allow_num = sizeof(csa_w_allow) / sizeof(reg2r_t);
int csa_w_hook_num = sizeof(csa_w_hook) / sizeof(csa_hook_t);
int csa_r_hook_num = sizeof(csa_r_hook) / sizeof(csa_hook_t);


const csa_t csa_dft = {
        .magic_code = 0xcdcd,
        .conf_ver = APP_CONF_VER,

        .mac = 0xfe,
        .baud_rate_l = 115200,
        .baud_rate_h = 115200,
        .bus_filter_m = { 0xff, 0xff },
        .bus_mode = 1,
        .bus_idle_wait_len = 0x0a,
        .bus_tx_permit_len = 0x14,
        .bus_max_idle_len = 0xc8,
        .bus_tx_pre_len = 0x01,
        .dbg_en = false,

        .qxchg_mcast = { .offset = 0, .size = 4 * 3},
        .qxchg_set = {
                { .offset = offsetof(csa_t, tp_pos), .size = 4 * 3 }
        },
        .qxchg_ret = {
                { .offset = CSA_OFS(meas_pos), .size = 4 },
                { .offset = offsetof(csa_t, tp_vel_out), .size = 4 }
        },

        .dbg_raw_en = 0,
        .dbg_raw = {
                { .offset = offsetof(csa_t, tp_pos), .size = 4 },
                { .offset = offsetof(csa_t, tp_state), .size = 1 },
                { .offset = CSA_OFS(tgt_pos), .size = 4 },
                { .offset = CSA_OFS(meas_pos), .size = 4 },
                { .offset = CSA_OFS(tgt_speed), .size = 4 },
                { .offset = offsetof(csa_t, tp_vel_out), .size = 4 }
        },

        .ref_volt = 500,
        .md_val = 7,       // 3'b111
        .lim_en = true,

        .tp_speed = 100000,
        .tp_accel = 200000,
        .tp_accel_emg = 2000000,
        .tp_max_err = 0,

        .pid_pos_kp = 500,
        .pid_pos_out_min = -2000000,    // 64000000/32
        .pid_pos_out_max = 2000000,     // limit output speed

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
    } else if (magic_code == 0xcdcd && (conf_ver >> 12) == (APP_CONF_VER >> 12)) {
        memcpy(&csa, (void *)APP_CONF_ADDR, offsetof(csa_t, _end_common));
        csa.conf_from = 2;
        csa.conf_ver = APP_CONF_VER;
    }
    if (csa.conf_from) {
        memset(&csa.do_reboot, 0, 3);
        csa.dbg_raw_en = 0;
        csa.tp_pos = 0;
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
                bool: "b", \
                float: "f", \
                char *: "[c]", \
                uint8_t *: "[B]", \
                regr_t: "H,B2", \
                regr_t *: "{H,B2}", \
                default: "-"))


#define CSA_SHOW(_p, _x, _desc) \
        d_debug("  [ 0x%04x, %d, \"%s\", " #_p ", \"" #_x "\", \"%s\" ],\n", \
                offsetof(csa_t, _x), sizeof(csa._x), t_name(csa._x), _desc);

#define CSA_SHOW_OFS(_p, _x, _desc) \
        d_debug("  [ 0x%04x, %d, \"%s\", " #_p ", \"" #_x "\", \"%s\" ],\n", \
                CSA_OFS(_x), sizeof(csa._x), t_name(csa._x), _desc);

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
    CSA_SHOW(0, keep_bl, "Keep running in bootloader");
    CSA_SHOW(0, save_conf, "Write 1 to save current config to flash");
    d_info("\n");

    CSA_SHOW(1, mac, "RS-485 port id, range: 0~254");
    CSA_SHOW(0, baud_rate_l, "RS-485 baud rate for first byte");
    CSA_SHOW(0, baud_rate_h, "RS-485 baud rate for follow bytes");
    CSA_SHOW(1, bus_filter_m, "Multicast address");
    CSA_SHOW(0, bus_mode, "0: Traditional, 1: Arbitration, 2: Break Sync, 3: Full-duplex");
    CSA_SHOW(0, bus_idle_wait_len, "Bus idle wait time");
    CSA_SHOW(0, bus_tx_permit_len, "Allow send wait time");
    CSA_SHOW(0, bus_max_idle_len, "Max idle wait time for BS mode");
    CSA_SHOW(0, bus_tx_pre_len, "Active TX_EN before TX");
    d_debug("\n");

    CSA_SHOW(0, dbg_en, "1: Report debug message to host, 0: do not report");
    CSA_SHOW(1, dbg_raw_en, "Enable raw debug plot");
    CSA_SHOW(1, dbg_raw, "Config raw debug for plot");
    d_info("\n");

    CSA_SHOW(1, qxchg_mcast, "Quick-exchange multicast data slice");
    CSA_SHOW(1, qxchg_set, "Config the write data components for quick-exchange channel");
    CSA_SHOW(1, qxchg_ret, "Config the return data components for quick-exchange channel");
    d_info("\n");

    CSA_SHOW(0, tp_pos, "Set target position");
    CSA_SHOW(0, tp_speed, "Set target speed");
    CSA_SHOW(0, tp_accel, "Set target accel");
    CSA_SHOW(0, tp_accel_emg, "Set emergency accel");
    CSA_SHOW(0, tp_max_err, "Maximum allowed position error, 0: disabled");
    CSA_SHOW(0, tp_state, "trap_planner: -1: disable, 0: idle, 1: planning");
    CSA_SHOW(0, tp_vel_out, "Current planned velocity");
    d_debug("\n");

    CSA_SHOW(0, pid_pos_kp, "");
    CSA_SHOW(0, pid_pos_out_min, "");
    CSA_SHOW(0, pid_pos_out_max, "");
    d_info("\n");

    CSA_SHOW(0, ref_volt, "Motor driver reference voltage, unit: mV");
    CSA_SHOW(0, md_val, "Motor driver md[2:0] pin value");
    CSA_SHOW(0, lim_en, "Enable limit switch");
    d_debug("\n");

    CSA_SHOW_OFS(0, set_home, "Write 0x80 to set home position");
    CSA_SHOW_OFS(0, state, "0: stop, 5: position with trap planner");
    d_debug("\n");

    CSA_SHOW_OFS(0, tgt_pos, "PID target position");
    CSA_SHOW_OFS(0, tgt_speed, "PID output speed");
    d_debug("\n");

    CSA_SHOW_OFS(0, meas_pos, "Motor current position");
    d_debug("\n");

    CSA_SHOW_OFS(0, loop_cnt, "Count for plot");
    CSA_SHOW_OFS(0, drv_mo, "MO pin state of drv chip, for debug");
    CSA_SHOW_OFS(0, string_test, "String test");
    d_debug("\n");
    while (frame_free_head.len < FRAME_MAX - 5);
}
