#include "flash.h"
#include "asm_offsets.h"
#include "asm.inc"

#include <avr/io.h>

; free to use
;   r18:r27
;   r0
;   r30:r31

; Z = 31:30
; Y = 29:28
; X = 27:26

#ifdef CONFIG_ASM_OPTIMIZATIONS
	.section .text
	.global dflash_seek
;
; Seek to an address in the data section and start reading the
; first byte
;
; parameters passed
;
; flash address = 24bit flash address
;
; r24:r23:r22 = flash address
;
dflash_seek:
	;
	; select the flash
	;
	cbi _SFR_IO_ADDR(CART_PORT), CART_BIT        ; (2) select flash

	;
	; send SFC_READ
	;
	ldi r25, 0x3                               ; (1) load flash read command
	out 0x2e, r25                              ; (1) send read command

	;
	; calc address
	;
	lds r20, addr_programDataPage              ; (2) load low byte of flash offset
	lds r21, addr_programDataPage+1            ; (2) load high byte of flash offset

	add     r23, r20            ; (1) add offset low byte to flash address byte 2
	adc     r24, r21            ; (1) adc offset high byte to flash address byte 3

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
	in r0, 0x2d              ; (1) clear SPSR

	out 0x2e, r24            ; (1) send addr[2]
	burn17

	out 0x2e, r23            ; (1) send addr[1]
	burn17

	out 0x2e, r22            ; (1) send addr[0]
	burn17

	out 0x2e, r1             ; (1) send 0 to read first byte at addr

	ret
#endif
