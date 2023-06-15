#!/bin/bash

avr-nm .pio/build/FXDemoPlatform/firmware.elf -CS --size-sort| sed -rn '/\s[bBdD]\s/p'
