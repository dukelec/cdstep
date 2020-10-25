#!/bin/bash

st-flash --reset write build/linear_motion.bin 0x08000000

