;
; Ullrich von Bassewitz, 21.08.1998
;
; CC65 runtime: Load effective address with offset in Y relative to SP
;

    	.export		leaasp, pleaasp
     	.import		pushax
     	.importzp	sp

leaasp:	ldx	sp+1		; Get high byte
     	clc
     	adc	sp
     	bcc	@L1
     	inx
@L1: 	rts


pleaasp:
  	ldx	sp+1		; Get high byte
  	clc
  	adc	sp
   	bcc	L9
   	inx
L9:	jmp	pushax



