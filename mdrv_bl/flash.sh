#!/bin/sh

st-flash --reset write build/mdrv_bl.bin 0x08000000

