#!/bin/bash

avr-objdump -t .pio/build/FXDemoPlatform/firmware.elf -C  |sort -k4 | uniq | awk  '{ $5 = sprintf("%d:", "0x" $5) } 1'

