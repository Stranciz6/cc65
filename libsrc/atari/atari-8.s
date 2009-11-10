;
; Graphics driver for the 320x192x2 (CIO mode 8, ANTIC mode F) on the Atari.
;
; Fatih Aygun (2009)
;

	.include	"atari.inc"
	.include 	"zeropage.inc"

	.include 	"tgi-kernel.inc"
	.include        "tgi-mode.inc"
	.include        "tgi-error.inc"

	.macpack        generic

; ******************************************************************************

	; ----------------------------------------------------------------------
	;
	; Constants and tables
	;
	; ----------------------------------------------------------------------

; Graphics mode
	.define grmode 8
; X resolution
	.define x_res 320
; Y resolution
	.define y_res 192
; Number of colors
	.define	colors 2
; Pixels per byte
	.define	ppb 8
; Screen memory size in bytes
	.define	scrsize x_res * y_res / ppb
; Pixel aspect ratio
	.define	aspect $0100				; 1:1
; Free memory needed
	.define	mem_needed 7147
; Number of screen pages
	.define	pages 1

.rodata
	mask_table:				; Mask table to set pixels
		.byte	%10000000, %01000000, %00100000, %00010000, %00001000, %00000100, %00000010, %00000001
	masks:					; Color masks
		.byte	%00000000, %11111111
	bar_table:				; Mask table for BAR
		.byte	%11111111, %01111111, %00111111, %00011111, %00001111, %00000111, %00000011, %00000001, %00000000
	default_palette:
		.byte	$00, $0E
.code

; ******************************************************************************

.proc SETPALETTE

	; ----------------------------------------------------------------------
	;
	; SETPALETTE: Set the palette (in ptr1)
	;
	; ----------------------------------------------------------------------

.code
	; Copy the palette
	ldy     #colors - 1
loop:	lda     (ptr1),y
	sta     palette,y
	dey
	bpl     loop

	; Get the color entries from the palette
	lda	palette
	sta	COLOR2
	lda	palette + 1
	sta	COLOR1

	; Done, reset the error code
        lda     #TGI_ERR_OK
        sta     error
        rts
.endproc

.include "atari_tgi_common.inc"
