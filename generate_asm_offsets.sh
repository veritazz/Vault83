#!/bin/sh

avr-c++ -x c++ -S src/EngineData.offsets -o - | gawk '($1 == "->") { print "#define " $2 " " $3 }' > src/asm_offsets.h
