#ifndef __FLASH_H
#define __FLASH_H

#ifndef __ASSEMBLER__

#include "EngineData.h"

#ifdef CONFIG_ASM_OPTIMIZATIONS

extern "C" void dflash_seek(const uint24_t a);

#endif

#else
#endif

#endif
