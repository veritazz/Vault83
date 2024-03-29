#include <Arduino.h>
#include <stdint.h>

const uint8_t hopsPerAngle[91] PROGMEM = {
	 8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
	 8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
	 8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
	 7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
	 7,  7,  6,  6,  6,  6,  6,  6,  6,  6,
	 6,  6,  5,  5,  5,  5,  5,  5,  5,  5,
	 4,  4,  4,  4,  4,  4,  4,  4,  3,  3,
	 3,  3,  3,  3,  3,  3,  2,  2,  2,  2,
	 2,  2,  2,  1,  1,  1,  1,  1,  1,  1,
	 8,
};
const uint16_t stepRaylength[91] PROGMEM = {
	  64,   64,   64,   64,   64,   64,   64,   64,   65,   65,
	  65,   65,   65,   66,   66,   66,   67,   67,   67,   68,
	  68,   69,   69,   70,   70,   71,   71,   72,   72,   73,
	  74,   75,   75,   76,   77,   78,   79,   80,   81,   82,
	  84,   85,   86,   88,   89,   91,   92,   94,   96,   98,
	 100,  102,  104,  106,  109,  112,  114,  118,  121,  124,
	 128,  132,  136,  141,  146,  151,  157,  164,  171,  179,
	 187,  197,  207,  219,  232,  247,  265,  285,  308,  335,
	 369,  409,  460,  525,  612,  734,  917, 1223, 1834, 3667,
	  64,
};

const int8_t cosByX[360] PROGMEM = {
	  64,   64,   64,   64,   64,   64,   64,   64,   63,   63,
	  63,   63,   63,   62,   62,   62,   62,   61,   61,   61,
	  60,   60,   59,   59,   58,   58,   58,   57,   57,   56,
	  55,   55,   54,   54,   53,   52,   52,   51,   50,   50,
	  49,   48,   48,   47,   46,   45,   44,   44,   43,   42,
	  41,   40,   39,   39,   38,   37,   36,   35,   34,   33,
	  32,   31,   30,   29,   28,   27,   26,   25,   24,   23,
	  22,   21,   20,   19,   18,   17,   15,   14,   13,   12,
	  11,   10,    9,    8,    7,    6,    4,    3,    2,    1,
	   0,   -1,   -2,   -3,   -4,   -6,   -7,   -8,   -9,  -10,
	 -11,  -12,  -13,  -14,  -15,  -17,  -18,  -19,  -20,  -21,
	 -22,  -23,  -24,  -25,  -26,  -27,  -28,  -29,  -30,  -31,
	 -32,  -33,  -34,  -35,  -36,  -37,  -38,  -39,  -39,  -40,
	 -41,  -42,  -43,  -44,  -44,  -45,  -46,  -47,  -48,  -48,
	 -49,  -50,  -50,  -51,  -52,  -52,  -53,  -54,  -54,  -55,
	 -55,  -56,  -57,  -57,  -58,  -58,  -58,  -59,  -59,  -60,
	 -60,  -61,  -61,  -61,  -62,  -62,  -62,  -62,  -63,  -63,
	 -63,  -63,  -63,  -64,  -64,  -64,  -64,  -64,  -64,  -64,
	 -64,  -64,  -64,  -64,  -64,  -64,  -64,  -64,  -63,  -63,
	 -63,  -63,  -63,  -62,  -62,  -62,  -62,  -61,  -61,  -61,
	 -60,  -60,  -59,  -59,  -58,  -58,  -58,  -57,  -57,  -56,
	 -55,  -55,  -54,  -54,  -53,  -52,  -52,  -51,  -50,  -50,
	 -49,  -48,  -48,  -47,  -46,  -45,  -44,  -44,  -43,  -42,
	 -41,  -40,  -39,  -39,  -38,  -37,  -36,  -35,  -34,  -33,
	 -32,  -31,  -30,  -29,  -28,  -27,  -26,  -25,  -24,  -23,
	 -22,  -21,  -20,  -19,  -18,  -17,  -15,  -14,  -13,  -12,
	 -11,  -10,   -9,   -8,   -7,   -6,   -4,   -3,   -2,   -1,
	   0,    1,    2,    3,    4,    6,    7,    8,    9,   10,
	  11,   12,   13,   14,   15,   17,   18,   19,   20,   21,
	  22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
	  32,   33,   34,   35,   36,   37,   38,   39,   39,   40,
	  41,   42,   43,   44,   44,   45,   46,   47,   48,   48,
	  49,   50,   50,   51,   52,   52,   53,   54,   54,   55,
	  55,   56,   57,   57,   58,   58,   58,   59,   59,   60,
	  60,   61,   61,   61,   62,   62,   62,   62,   63,   63,
	  63,   63,   63,   64,   64,   64,   64,   64,   64,   64,
};

const uint16_t fineCosByX[91] PROGMEM = {
	65535, 65526, 65496, 65446, 65376, 65287, 65177, 65048, 64898, 64729,
	64540, 64332, 64104, 63856, 63589, 63303, 62997, 62672, 62328, 61966,
	61584, 61183, 60764, 60326, 59870, 59396, 58903, 58393, 57865, 57319,
	56756, 56175, 55578, 54963, 54332, 53684, 53020, 52339, 51643, 50931,
	50203, 49461, 48703, 47930, 47143, 46341, 45525, 44695, 43852, 42995,
	42126, 41243, 40348, 39441, 38521, 37590, 36647, 35693, 34729, 33754,
	32768, 31772, 30767, 29753, 28729, 27697, 26656, 25607, 24550, 23486,
	22415, 21336, 20252, 19161, 18064, 16962, 15855, 14742, 13626, 12505,
	11380, 10252, 9121, 7987, 6850, 5712, 4572, 3430, 2287, 1144,
	   0,
};

const uint16_t tanByX[90] PROGMEM = {
	   0,    1,    2,    3,    4,    6,    7,    8,    9,   10,
	  11,   12,   14,   15,   16,   17,   18,   20,   21,   22,
	  23,   25,   26,   27,   28,   30,   31,   33,   34,   35,
	  37,   38,   40,   42,   43,   45,   46,   48,   50,   52,
	  54,   56,   58,   60,   62,   64,   66,   69,   71,   74,
	  76,   79,   82,   85,   88,   91,   95,   99,  102,  107,
	 111,  115,  120,  126,  131,  137,  144,  151,  158,  167,
	 176,  186,  197,  209,  223,  239,  257,  277,  301,  329,
	 363,  404,  455,  521,  609,  732,  915, 1221, 1833, 3667,
};

/*
 * table to support drawing 1-16 pixels at once
 */
const uint8_t pixel_patterns[768] PROGMEM = {
/*             |        shift by 0     |        shift by 1     |        shift by 2     |        shift by 3     |        shift by 4     |        shift by 5     |        shift by 6     |        shift by 7     | */
/*  1 pixel  */	 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
/*  2 pixels */	 0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00,
/*  3 pixels */	 0x07, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0x80, 0x03, 0x00, 0x00,
/*  4 pixels */	 0x0f, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0x80, 0x07, 0x00, 0x00,
/*  5 pixels */	 0x1f, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0xe0, 0x03, 0x00, 0x00, 0xc0, 0x07, 0x00, 0x00, 0x80, 0x0f, 0x00, 0x00,
/*  6 pixels */	 0x3f, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0xf8, 0x01, 0x00, 0x00, 0xf0, 0x03, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xc0, 0x0f, 0x00, 0x00, 0x80, 0x1f, 0x00, 0x00,
/*  7 pixels */	 0x7f, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0xfc, 0x01, 0x00, 0x00, 0xf8, 0x03, 0x00, 0x00, 0xf0, 0x07, 0x00, 0x00, 0xe0, 0x0f, 0x00, 0x00, 0xc0, 0x1f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00,
/*  8 pixels */	 0xff, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x00, 0x00, 0xfc, 0x03, 0x00, 0x00, 0xf8, 0x07, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xc0, 0x3f, 0x00, 0x00, 0x80, 0x7f, 0x00, 0x00,
/*  9 pixels */	 0xff, 0x01, 0x00, 0x00, 0xfe, 0x03, 0x00, 0x00, 0xfc, 0x07, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0x00, 0xf0, 0x1f, 0x00, 0x00, 0xe0, 0x3f, 0x00, 0x00, 0xc0, 0x7f, 0x00, 0x00, 0x80, 0xff, 0x00, 0x00,
/* 10 pixels */	 0xff, 0x03, 0x00, 0x00, 0xfe, 0x07, 0x00, 0x00, 0xfc, 0x0f, 0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0xe0, 0x7f, 0x00, 0x00, 0xc0, 0xff, 0x00, 0x00, 0x80, 0xff, 0x01, 0x00,
/* 11 pixels */	 0xff, 0x07, 0x00, 0x00, 0xfe, 0x0f, 0x00, 0x00, 0xfc, 0x1f, 0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00, 0xf0, 0x7f, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00, 0xc0, 0xff, 0x01, 0x00, 0x80, 0xff, 0x03, 0x00,
/* 12 pixels */	 0xff, 0x0f, 0x00, 0x00, 0xfe, 0x1f, 0x00, 0x00, 0xfc, 0x3f, 0x00, 0x00, 0xf8, 0x7f, 0x00, 0x00, 0xf0, 0xff, 0x00, 0x00, 0xe0, 0xff, 0x01, 0x00, 0xc0, 0xff, 0x03, 0x00, 0x80, 0xff, 0x07, 0x00,
/* 13 pixels */	 0xff, 0x1f, 0x00, 0x00, 0xfe, 0x3f, 0x00, 0x00, 0xfc, 0x7f, 0x00, 0x00, 0xf8, 0xff, 0x00, 0x00, 0xf0, 0xff, 0x01, 0x00, 0xe0, 0xff, 0x03, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x80, 0xff, 0x0f, 0x00,
/* 14 pixels */	 0xff, 0x3f, 0x00, 0x00, 0xfe, 0x7f, 0x00, 0x00, 0xfc, 0xff, 0x00, 0x00, 0xf8, 0xff, 0x01, 0x00, 0xf0, 0xff, 0x03, 0x00, 0xe0, 0xff, 0x07, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x80, 0xff, 0x1f, 0x00,
/* 15 pixels */	 0xff, 0x7f, 0x00, 0x00, 0xfe, 0xff, 0x00, 0x00, 0xfc, 0xff, 0x01, 0x00, 0xf8, 0xff, 0x03, 0x00, 0xf0, 0xff, 0x07, 0x00, 0xe0, 0xff, 0x0f, 0x00, 0xc0, 0xff, 0x1f, 0x00, 0x80, 0xff, 0x3f, 0x00,
/* 16 pixels */	 0xff, 0xff, 0x00, 0x00, 0xfe, 0xff, 0x01, 0x00, 0xfc, 0xff, 0x03, 0x00, 0xf8, 0xff, 0x07, 0x00, 0xf0, 0xff, 0x0f, 0x00, 0xe0, 0xff, 0x1f, 0x00, 0xc0, 0xff, 0x3f, 0x00, 0x80, 0xff, 0x7f, 0x00,
/* 17 pixels */	 0xff, 0xff, 0x01, 0x00, 0xfe, 0xff, 0x03, 0x00, 0xfc, 0xff, 0x07, 0x00, 0xf8, 0xff, 0x0f, 0x00, 0xf0, 0xff, 0x1f, 0x00, 0xe0, 0xff, 0x3f, 0x00, 0xc0, 0xff, 0x7f, 0x00, 0x80, 0xff, 0xff, 0x00,
/* 18 pixels */	 0xff, 0xff, 0x03, 0x00, 0xfe, 0xff, 0x07, 0x00, 0xfc, 0xff, 0x0f, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0xf0, 0xff, 0x3f, 0x00, 0xe0, 0xff, 0x7f, 0x00, 0xc0, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x01,
/* 19 pixels */	 0xff, 0xff, 0x07, 0x00, 0xfe, 0xff, 0x0f, 0x00, 0xfc, 0xff, 0x1f, 0x00, 0xf8, 0xff, 0x3f, 0x00, 0xf0, 0xff, 0x7f, 0x00, 0xe0, 0xff, 0xff, 0x00, 0xc0, 0xff, 0xff, 0x01, 0x80, 0xff, 0xff, 0x03,
/* 20 pixels */	 0xff, 0xff, 0x0f, 0x00, 0xfe, 0xff, 0x1f, 0x00, 0xfc, 0xff, 0x3f, 0x00, 0xf8, 0xff, 0x7f, 0x00, 0xf0, 0xff, 0xff, 0x00, 0xe0, 0xff, 0xff, 0x01, 0xc0, 0xff, 0xff, 0x03, 0x80, 0xff, 0xff, 0x07,
/* 21 pixels */	 0xff, 0xff, 0x1f, 0x00, 0xfe, 0xff, 0x3f, 0x00, 0xfc, 0xff, 0x7f, 0x00, 0xf8, 0xff, 0xff, 0x00, 0xf0, 0xff, 0xff, 0x01, 0xe0, 0xff, 0xff, 0x03, 0xc0, 0xff, 0xff, 0x07, 0x80, 0xff, 0xff, 0x0f,
/* 22 pixels */	 0xff, 0xff, 0x3f, 0x00, 0xfe, 0xff, 0x7f, 0x00, 0xfc, 0xff, 0xff, 0x00, 0xf8, 0xff, 0xff, 0x01, 0xf0, 0xff, 0xff, 0x03, 0xe0, 0xff, 0xff, 0x07, 0xc0, 0xff, 0xff, 0x0f, 0x80, 0xff, 0xff, 0x1f,
/* 23 pixels */	 0xff, 0xff, 0x7f, 0x00, 0xfe, 0xff, 0xff, 0x00, 0xfc, 0xff, 0xff, 0x01, 0xf8, 0xff, 0xff, 0x03, 0xf0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x0f, 0xc0, 0xff, 0xff, 0x1f, 0x80, 0xff, 0xff, 0x3f,
/* 24 pixels */	 0xff, 0xff, 0xff, 0x00, 0xfe, 0xff, 0xff, 0x01, 0xfc, 0xff, 0xff, 0x03, 0xf8, 0xff, 0xff, 0x07, 0xf0, 0xff, 0xff, 0x0f, 0xe0, 0xff, 0xff, 0x1f, 0xc0, 0xff, 0xff, 0x3f, 0x80, 0xff, 0xff, 0x7f,
};
