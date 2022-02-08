#ifndef __MAP_H
#define __MAP_H

#ifndef __ASSEMBLER__

#ifdef CONFIG_ASM_OPTIMIZATIONS

extern "C" uint8_t checkIgnoreBlock(uint8_t mapX, uint8_t mapY);
extern "C" uint8_t readThroughCache2(uint8_t mapX, uint8_t mapY, unsigned char *cache);

#endif

#else
#endif

#endif
