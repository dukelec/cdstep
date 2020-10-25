#!/usr/bin/env python3
# Software License Agreement (BSD License)
#
# Copyright (c) 2017, DUKELEC, Inc.
# All rights reserved.
#
# Author: Duke <d@d-l.io>

"""MDRV debug tool

This tool use CDBUS Bridge by default, communicate with any MDRV on the RS485 bus.

If you want to communicate with CDBUS Bridge itself,
or other device use CDBUS protol through serial port,
use --direct flag then.

Args:
  --dev DEV         # specify serial port, default: /dev/ttyACM0
  --mac MAC         # set CDBUS Bridge filter at first, default: 1
  --direct          # see description above
  --help    | -h    # this help message
  --verbose | -v    # debug level: verbose
  --debug   | -d    # debug level: debug
  --info    | -i    # debug level: info
"""

import sys, os
import struct
import math
from time import sleep
import _thread
import re
import pprint
import matplotlib.pyplot as plt

try:
    import readline
except:
    from pyreadline import Readline
    readline = Readline()

sys.path.append(os.path.join(os.path.dirname(__file__), './pycdnet'))

from mdrv_reg import *
from cdnet.utils.log import *
from cdnet.utils.cd_args import CdArgs
from cdnet.dev.cdbus_serial import CDBusSerial
from cdnet.dev.cdbus_bridge import CDBusBridge
from cdnet.dispatch import *

args = CdArgs()
dev_str = args.get("--dev", dft="/dev/ttyACM0")
direct = args.get("--direct") != None
local_mac = int(args.get("--mac", dft="0xaa" if direct else "0x00"), 0)

monitor = args.get("--monitor", "-m") != None

if args.get("--help", "-h") != None:
    print(__doc__)
    exit()

if args.get("--verbose", "-v") != None:
    logger_init(logging.VERBOSE)
elif args.get("--debug", "-d") != None:
    logger_init(logging.DEBUG)
elif args.get("--info", "-i") != None:
    logger_init(logging.INFO)

if direct:
    dev = CDBusSerial(dev_port=dev_str)
else:
    dev = CDBusBridge(dev_port=dev_str, filter_=local_mac)

CDNetIntf(dev, mac=local_mac)
sock = CDNetSocket(('', 0xcdcd))
sock_dbg = CDNetSocket(('', 9))
sock_raw = CDNetSocket(('', 0xa))


def dbg_echo():
    while True:
        rx = sock_dbg.recvfrom()
        print('\x1b[0;37m  ' + re.sub(br'[^\x20-\x7e]',br'.', rx[0][0:-1]).decode() + '\x1b[0m')
        #print('\x1b[0;37m  ' + re.sub(br'[^\x20-\x7e]',br'.', rx[0]).decode() + '\x1b[0m')

_thread.start_new_thread(dbg_echo, ())


# ori = [[2, 3, 4, 5, 6, 7], [4, 5, 6, 7, 8, 9], ... ]
# t = list(range(0, len(ori)))
# dat = list(zip(*ori))

def init_lines(len_):
    # not use: w for white
    #       green         blue          yellow        black         red           cyan          purple
    ct = [('g.-', 0.2), ('b+-', 0.2), ('yo-', 0.4), ('kx-', 0.2), ('r.-', 0.2), ('c.-', 0.3), ('m.-', 0.3)]                 
    for i in range(len_):
        plt.setp(plt.plot([], [], ct[i][0]), alpha=ct[i][1]) # first two args: [t], [dat]

dbg_raw_msk = 0x1

def dbg_raw():
    dp = []
    dd = []     # sub plots of [[time], [line0], [line1], ... [lineN]]
    dt_len = [] # replace type to len from dt
    dt_sum = [] # sum len of a data group
    dt = [
        ['i', 'i', 'b', 'i', 'f'],
        []
    ]
    
    _L = lambda _i : dd[idx][_i+1][-1]
    dtc = [
        [], #['_L(1) - _L(3)'], # cal: line1 - line3
        []
    ]
    
    for i in range(len(dt)):
        dd.append([])
        dd[i].append([]) # [time]
        dt_len.append([])
        sum_ = 0
        for n in range(len(dt[i])):
            dd[i].append([]) # [lineN]
            sum_ += struct.calcsize(dt[i][n])
            dt_len[i].append(struct.calcsize(dt[i][n]))
        dt_sum.append(sum_)
    
    for i in range(len(dtc)):
        for n in range(len(dtc[i])):
            dd[i].append([]) # [lineN]
    
    w = plt.figure('mdrv plot') # New window
    plt_idx = 1
    for i in range(2): # max 2 plot
        if (1 << i) & dbg_raw_msk:
            dp.append(w.add_subplot(bin(dbg_raw_msk).count('1'), 1, plt_idx))
            plt.ylabel(f'idx {i}')
            init_lines(len(dd[i])-1)
            plt_idx += 1
        else:
            dp.append(None)
    w.tight_layout()
    w.show()
    plt.get_current_fig_manager().toolbar.pan()
    #plt.set_default_size(600, 900)
    
    update_delay = 0
    plt_update = False
    
    while True:
        if plt_update == False or update_delay == 0:
            #plt.pause(0.001)
            w.canvas.draw_idle()
            w.canvas.start_event_loop(0.000001)
        update_delay += 1
        if update_delay > 100:
            update_delay = 0
            if plt_update:
                plt_update = False
                for i in range(len(dd[idx])-1):
                    dp[idx].lines[i].set_xdata(dd[idx][0])
                    dp[idx].lines[i].set_ydata(dd[idx][i+1])
                dp[idx].relim()
                dp[idx].autoscale_view()

        dat, _ = sock_raw.recvfrom(timeout=0.01)
        if dat != None:
            idx = dat[0] & 3;
            #cnt = struct.unpack("I", dat[1:1+4])[0]
            #skip = dat[5]
            #step = (skip + 1) * pow(5, min(idx, 2))
            p = 0
            while True:
                #dd[idx][0].append(cnt + p * step) # x axis
                addr = p * (dt_sum[idx] + 4) + 1
                dd[idx][0].append(struct.unpack('<I', dat[addr:addr+4])[0]) # x axis
                addr += 4
                for i in range(len(dt[idx])):
                    dd[idx][i+1].append(struct.unpack(dt[idx][i], dat[addr:addr+dt_len[idx][i]])[0])
                    addr += dt_len[idx][i]
                for i in range(len(dtc[idx])):
                    val = eval(dtc[idx][i])
                    dd[idx][len(dt[idx])+1+i].append(val)
                p += 1
                if len(dat[p*(dt_sum[idx]+4)+1:]) < dt_sum[idx] + 4:
                    break
            #pp.pprint(dd[idx])
            plt_update = True


def csa_write(offset, dat):
    sock.sendto(b'\x20' + struct.pack("<H", offset) + dat, ('80:00:ff', 5))
    ret, _ = sock.recvfrom(timeout=1)
    if ret == None or ret[0] != 0x80:
        print(f'csa_write error at: 0x{offset:x}: {dat.hex()}')
    return ret

def csa_read(offset, len_):
    sock.sendto(b'\x00' + struct.pack("<HB", offset, len_), ('80:00:ff', 5))
    ret, _ = sock.recvfrom(timeout=1)
    if ret == None or ret[0] != 0x80:
        print(f'csa_write read at: 0x{offset:x}, len: {len_}')
    return ret

def qxchg_wr(dat):
    sock.sendto(b'\x20' + dat, ('80:00:ff', 6))
    ret, _ = sock.recvfrom(timeout=1)
    if ret == None or ret[0] != 0x80:
        print(f'qxchg error set: {dat.hex()}')
    return ret


def calibrate_electrical_angle():
    print('init...')
    csa_write(R_state, bytes([0]))
    sleep(0.5)
    csa_write(R_cali_angle_step, struct.pack("<f", 0))
    csa_write(R_cali_current, struct.pack("<f", 200))
    csa_write(R_state, bytes([1]))
    
    data = {}
    data['amount'] = 0
    
    for i in range(21):
        csa_write(R_cali_angle_elec, struct.pack("<f", 0))
        sleep(1)

        tmp = csa_read(R_ori_encoder, 2)
        data['a0'] = struct.unpack("<H", tmp[1:])[0]
        print('a0 : %04x' % data['a0'])
        
        csa_write(R_cali_angle_elec, struct.pack("<f", math.pi/2))
        sleep(1)
        
        csa_write(R_cali_angle_elec, struct.pack("<f", math.pi))
        sleep(1)

        tmp = csa_read(R_ori_encoder, 2)
        data['a1'] = struct.unpack("<H", tmp[1:])[0]
        print('a1 : %04x' % data['a1'])
        
        csa_write(R_cali_angle_elec, struct.pack("<f", math.pi + math.pi/2))
        sleep(1)
        
        n = round(data['a0'] + ((data['a1'] - data['a0']) & 0xffff)/2.0)
        print(f'{i} -- : {n:08x}')
        n = round(data['a0'] + ((data['a1'] - data['a0']) & 0xffff)/2.0 - (65536.0*i/21)) & 0xffff
        print(f'{i} <> : {n:08x}')
        data['amount'] += n
    
    
    csa_write(R_state, bytes([0])) # stop
    
    #msg = {}
    data['avg'] = round(data['amount'] / 21.0)
    print(' avg: %08x' % data['avg'])
    #msg = bytes(struct.pack("i", data['avg']))
    #sers.send_cmd(CMD_HALL_DELTA, msg)
    #sers.send_cmd(CMD_EEPROM_SAVE)





if __name__ == "__main__":
    if monitor:
        _thread.start_new_thread(dbg_raw, ())
    
    print('start')
    sock.sendto(b'\x00', ('80:00:ff', 1))
    ret, _ = sock.recvfrom(timeout=1)
    if ret:
        print('mdrv ver: ' + re.sub(br'[^\x20-\x7e]',br'.', ret).decode())
    pp = pprint.PrettyPrinter(indent = 4)
    
    print('please input comands:')
    
    while True:

        key_data = input()
        print('input: %s' % key_data)
        
        if key_data.startswith('m'):
            mode = int(key_data[1:])
            print('set mode = %d ...' % mode)
            csa_write(R_state, bytes([mode]))
        
        elif key_data.startswith('reboot'):
            csa_write(R_do_reboot, bytes([1]))
            print('done')
        
        elif key_data.startswith('save_conf'):
            csa_write(R_save_conf, bytes([1]))
            print('done')
            
        elif key_data.startswith('v'):
            s = int(key_data[1:])
            print(f'set speed = {s} ...')
            csa_write(R_tc_speed, struct.pack("<I", s))
            
        elif key_data.startswith('a'):
            s = int(key_data[1:])
            print(f'set accel = {s} ...')
            csa_write(R_tc_accel, struct.pack("<I", s))
            
        elif key_data.startswith('p'):
            p = int(key_data[1:])
            print(f'set output pos = {p} ...')
            qxchg_wr(struct.pack("<i", p))
        
        elif key_data.startswith('d0'):
            csa_write(R_dbg_raw_msk, bytes([0]))
        elif key_data.startswith('d1'):
            csa_write(R_dbg_raw_msk, bytes([dbg_raw_msk]))
            # enable str dbg

