#!/bin/bash
# Copy to the Release dir
arm-none-eabi-gdb -nx --batch \
  -ex 'target extended-remote /dev/ttyACM0' \
  -ex 'monitor swdp_scan' \
  -ex 'attach 1' \
  -ex 'load' \
  -ex 'compare-sections' \
  -ex 'kill' \
  Test99.elf
