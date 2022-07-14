#include "map.h"
#include "asm_offsets.h"
#include "leveldata.h"

#include <avr/io.h>

#ifdef CART_CS_RX /* old ArduboyFX demo platform */
#define CART_PORT    PORTD
#define CART_BIT     PORTD2
#else
#define CART_PORT    PORTD
#define CART_BIT     PORTD1
#endif

; free to use
;   r18:r27
;   r0
;   r30:r31

; Z = 31:30
; Y = 29:28
; X = 27:26

.extern levelFlashOffset

	.section .text

#ifdef CONFIG_ASM_OPTIMIZATIONS


	.macro burn17
;---------------------------------------------------------------
;
; 17 cycles to burn, done in a way so that this does not burn
; more than 17 cycles if nothing happens
;
;---------------------------------------------------------------
 
	nop                      ; (1) just burn 1 cycle
	in r0, 0x2d              ; (1) read SPSR
	sbrs r0, 7               ; (1/2) check if transfer is complete
	rjmp 1f                  ; (2) transfer not complete, jump to next attempt
	rjmp 4f                  ; (2) transfer complete, jump to end
1:
	in r0, 0x2d              ; (1) read SPSR
	sbrs r0, 7               ; (1/2) check if transfer is complete
	rjmp 1f                  ; (2) transfer not complete, jump to next attempt
	rjmp 4f                  ; (2) transfer complete, jump to end
1:
	in r0, 0x2d              ; (1) read SPSR
	sbrs r0, 7               ; (1/2) check if transfer is complete
	rjmp 1f                  ; (2) transfer not complete, jump to next attempt
	rjmp 4f                  ; (2) transfer complete, jump to end
1:
	in r0, 0x2d              ; (1) read SPSR
	sbrs r0, 7               ; (1/2) check if transfer is complete
	rjmp 4f                  ; (2) transfer not complete, just jump to burn 2 cycles
4:
	in r0, 0x2d              ; (1) clear SPSR
5:
	.endm


	.global checkIgnoreBlock

;
; r25:r24 = mapX, r23:r22 = mapY
;
checkIgnoreBlock:
	;
	; select the flash
	;
	cbi _SFR_IO_ADDR(CART_PORT), CART_BIT        ; (2) select flash

	;
	; send SFC_READ
	;
	ldi r25, 0x3                                 ; (1) load flash read command
	out 0x2e, r25                                ; (1) send read command

	;
	; precalc (programDataPage << 8) + levelFlashOffset
	;
	;
	lds r23, levelFlashOffset + 2               ; (2) load addr[2]

	;
	; 24bit flash address
	;
	; [programDataPage[1] + carry]
	; [programDataPage[0] + ]
	;
	;
	; MAP_WIDTH * r22 + r24
	ldi r25, MAP_WIDTH                   ; (1) load map width into register
	mul r25, r22                         ; (2) mapY * map width
	add r0, r24                          ; (1) add mapX
	movw r24, r0                         ; (1) mov r1:r0 to r25:r24
	eor r1, r1                           ; (1) zero r1
	adc r25, r1                          ; (1) add carry from (add mapX)

	lds r22, levelFlashOffset + 0       ; (2) load addr[0]
	add r24, r22                         ; (1) add offset low byte
	lds r22, levelFlashOffset + 1       ; (2) load addr[1]
	adc r25, r22                         ; (1) add offset high byte + carry

	nop                                  ; (1) burn 1 cycle
	nop                                  ; (1) burn 1 cycle
	;
	; wait
	;
1:
	in r0, 0x2d              ; (1) clear SPSR

	out 0x2e, r23            ; (1) send addr[2]
	burn17

	out 0x2e, r25            ; (1) send addr[1]
	burn17

	out 0x2e, r24            ; (1) send addr[0]
	burn17

	out 0x2e, r1             ; (1) send 0 to read first byte at addr
	burn17

	;
	; deselect the flash
	;
	sbi _SFR_IO_ADDR(CART_PORT), CART_BIT          ; (2)
	;
	; return SPDR
	;
	in r24, 0x2e
	ret


;--------------------------------------------------------------------------------------------------

	.global readThroughCache2

#define CACHE_ROWS                8
#define CACHE_COLUMNS             16
;
; r25:r24 = mapX, r23:r22 = mapY, r21:r20 = cache
;
readThroughCache2:
	;
	; select the flash
	;
	cbi _SFR_IO_ADDR(CART_PORT), CART_BIT        ; (2)

	;
	; send SFC_READ
	;
	ldi r25, 0x3              ; (1)
	out 0x2e, r25             ; (1)

	nop
	;
	; precalc (programDataPage << 8) + levelFlashOffset
	;
	;
	lds r23, levelFlashOffset + 2     ; (2)

; 14 free
	movw r30, r20             ; (1) move cache into Z

	;uint8_t cacheRow = mapX % CACHE_ROWS;
	mov r21, r24              ; (1) copy mapX
	andi r24, CACHE_ROWS-1      ; (1)

	;uint8_t cacheColumn = mapY % CACHE_COLUMNS;
	mov r20, r22              ; (1) copy mapY
	andi r22, CACHE_COLUMNS-1   ; (1)

	;/* calculate cache offset */
	;uint16_t cacheOffset = ((uint16_t)cacheRow * (CACHE_COLUMNS * sizeof(cache_entry))) + (cacheColumn * sizeof(cache_entry));

	; o = cr * a * 4 + cc * 4 = 4(a)(cr) + 4(cc) = 4(a*cr+cc)
	ldi r25, CACHE_COLUMNS    ; (1)
	mul r25, r24              ; (2)
	add r0, r22              ; (1)
	ldi r25, 4                ; (1)
	mul r0, r25               ; (2)
	add r30, r0              ; (1)
	adc r31, r1              ; (1)
; 0 free
	;
	; send byte to SPI
	;
;	in r0, 0x2d              ; (1)
	out 0x2e, r23            ; send addr[2]
	;
	; 24bit flash address
	;
	; [programDataPage[1] + carry]
	; [programDataPage[0] + ]
	;
	;
	; MAP_WIDTH * r22 + r24
	ldi r23, MAP_WIDTH                   ; (1)
	mul r23, r20                         ; (2)
	add r0, r21                          ; (1)
	movw r24, r0                         ; (1)
	eor r1, r1                           ; (1)
	adc r25, r1                          ; (1)

	lds r22, levelFlashOffset + 0       ; (2)
	add r22, r24                         ; (1)
	lds r23, levelFlashOffset + 1       ; (2)
	adc r23, r25                         ; (1)

	ld r24, Z                 ; (2) load cache entry mapX

	nop
	nop
	;
	; send byte to SPI
	;
;	in r0, 0x2d              ; (1) read SPSR to clear SPIF
	out 0x2e, r23            ; send addr[1]
; 16 free

	cp r24, r21               ; (1)
	brne 3f                  ; (1/2)
	ldd r24, Z+1              ; (2) load cache entry mapY
	cp r24, r20               ; (1)
	brne 7f                  ; (1/2)
	; found it in cache, load tile from cache
	ldd r24, Z+2             ; (2)

	;
	; wait until flash command finished before deselecting the chip
	;
1:
	nop
	in r0, 0x2d              ; (1)
	sbrs r0, 7               ; (1/2)
	rjmp 1b                  ; (2)
	;
	; deselect the flash
	;
	sbi _SFR_IO_ADDR(CART_PORT), CART_BIT          ; (2)

	ret                       ; (4)

	; TODO continue to read from flash
; 13 free
3:
	nop
	nop
	nop
	nop
; 9 free
7:
	; Z contains pointer to cache entry
	st  Z, r21                ; (2) store new mapX
	std Z+1, r20              ; (2) store new mapY
	nop
	nop
	nop
	nop
	nop

	nop

	;
	; send byte to SPI
	;
;	in r0, 0x2d              ; (1)
	out 0x2e, r22            ; send addr[0]

; 16 free
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
1:
;	in r0, 0x2d              ; (1)
;	sbrs r0, 7               ; (1/2)
;	rjmp 1b                  ; (2)

	;
	; send byte to SPI
	;
	out 0x2e, r1             ; send 0 to read first byte at addr

; 16 free
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop
nop

	;
	; wait
	;
1:
	in r0, 0x2d
;	sbrs r0, 7
;	rjmp 1b

	;
	; deselect the flash
	;
	sbi _SFR_IO_ADDR(CART_PORT), CART_BIT          ; (2)
	;
	; read SPDR
	;
	in r24, 0x2e

	std Z+2, r24              ; (2) store new tile

	ret

#endif
