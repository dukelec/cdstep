#!/bin/bash

st-flash --reset write build/mdrv_fw.bin 0x08006800

