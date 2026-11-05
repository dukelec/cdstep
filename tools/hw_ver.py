#!/usr/bin/env python3
# Software License Agreement (MIT License)
#
# Copyright (c) 2026, DUKELEC, Inc.
# All rights reserved.
#
# Author: Duke Fong <d@d-l.io>

"""CDSTEP hardware version tool

Write the hardware version to the otp, shown as "HW: vX.Y[.Z]" in the info string after reboot.
The otp can not be erased: 9 slots in total, each write takes one, the last one wins.

Args:
  --ver VER           # hardware version, default: 4.2
  --read-only | -r    # only show the slots
  --yes       | -y    # write without asking
  --dev DEV           # specify serial port, default: ttyACM0
  --baud BAUD         # set baudrate, default: 10000000
  --target-addr ADDR  # default: 00:00:fe
  --help    | -h      # this help message
  --verbose | -v      # debug level: verbose
  --debug   | -d      # debug level: debug
  --info    | -i      # debug level: info

Command example:

$ ./hw_ver.py --ver 4.2

"""

HW_VER_ADDR = 0x1fff7008 # same as mdrv_fw / mdrv_bl
HW_VER_SLOTS = 9

import sys, os
import re
import struct

sys.path.append(os.path.join(os.path.dirname(__file__), './pycdnet'))

from cdnet.utils.log import *
from cdnet.utils.cd_args import CdArgs
from cdnet.dev.cdbus_serial import CDBusSerial
from cdnet.dispatch import *

args = CdArgs()
local_mac = int(args.get("--local-mac", dft="0x00"), 0)
dev_str = args.get("--dev", dft="ttyACM0")
baud = int(args.get("--baud", dft="10000000"), 0)
target_addr = args.get("--target-addr", dft="00:00:fe")
ver_str = args.get("--ver", dft="4.2")
read_only = args.get("--read-only", "-r") != None
yes = args.get("--yes", "-y") != None


if args.get("--help", "-h") != None:
    print(__doc__)
    exit()

m = re.fullmatch(r'v?(\d+)\.(\d+)(?:\.(\d+))?', ver_str)
ver = tuple(int(x or 0) for x in m.groups()) if m else None
# 0.0.0 and 255.255.255 can not be told from a zeroed / blank slot
if not ver or max(ver) > 255 or ver in ((0, 0, 0), (255, 255, 255)):
    print(f'invalid version: {ver_str}')
    exit(-1)


if args.get("--verbose", "-v") != None:
    logger_init(logging.VERBOSE)
elif args.get("--debug", "-d") != None:
    logger_init(logging.DEBUG)
elif args.get("--info", "-i") != None:
    logger_init(logging.INFO)


dev = CDBusSerial(dev_str, baud=baud)
CDNetIntf(dev, mac=local_mac)
sock = CDNetSocket(('', 0x40))


def info_read():
    sock.clear()
    sock.sendto(b'', (target_addr, 1))
    ret, _ = sock.recvfrom(timeout=1)
    return ret.decode() if ret else None

def flash_read(addr, len_):
    sock.clear()
    sock.sendto(b'\x00' + struct.pack("<IB", addr, len_), (target_addr, 8))
    ret, _ = sock.recvfrom(timeout=1)
    if not ret or ret[0] != 0 or len(ret) != len_ + 1:
        print(f'flash read error at: 0x{addr:08x}')
        exit(-1)
    return ret[1:]

def flash_write(addr, dat):
    sock.clear()
    sock.sendto(b'\x20' + struct.pack("<I", addr) + dat, (target_addr, 8))
    ret, _ = sock.recvfrom(timeout=1)
    return ret != None and ret[0] == 0

def ver_fmt(v):
    return f'v{v[0]}.{v[1]}.{v[2]}' if v[2] else f'v{v[0]}.{v[1]}'

def read_slots():
    dat = flash_read(HW_VER_ADDR, HW_VER_SLOTS * 8)
    return [dat[i*8:i*8+8] for i in range(HW_VER_SLOTS)]

def show_slots(slots):
    cur, empty = None, None # same rules as get_hw_ver() in the fw
    for i, s in enumerate(slots):
        if s == b'\xff' * 8:
            note = 'blank'
            if empty == None:
                empty = i
        elif s == b'\x00' * 8:
            note = 'zeroed, skipped'
        else:
            note = ver_fmt(s[:3])
            if empty == None:
                cur = tuple(s[:3])
        print(f'  slot {i} @{HW_VER_ADDR + i*8:08x}: {s.hex(" ")}  {note}')
    print(f'current: {ver_fmt(cur) if cur else "none"}')
    return cur, empty


info = info_read()
if not info:
    print(f'no reply from {target_addr}')
    exit(-1)
print(f'info: {info}')

cur, empty = show_slots(read_slots())
if read_only:
    exit()
if cur == ver:
    print(f'already {ver_fmt(ver)}, nothing to write')
    exit()
if empty == None:
    print('no blank slot left')
    exit(-1)

addr = HW_VER_ADDR + empty * 8
if not yes:
    q = f'write {ver_fmt(ver)} to slot {empty} @{addr:08x}, the otp can not be erased, continue? [y/N] '
    if input(q).strip().lower() != 'y':
        exit()

ok = flash_write(addr, bytes(ver) + b'\xff' * 5)
cur, _ = show_slots(read_slots())
if not ok or cur != ver:
    print('write failed')
    exit(-1)
print('done, reboot to show it in the info string')
