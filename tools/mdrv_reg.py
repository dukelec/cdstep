R_conf_ver = 0x0002 # len: 2
R_conf_from = 0x0004 # len: 1
R_do_reboot = 0x0005 # len: 1
R_save_conf = 0x0007 # len: 1

R_bus_mac = 0x0009 # len: 1
R_bus_baud_low = 0x000c # len: 4
R_bus_baud_high = 0x0010 # len: 4
R_dbg_en = 0x0014 # len: 1
R_dbg_dst_addr = 0x0016 # len: 3
R_dbg_dst_port = 0x001a # len: 2

R_qxchg_set = 0x001c # len: 40
R_qxchg_ret = 0x0044 # len: 40
R_qxchg_ro = 0x006c # len: 40

R_dbg_raw_dst_addr = 0x0094 # len: 3
R_dbg_raw_dst_port = 0x0098 # len: 2
R_dbg_raw_msk = 0x009a # len: 1
R_dbg_raw_th = 0x009b # len: 1
R_dbg_raw_skip = 0x009c # len: 2
R_dbg_raw = 0x009e # len: 80

R_tc_pos = 0x00f0 # len: 4
R_tc_speed = 0x00f4 # len: 4
R_tc_accel = 0x00f8 # len: 4
R_tc_speed_min = 0x00fc # len: 4

R_state = 0x0100 # len: 1
R_tc_state = 0x0101 # len: 1
R_cur_pos = 0x0104 # len: 4
R_tc_vc = 0x0108 # len: 4
R_tc_ac = 0x010c # len: 4
