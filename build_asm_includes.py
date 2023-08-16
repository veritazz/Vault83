Import("env")

print("building ASM includes")

cmd = [
  "$CXX",
  "$_CCCOMCOM -I$PROJECT_LIBDEPS_DIR/$PIOENV/ArduboyFX/src -mmcu=atmega32u4 -Wall -I$PROJECT_PACKAGES_DIR/framework-arduino-avr/libraries/EEPROM/src -Ilib/Arduboy2/src -x c++ -S src/EngineData.offsets -o - | gawk '($1 == \"->\") { print \"#define \" $2 \" \" $3 }' > src/asm_offsets.h"
]

env.Execute(" ".join(cmd))
