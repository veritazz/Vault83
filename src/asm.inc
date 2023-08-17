#ifdef CART_CS_RX /* old ArduboyFX demo platform */
#define CART_PORT    PORTD
#define CART_BIT     PORTD2
#else
#define CART_PORT    PORTD
#define CART_BIT     PORTD1
#endif

#ifdef CONFIG_ASM_OPTIMIZATIONS
	.section .text

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
#endif
