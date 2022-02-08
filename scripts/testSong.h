#ifndef SQUAWK_TEMPLATE_H
#define SQUAWK_TEMPLATE_H

#include "atm_cmd_constants.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#endif

#ifndef NUM_PATTERNS
#define NUM_PATTERNS(struct_) (ARRAY_SIZE( ((struct_ *)0)->patterns_offset))
#endif

#ifndef DEFINE_PATTERN
#define DEFINE_PATTERN(pattern_id, values) const uint8_t pattern_id[] = values;
#endif

#define pattern0_ch1_data { \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_C4, \
	ATM_CMD_M_ARPEGGIO_ON(0xf1, 0x00), \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_ARPEGGIO_OFF, \
	ATM_CMD_I_NOTE_D4, \
	ATM_CMD_M_ARPEGGIO_ON(0xf1, 0x00), \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_ARPEGGIO_OFF, \
	ATM_CMD_I_NOTE_E4, \
	ATM_CMD_M_ARPEGGIO_ON(0xf1, 0x00), \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_ARPEGGIO_OFF, \
	ATM_CMD_I_NOTE_F4, \
	ATM_CMD_M_ARPEGGIO_ON(0xf1, 0x00), \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_ARPEGGIO_OFF, \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_ARPEGGIO_ON(0xf1, 0x00), \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_ARPEGGIO_OFF, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_DELAY_TICKS(12), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_A4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_F4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_F4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_F4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_F4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_E4, \
	ATM_CMD_M_DELAY_TICKS(12), \
	ATM_CMD_I_NOTE_E4, \
	ATM_CMD_M_DELAY_TICKS(12), \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_G4, \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_C4, \
	ATM_CMD_M_DELAY_TICKS_1(162), \
	ATM_CMD_I_RETURN, \
}
DEFINE_PATTERN(pattern0_ch1_array, pattern0_ch1_data);

#define pattern0_ch3_data { \
	ATM_CMD_M_DELAY_TICKS(6), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS(24), \
	ATM_CMD_I_NOTE_A3_, \
	ATM_CMD_M_DELAY_TICKS_1(210), \
	ATM_CMD_I_RETURN, \
}
DEFINE_PATTERN(pattern0_ch3_array, pattern0_ch3_data);

#define pattern1_ch0_data { \
	ATM_CMD_I_NOTE_OFF, \
	ATM_CMD_I_STOP, \
}
DEFINE_PATTERN(pattern1_ch0_array, pattern1_ch0_data);

#define pattern1_ch1_data { \
	ATM_CMD_M_SET_VOLUME(64), \
	ATM_CMD_M_SET_TEMPO(50), \
	ATM_CMD_M_SET_WAVEFORM(0), \
	ATM_CMD_M_CALL(0), \
	ATM_CMD_I_NOTE_OFF, \
	ATM_CMD_I_STOP, \
}
DEFINE_PATTERN(pattern1_ch1_array, pattern1_ch1_data);

#define pattern1_ch2_data { \
	ATM_CMD_I_NOTE_OFF, \
	ATM_CMD_I_STOP, \
}
DEFINE_PATTERN(pattern1_ch2_array, pattern1_ch2_data);

#define pattern1_ch3_data { \
	ATM_CMD_M_SET_VOLUME(64), \
	ATM_CMD_M_SET_TEMPO(50), \
	ATM_CMD_M_SET_WAVEFORM(1), \
	ATM_CMD_M_CALL(1), \
	ATM_CMD_I_NOTE_OFF, \
	ATM_CMD_I_STOP, \
}
DEFINE_PATTERN(pattern1_ch3_array, pattern1_ch3_data);

const PROGMEM struct Squawk_Template_data {
	uint8_t fmt;
	uint8_t num_patterns;
	uint16_t patterns_offset[6];
	uint8_t num_channels;
	uint8_t start_patterns[4];
	uint8_t pattern0[sizeof(pattern0_ch1_array)];
	uint8_t pattern1[sizeof(pattern0_ch3_array)];
	uint8_t pattern2[sizeof(pattern1_ch0_array)];
	uint8_t pattern3[sizeof(pattern1_ch1_array)];
	uint8_t pattern4[sizeof(pattern1_ch2_array)];
	uint8_t pattern5[sizeof(pattern1_ch3_array)];
} Squawk_Template = {
	.fmt = ATM_SCORE_FMT_FULL,
	.num_patterns = NUM_PATTERNS(struct Squawk_Template_data),
	.patterns_offset = {
		offsetof(struct Squawk_Template_data, pattern0),
		offsetof(struct Squawk_Template_data, pattern1),
		offsetof(struct Squawk_Template_data, pattern2),
		offsetof(struct Squawk_Template_data, pattern3),
		offsetof(struct Squawk_Template_data, pattern4),
		offsetof(struct Squawk_Template_data, pattern5),
	},
	.num_channels = 4,
	.start_patterns = {
		2,
		3,
		4,
		5,
	},
	.pattern0 = pattern0_ch1_data,
	.pattern1 = pattern0_ch3_data,
	.pattern2 = pattern1_ch0_data,
	.pattern3 = pattern1_ch1_data,
	.pattern4 = pattern1_ch2_data,
	.pattern5 = pattern1_ch3_data,
};

#endif
