;
; Ullrich von Bassewitz, 26.10.2000
;
; Screen size variables
;

	.export		xsize, ysize
	.constructor	initscrsize

	.include	"../cbm/cbm.inc"

.code

initscrsize:
   	jsr	SCREEN
   	stx	xsize
   	sty	ysize
	rts

.bss

xsize: 	.res	1
ysize:	.res	1


