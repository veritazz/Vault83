#ifndef __DRAWING_H
#define __DRAWING_H

#ifndef __ASSEMBLER__

#include "EngineData.h"

#ifdef CONFIG_ASM_OPTIMIZATIONS

extern "C" void drawTexel(const uint8_t *p, unsigned char *buffer, uint8_t tmp_px, uint8_t color);
extern "C" void drawAll(int16_t screenY, uint16_t scale, uint16_t wallHeight, struct renderInfo *re);
extern "C" void drawTextureColumnScaleUp2(int16_t screenY, uint8_t wallHeight, uint8_t px_base, struct renderInfo *re);

#endif

#else
#endif

#endif
