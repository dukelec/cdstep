R_conf_ver = 0x0002 # len: 2, uint16_t | Magic Code: 0xcdcd
R_conf_from = 0x0004 # len: 1, bool | 0: default config, 1: load from flash
R_do_reboot = 0x0005 # len: 1, bool | Write 1 to reboot
R_save_conf = 0x0007 # len: 1, bool | Write 1 to save current config to flash

R_bus_mac = 0x0009 # len: 1, uint8_t | RS-485 port id, range: 0~254
R_bus_baud_low = 0x000c # len: 4, uint32_t | RS-485 baud rate for first byte
R_bus_baud_high = 0x0010 # len: 4, uint32_t | RS-485 baud rate for follow bytes
R_dbg_en = 0x0014 # len: 1, bool | 1: Report debug message to host, 0: do not report
R_dbg_dst_addr = 0x0016 # len: 3, uint8_t * | Send debug message to this address
R_dbg_dst_port = 0x001a # len: 2, uint16_t | Send debug message to this port

R_qxchg_set = 0x001c # len: 40, regr_t * | Config the write data components for quick-exchange channel
R_qxchg_ret = 0x0044 # len: 40, regr_t * | Config the return data components for quick-exchange channel
R_qxchg_ro = 0x006c # len: 40, regr_t * | Config the return data components for the read only quick-exchange channel

R_dbg_raw_dst_addr = 0x0094 # len: 3, uint8_t * | Send raw debug data to this address
R_dbg_raw_dst_port = 0x0098 # len: 2, uint16_t | Send raw debug data to this port
R_dbg_raw_msk = 0x009a # len: 1, uint8_t | Config which raw debug data to be send
R_dbg_raw_th = 0x009b # len: 1, uint8_t | Config raw debug data package size
R_dbg_raw_skip = 0x009c # len: 2, uint8_t * | Reduce raw debug data
R_dbg_raw = 0x009e # len: 80, -- | Config raw debug data components

R_tc_pos = 0x00f0 # len: 4, int32_t | Set target position
R_tc_speed = 0x00f4 # len: 4, uint32_t | Set target speed
R_tc_accel = 0x00f8 # len: 4, uint32_t | Set target accel
R_tc_speed_min = 0x00fc # len: 4, uint32_t | Set the minimum speed

R_state = 0x0100 # len: 1, uint8_t | 0: disable drive, 1: enable drive

#------------ Follows are not writable: -------------------
R_tc_state = 0x0101 # len: 1, uint8_t | t_curve: 0: stop, 1: run, 2: tailer
R_cur_pos = 0x0104 # len: 4, int32_t | Motor current position
R_tc_vc = 0x0108 # len: 4, float | Motor current speed
R_tc_ac = 0x010c # len: 4, float | Motor current accel

