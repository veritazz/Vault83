#ifndef __TABLES_H
#define __TABLES_H

#include <stdint.h>

extern const int8_t cosByX[360];
extern const uint16_t fineCosByX[91];
extern const uint16_t tanByX[90];
extern const uint16_t stepRaylength[91];
extern const uint8_t hopsPerAngle[91];
extern const uint8_t pixel_patterns[768];

#define DIVIDE_BY_X           64
#define FINE_BY_X             65536
#define FOG_OF_WAR_DISTANCE   512



#endif