;
; Ullrich von Bassewitz, 11.04.1999
;
; CC65 runtime: Load an unsigned char indirect from pointer somewhere in stack
;

	.export		ldau00sp, ldau0ysp
	.importzp      	sp, ptr1

ldau00sp:
     	ldy	#1
ldau0ysp:
     	lda	(sp),y
     	sta	ptr1+1
     	dey
     	lda	(sp),y
     	sta	ptr1
	ldx	#0
	lda	(ptr1,x)
	rts

