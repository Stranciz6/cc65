;
; Ullrich von Bassewitz, 03.06.1999
;
; unsigned char __fastcall__ cbm_k_open (void);
;

       	.export	       	_cbm_k_open
        .import         OPEN


_cbm_k_open:
	jsr	OPEN
	bcs	@NotOk
        lda     #0
@NotOk:	ldx     #0              ; Clear high byte
        rts
