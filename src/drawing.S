;
; vim: syntax=avr
;
#include "drawing.h"
#include "asm_offsets.h"

	.section .text
; free to use
;   r18:r27
;   r0
;   r30:r31

; Z = 31:30
; Y = 29:28
; X = 27:26

; tmp_px
; > 16px = 4 bytes (8 * 4 = 32)
; > 8px  = 3 bytes (8 * 3 = 24)
; > 1px  = 2 bytes (8 * 2 = 16)
;   1px  = 1 bytes (8 * 1 = 8)
;
;   1   1
; ldi (1), ldi (1), adiw (2), lpm (3)  = 7

#ifdef CONFIG_ASM_OPTIMIZATIONS

	.global drawTexel
;
; parameters passed
;
; p      = progmem pointer to pixel table
; buffer = buffer to screen column
; tmp_px = number of pixels to draw
; color  = color to draw (0 = black, else white)
;
; r25:r24 = p, r23:r22 = buffer, r21:r20 = tmp_px, r19:r18 = color
;
drawTexel:
	movw r26, r22     ; (1) load buffer into X
	movw r30, r24     ; (1) load p into Z

	; always 1 byte to draw, so load values here
	; --- 1 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer

	; check if color is 0 (black)
	and r18, r18      ; (1) test if color is zero
	breq _draw_black  ; (1, 2) branch to draw_black code

	; draw 1st byte
	or  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X

	; it tmp_px is one, we are done
	cpi r20, 1
	breq _done  ; jump to 1 byte (2)

	; check if tmp_px is lower than 8
	cpi r20, 8 ; 2
	brlo _draw_use_2_bytes ; 7 to 2 pixel to draw
	; check if tmp_px is lower than 8
	cpi r20, 16 ; 9
	brlo _draw_use_3_bytes ; 15 to 8 pixel to draw

	; more than 15 pixel to draw

_draw_use_4_bytes:
	; --- 4 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	or  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
_draw_use_3_bytes:
	; --- 3 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	or  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
_draw_use_2_bytes:
	; --- 2 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	or  r19, r21     ; (1) or *p to *buffer
	st  X, r19      ; (2) store r19 to *buffer, inc X

_done:
	ret

_draw_black:

	; draw 1st byte
	com r19
	and  r19, r21    ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X

	; it tmp_px is one, we are done
	cpi r20, 1
	breq _done

	; check if tmp_px is lower than 8
	cpi r20, 8 ; 2
	brlo _draw_0_use_2_bytes ; 7 to 2 pixel to draw
	; check if tmp_px is lower than 8
	cpi r20, 16 ; 9
	brlo _draw_0_use_3_bytes ; 15 to 8 pixel to draw

	; more than 15 pixel to draw

_draw_0_use_4_bytes:
	; --- 4 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	com r19
	and  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
_draw_0_use_3_bytes:
	; --- 3 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	com r19
	and  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
_draw_0_use_2_bytes:
	; --- 2 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	com r19
	and  r19, r21     ; (1) or *p to *buffer
	st  X, r19      ; (2) store r19 to *buffer, inc X

	; exit
	ret


; free to use
;   r18:r27
;   r0
;   r30:r31
;   r1 = always zero

; Z = 31:30
; Y = 29:28
; X = 27:26
;----------------------------------------------------------------
	.global drawAll
;
; parameters passed
;
; r25:r24 = screenY(16bit), r23:r22 = scale, r21:r20 = wallHeight
drawAll:
	push r12              ; (2)
	push r13              ; (2)
	push r16              ; (2)
	push r17              ; (2)
	push r28              ; (2)
	push r29              ; (2)

	eor r16, r16          ; (1) set texY lo to zero
	eor r17, r17          ; (1) set texY hi to zero

	;
	; assumption that screenY is >= -255
	;
	tst r25               ; (1) test if screenY is < 0
	brpl 1f               ; (1/2)
	; screenY is < 0
	neg r24               ; (1) abs((uint8_t)screenY)

	; reduce wallHeight by screenY
	sub r20, r24          ; (1) wh - screenY
	sbc r21, r1           ; (1) ...

	mul r24, r22          ; (2) multiply lsb of scale with abs(screenY)
	movw r16, r0          ; (1) move r1:r0 result to r17:r16
	mul r24, r23          ; (2) multiply msb of scale with abs(screenY)
	add r17, r0           ; (1) add lsb of result to msb of previous result


	eor r24, r24          ; (1) set nScreenY = 0
	eor r1, r1            ; (1) r1 = 0
	; -------------------------------------------------
1:      ; screenY is >= 0
	; -------------------------------------------------
	; a = SCREEN_HEIGHT - yscreen
	ldi r26, (64-8)       ; (1) load SCREEN_HEIGHT into r26
	sub r26, r24          ; (1) a = SCREEN_HEIGHT - yscreen

	; if wallHeight > a: wallHeight = a
	cp r26, r20           ; (1) a - wh
	cpc r1, r21           ; (1)
	brsh 1f               ; (1/2)

	mov r20, r26          ; (1) wh = a, r21 don't care

1:
; TODO stencilbuffer is screenColumn+8
	; buffer = es.screenColumn + nScreenY / 8;
	ldi r30, lo8(es + offsetof_engineState_screenColumn)    ; (2)
	ldi r31, hi8(es + offsetof_engineState_screenColumn)    ; (2)
	mov r0, r24           ; (1) r0 = nScreenY
	lsr r0                ; (1) nScreenY / 8
	lsr r0                ; (1) ...
	lsr r0                ; (1) ...
	add r30, r0           ; (1)
			      ; adc not required as screenColumn is aligned to an 8 byte boundary


	; /* bit position on the screen */
	; bit = bitshift_left[nScreenY % 8];
	ldi r26, lo8(bitshift_left)   ; (2)
	ldi r27, hi8(bitshift_left)   ; (2)
	mov r25, r24          ; (1) move nScreenY to r25
	andi r25, 0x07        ; (1) nScreenY % 8
	add r26, r25          ; (1) bitshift_left + (nScreenY % 8), no adc required (aligend to 8 byte)
	ld r13, X             ; (2) load from (bitshift_left + (nScreenY % 8))
			      ;     r13 = bit

	mov r25, r17          ; (1) texOffsetOld = hi8(texY) & 0xc0
	andi r25, 0xc0        ; (1) r25 is texOffsetOld
	; uint8_t texOffset = (texOffsetOld >> (TEXTURE_SCALE_SHIFT + 3));
	mov r0, r25           ; (1) r0 is texOffsetOld (hi8(texY) & 0xc0)
	swap r0               ; (1) r0 >> 6 (upper 2 bits moved to lower 2 bits)
	lsr r0                ; (1) ...
	lsr r0                ; (1) ...

	; not required	andi r0, 0x3
	; // texoffset can be added to textureptr without adc (pointer is aligned to 8 byte)
	; uint8_t texColumn = texturePtr[texOffset];
	;
	ldi r28, lo8(es + offsetof_engineState_texColumn)      ; (2)
	ldi r29, hi8(es + offsetof_engineState_texColumn)      ; (2)
	add r28, r0            ; (1) add texOffset
	ldd r0, Y+4            ; (2) load texture byte
	ld r12, Y              ; (2) load mask byte

	eor r18, r18           ; (1) mask = 0 (mask to draw black pixel)
	eor r19, r19           ; (1) v = 0 (mask to draw white pixel)

	; ------------------------------------------
	; while (wallHeight--)
	; ...
1:
;
; check if stencilbuffer at r13 is clear
;
ldd r26, Z+8
and r26, r13
brne 3f
	; /* bit position on the texture */
	; uint8_t bit_pos = (texY >> TEXTURE_SCALE_SHIFT) % 8;
	mov r21, r17          ; (1) r21 = hi8(texY)
	lsl r21               ; (1) (r21 >> 3) & 0x7
	swap r21              ; (1) ...
	andi r21, 0x7         ; (1) ... (r21 is bit_pos)
	; /* read texture bit and check texColumn for this bit */
	ldi r26, lo8(bitshift_left)  ; (2) r27 (hi8(bitshift_left) is still valid
	add r26, r21          ; (1) add bit_pos (adc not required as bitshift_left is aligned to 8 byte)
	ld r26, X             ; (2) bitshift_left[bit_pos]
	mov r21, r26          ; (1)
	and r26, r12          ; (1) mask & bitshift_left[bit_pos]
	breq 3f               ; (1/2) skip if mask says no
	and r21, r0           ; (1) texColumn & bitshift_left[bit_pos]
	breq 2f               ; (1/2) if zero (black)
	or r19, r13           ; (1) v |= bit (white pixel)
2:
	or r18, r13           ; (1) mask |= bit (black pixel)
3:
	lsl r13               ; (1) bit <<= 1
	brne  4f              ; (1/2) branch if not zero (still a bit set) TODO optimize for not jumping
; also update the stencil buffer
ldd r26, Z+8
or r26, r18
or r26, r19
std Z+8, r26

	ld r26, Z             ; (2) *buffer
	com r18               ; (1) ~mask
	and r26, r18          ; (1) *buffer & ~mask
	or r26, r19           ; (1) *buffer++ | v
	st Z+, r26            ; (2) ...
	eor r18, r18          ; (1) mask = 0
	eor r19, r19          ; (1) v = 0
	inc r13               ; (1) bit = 1
4:
	add r16, r22          ; (1) texY += scale
	adc r17, r1           ; (1) ...
	add r17, r23          ; (1) ...

	; // texY(high) andi 0xc0
	; texY(high) = r17
	; texOffsetOld = r25
	; if ((texY & 0xc000) != texOffsetOld) {
	;	texOffsetOld = texY & 0xc000;
	;	texOffset = (texOffsetOld >> (TEXTURE_SCALE_SHIFT + 3));
	; // texY(high) swap, lsr, lsr, (andi)
	; // if texturePtr is 4 byte aligned, adc is not needed
	;	texColumn = texturePtr[texOffset];
	; }
	; compare the higher 2 bits of hi8(texY) and texOffsetOld (shift is 11+3, so only the upper 2 bits count)
	mov r26, r17          ; (1) r26 = hi8(texY)
	andi r26, 0xc0        ; (1) r26 &= 0xc0
	cp r26, r25           ; (1) compare texY (high) and texOffsetOld
	breq  5f              ; (1/2) if equal, texColumn is still ok  TODO optimize for not jumping
			      ;       else new texColumn needs to be loaded
	mov r0, r26           ; (1) r0 = hi8(texY) & 0xc0
	sub r0, r25           ; (1) sub texOffsetOld from r0
	mov r25, r26          ; (1) texOffsetOld = hi8(texY)
	swap r0               ; (1) r0 >>= 6
	lsr r0                ; (1) ...
	lsr r0                ; (1) r0 = texOffset

	add r28, r0           ; (1) add to lo8(texturePtr)
			      ;     no adc because texturePtr is 8 byte aligned
	ldd r0, Y+4           ; (2) load next texture byte
	ld r12, Y             ; (2) load next mask byte
5:
	dec r20               ; (1) decrement wallHeight
	brne 1b               ; (1/2) repeat if not zero
	; ------------------------------------------

	; /* draw remaining pixels if any */
; also update the stencil buffer
ldd r26, Z+8
or r26, r18
or r26, r19
std Z+8, r26

	ld r26, Z             ; (2) *buffer
	com r18               ; (1) ~mask
	and r26, r18          ; (1) *buffer & ~mask
	or r26, r19           ; (1) *buffer++ | v
	st Z, r26             ; (2) ...

	pop r29               ; (2)
	pop r28               ; (2)
	pop r17               ; (2)
	pop r16               ; (2)
	pop r13               ; (2)
	pop r12               ; (2)

	ret                   ; (5)

; free to use
;   r18:r27
;   r0
;   r30:r31
;   r1 = always zero
;   r24 = return value

#define __zero_reg__ r1
#define __tmp_reg__ r17

; Z = 31:30
; Y = 29:28
; X = 27:26
;----------------------------------------------------------------
	.global drawTextureColumnScaleUp2

;
; parameters passed
;
; r25:r24 = screenY, r23:r22 = wallHeight, r21:r20 = px_base, r19:r18 = renderInfo #pixels to skip
drawTextureColumnScaleUp2:
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	push r16
	push r17
	push r28
	push r29

	eor r11, r11       ; (1) r11 = texY = 0
	mov r12, r22       ; (1) r12 = sWallHeight
	eor r13, r13       ; (1) r13 = _drawn_pixels = 0
	mov r14, r24       ; (1) r14 = nScreenY = screenY (only valid if screenY >= 0)

	;
	; read pixel pattern from flash
	;
	in r15, 0x2e              ; (1) r15 = px_pattern = SPDR
	out 0x2e, __zero_reg__    ; (1) SPDR = 0, write zero to read next byte from flash

	mov r16, r20              ; (1) r16 = px_base

	;
	; Part 1
	;   fix screenY in case it is negative (outside of visible area), add
	;   px pixels according to the scaling patterns until it is 0 or positive
	;
	tst r25                    ; (1) check if screenY < 0
	brpl _screenY_positive     ; (1/2) if screen >= 0

	eor r14, r14               ; (1) r14 = nScreenY = 0

1:
	mov r0, r16
	lsr r15
	adc r0, __zero_reg__
	add r24, r0                ; (1) add screenY px_base
	adc r25, __zero_reg__      ; (1) ...

	tst r25                    ; (1) check if screenY < 0
	brpl 1f                    ; (1/2) if screen >= 0

	inc r11                    ; (1) texY++
	mov __tmp_reg__, r11       ; (1) texY % 8
	andi __tmp_reg__, 0x7      ; (1) ...
	brne 1b                    ; (1/2) branch if != 0

	in r15, 0x2e               ; (1) px_pattern = SPDR
	out 0x2e, __zero_reg__     ; (1) SPDR = 0, read next byte from flash
	rjmp 1b                    ; (2) continue loop

_screenY_positive:
	mov r24, r16               ; (1) set register to px_base
	lsr r15                    ; (1) px_pattern >> 1
	adc r24, __zero_reg__      ; (1) add px_pattern & 1 to px_pase

	;---------------------------------
	; end of screenY adjustment loop
	;---------------------------------
1:
	movw r28, r18
	ldd r18, Y + offsetof_renderInfo_ystart
	ldd r19, Y + offsetof_renderInfo_yend

	mov r25, r18          ; (1) ystart to r25
	sub r25, r14          ; (1) ystart - yscreen = rest
	brpl 1f               ; (1/2) skip next if still pixels to skip
	eor r25, r25          ; (1) no more pixels to skip
1:
	sub r19, r14
	mov r12, r19


	; r8 = tmp_px, remaining pixels to draw or zero
	mov r8, r24                ; (1) current screenY becomes tmp_px

	;
	; load texel mask
	;
	; r9 = texels (current texture pixels)
	;      texels = es.texColumn[texY / 8];
	;
	ldi r28, lo8(es + offsetof_engineState_texColumn)     ; (1) r28 = low byte of es.texColumn
	ldi r29, hi8(es + offsetof_engineState_texColumn)     ; (1) r29 = high byte of es.texColumn
	mov __tmp_reg__, r11                                  ; (1) texY / 8
	lsr __tmp_reg__                                       ; (1) ...
	lsr __tmp_reg__                                       ; (1) ...
	lsr __tmp_reg__                                       ; (1) ...
	add r28, __tmp_reg__                                  ; (1) add (texY / 8)
	ld r9, Y+                                             ; (2) load texels (new texture pixels)

	;
	; r10 = texShift (current bit in texels)
	;       texShift = bitshift_left[texY % 8];
	;
	mov __tmp_reg__, r11             ; (1) texY % 8
	andi __tmp_reg__, 0x7            ; (1) ...
	ldi r30, lo8(bitshift_left)      ; (1) r30 = low byte of bitshift_left
	ldi r31, hi8(bitshift_left)      ; (1) r31 = low byte of bitshift_left
	add r30, __tmp_reg__             ; (1) add (texY % 8)
	ld r10, Z                        ; (2) load texShift

	tst r8                           ; (1) check if tmp_px == 0
	brne 2f                          ; (1/2) jump to drawing loop
	rjmp _draw_loop_next_texel       ; (2) jump into drawing loop without drawing
2:

	cp r12, r8     ; compare tmp_px with sWallHeight
	brpl 4f        ; (1/2) jump if sWallHeight >= tmp_px
	mov r8, r12    ; (1) else set tmp_px to sWallHeight
4:
	; --- here tmp_px is not zero

	;
	; Part 2
	;   draw the texels according to the scaling patterns
	;

	;--------------------------------
	;
	; main drawing loop
	;
	;--------------------------------
	cpse r25, r1     ; (1/2) check if pixels to skip
	rjmp _skip       ; (2) jump to skip code

_drawing_loop:
	mov r0, r9       ; (1) copy texel-mask as "and" will modify Rd
	and r0, r10      ; (1) check if texel need to be drawn (texel-mask & texShift)
	brne 2f          ; (1/2) branch to draw (3 = no draw, 2 = draw)
	rjmp 1f          ; (2) branch to no draw
2:

	;--------------------------------
	; we need to draw pixels
	;--------------------------------
	; uint8_t color = (es.texColumn[texY / 8 + 4]) & texShift; color of pixel
	;
	ldd r18, Y+3                             ; (2) load value at es.texColumn[texY / 8 + 3]
						 ; only +3 because Y is already incremented

	;--------------------------------
	; unsigned char *buffer = es.screenColumn + nScreenY / 8; destination buffer
	;
	mov __tmp_reg__, r14                                     ; (1) nScreenY / 8
	lsr __tmp_reg__                                          ; (1) ...
	lsr __tmp_reg__                                          ; (1) ...
	lsr __tmp_reg__                                          ; (1) offset to be added to buffer
	ldi r26, lo8(es + offsetof_engineState_screenColumn)     ; (1) load low byte of address
	add r26, __tmp_reg__                                     ; (1) add nScreenY / 8 to screenColumn
	ldi r27, hi8(es + offsetof_engineState_screenColumn)     ; (1) load high byte of address

	;--------------------------------
	; const uint8_t shift = nScreenY % 8; bit position of the pixels to draw within a byte
	;
	mov r19, r14                                             ; (1) r19 = shift = nScreenY
	andi r19, 0x7                                            ; (1) shift % 8

	mov __tmp_reg__, r8                                      ; (1) y = tmp_px
	dec __tmp_reg__                                          ; (1) tmp_px - 1

	lsl __tmp_reg__                                          ; (1) y << 1
	lsl __tmp_reg__                                          ; (1) y << 1
	lsl __tmp_reg__                                          ; (1) y << 1
	add __tmp_reg__, r19                                     ; (1) y += shift
	; here >8 bit
	ldi r19, 4
	mul __tmp_reg__, r19   ; y *= 4
	;
	; const uint8_t *p = pixel_patterns + ((uint16_t)((uint8_t)tmp_px - 1) * 32 + 4 * shift);
	;
	ldi r30, lo8(pixel_patterns)
	add r30, r0
	ldi r31, hi8(pixel_patterns)
	adc r31, r1
	eor r1, r1             ; restore __zero_reg__
	;--------------------------------
	; just count the number of pixels that will be drawn
	add r13, r8    ; add tmp_px to the number of drawn pixels
	;--------------------------------
	; save 18-27, r0, r30-r31 (X, Z, 18-25)
	; r25:r24 = p, r23:r22 = buffer, r21:r20 = tmp_px, r19:r18 = color
	;--------------------------------
	mov r20, r8      ; r20 = tmp_px

	;--------------------------------
	; drawTexel(p, buffer, tmp_px, color);
	;--------------------------------

	; always 1 byte to draw, so load values here
	; --- 1 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer

	; check if color is 0 (black)
	;and r18, r18      ; (1) test if color is zero
	and r18, r10       ; (1) test if color is zero
	breq 5f            ; (1, 2) branch to draw_black code

	; draw 1st byte
	or  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X

	; it tmp_px is one, we are done
	cpi r20, 1
	breq 1f; _done  ; (1/2) jump to end (2)

	; check if tmp_px is lower than 8
	cpi r20, 8      ; 2
	brlo 2f         ; 7 to 2 pixel to draw
	; check if tmp_px is lower than 8
	cpi r20, 16     ; 9
	brlo 3f         ; 15 to 8 pixel to draw

	;
	; draw white use 4 bytes, more than 15 pixel to draw
	;
	; --- 4 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	or  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
3:
	;
	; draw white use 3 bytes
	;
	; --- 3 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	or  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
2:
	;
	; draw white use 2 bytes
	;
	; --- 2 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	or  r19, r21     ; (1) or *p to *buffer
	st  X, r19      ; (2) store r19 to *buffer, inc X

	;
	; drawing done
	;
	rjmp 1f ;_drawTexel_done

5:
	; draw 1st byte
	com r19
	and  r19, r21    ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X

	; it tmp_px is one, we are done
	cpi r20, 1
	breq 1f

	; check if tmp_px is lower than 8
	cpi r20, 8     ; 2
	brlo 2f        ; 7 to 2 pixel to draw
	; check if tmp_px is lower than 8
	cpi r20, 16    ; 9
	brlo 3f        ; 15 to 8 pixel to draw

	;
	; draw black use 4 bytes, more than 15 pixel to draw
	;
	; --- 4 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	com r19
	and  r19, r21    ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
3:
	;
	; draw black use 3 bytes
	;
	; --- 3 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	com r19
	and  r19, r21     ; (1) or *p to *buffer
	st  X+, r19      ; (2) store r19 to *buffer, inc X
2:
	;
	; draw black use 2 bytes
	;
	; --- 2 byte ---
	lpm r19, Z+      ; (3) load byte from p, inc Z
	ld  r21, X       ; (2) load byte from buffer
	com r19
	and  r19, r21     ; (1) or *p to *buffer
	st  X, r19      ; (2) store r19 to *buffer, inc X

1:
	; -------------------------------
	;
	; drawing done
	;
	;--------------------------------

	;--------------------------------
	; no draw or drawing done
	;--------------------------------
1:
; TODO the add can move below breq if r14 is not required at the end
	add r14, r8            ; (1) nScreenY += tmp_px
	sub r12, r8            ; (1) sWallHeight -= tmp_px
	breq 5f                ; (1/2) stop if sWallHeight is 0

_draw_loop_next_texel:
	inc r11                ; texY++

	mov __tmp_reg__, r11   ; if texY % 8 == 0, load new texels
	andi __tmp_reg__, 0x7  ; ...
	brne 2f                ; ...
	ld r9, Y+              ; load next texture byte (texels)
2:
	lsl r10                ; shift to next pixel of interest in texture byte
	brcc 3f                ; (1/2) if no bit went into carry just continue
	; texShift became zero
	adc r10, __zero_reg__      ; add carry to texShift (texShift becomes 1)
	in r15, 0x2e    ; px_pattern = SPDR, read next pattern from flash
	out 0x2e, __zero_reg__           ; SPDR = 0, read next byte from flash
3:

	mov r8, r16    ; move px_base to tmp_px
	lsr r15
	adc r8, __zero_reg__    ; add (px_base & 1) to tmp_px

	cp r12, r8     ; compare tmp_px with sWallHeight
	brpl 4f        ; (1/2) jump if sWallHeight >= tmp_px
	mov r8, r12    ; (1) else set tmp_px to sWallHeight
4:
	; continue the loop
	rjmp _drawing_loop      ; (2)
5:
	; TODO done
	mov r24, r14
	sub r24, r13

	ldi r24, 0
1:

	; TODO pop
	pop r29
	pop r28
	pop r17
	pop r16
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8

	in r0, 0x2d            ; clear SPSR

	ret                    ; return

_skip:
	sub r25, r8
	brpl 6f

	neg r25
	mov r0, r8
	sub r0, r25
	add r14, r0            ; (1) nScreenY += tmp_px
	sub r12, r0            ; (1) sWallHeight -= tmp_px

tst r25
breq _draw_loop_next_texel

	mov r8, r25
	rjmp _drawing_loop
6:
	rjmp 1b

#endif
