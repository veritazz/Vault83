#!/bin/bash

avr-objdump -sD .pio/build/leonardo/firmware.elf > firmware.S

