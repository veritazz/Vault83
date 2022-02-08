#!/bin/bash

avr-nm .pio/build/leonardo/firmware.elf -CS --size-sort| sed -rn '/\s[bBdD]\s/p'
