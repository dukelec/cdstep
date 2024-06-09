#!/bin/sh

st-flash --reset --format ihex write build/*.hex


# st-flash --area=option read
# 0xfffffeaa -> 0xf7fffeaa (nrst as io)
# st-flash --area=option write 0xf7fffeaa


# bit[28:27] => 2'b10: nrst as io

