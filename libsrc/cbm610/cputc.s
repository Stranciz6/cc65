;
; Ullrich von Bassewitz, 06.08.1998
;
; void cputcxy (unsigned char x, unsigned char y, char c);
; void cputc (char c);
;

    	.export	     	_cputcxy, _cputc, cputdirect, putchar
	.export	     	newline, plot
	.exportzp	CURS_X, CURS_Y
	.import		_gotoxy
	.import	     	popa
	.import	     	xsize, revers

	.include     	"cbm610.inc"
	.include	"zeropage.inc"
	.include     	"../cbm/cbm.inc"

_cputcxy:
	pha	     		; Save C
	jsr	popa 		; Get Y
	jsr	_gotoxy		; Set cursor, drop x
	pla	     		; Restore C

; Plot a character - also used as internal function

_cputc: cmp    	#$0A  		; CR?
    	bne	L1
    	lda	#0
    	sta	CURS_X
       	beq    	plot 	       	; Recalculate pointers

L1: 	cmp	#$0D  	       	; LF?
       	beq	newline	       	; Recalculate pointers

; Printable char of some sort

	cmp	#' '
    	bcc	cputdirect     	; Other control char
    	tay
    	bmi	L10
    	cmp	#$60
    	bcc	L2
    	and	#$DF
    	bne	cputdirect     	; Branch always
L2: 	and	#$3F

cputdirect:
  	jsr	putchar	       	; Write the character to the screen

; Advance cursor position

advance:
        iny
        cpy     xsize
        bne     L3
        jsr     newline         ; new line
        ldy     #0              ; + cr
L3:     sty     CURS_X
        rts

newline:
   	clc
   	lda	xsize
   	adc	CharPtr
   	sta     CharPtr
   	bcc	L4
   	inc	CharPtr+1
L4:	inc	CURS_Y
   	rts

; Handle character if high bit set

L10:	and	#$7F	 
       	cmp    	#$7E 	       	; PI?
	bne	L11
	lda	#$5E 	       	; Load screen code for PI
	bne	cputdirect
L11:	ora	#$40
	bne	cputdirect	; Branch always

; Set cursor position, calculate RAM pointers

plot:	ldy	CURS_X
	ldx	CURS_Y
	clc
	jmp	PLOT

; Write one character to the screen without doing anything else, return X
; position in Y

putchar:
	ldx	IndReg
	ldy	#$0F
	sty	IndReg
    	ora	revers	    	; Set revers bit
       	ldy    	CURS_X
	sta	(CharPtr),y 	; Set char
	stx	IndReg
	rts


