;
; Ullrich von Bassewitz, 2003-05-04
;
; char* __fastcall__ strncpy (char* dest, const char* src, unsigned size);
;

	.export	   	_strncpy
	.import	   	popax
	.importzp  	ptr1, ptr2, ptr3, tmp1, tmp2

.proc   _strncpy

        eor     #$FF
        sta     tmp1
        txa
        eor     #$FF
        sta     tmp2            ; Store -size - 1
       	jsr    	popax 	  	; get src
	sta	ptr1
	stx	ptr1+1
	jsr	popax 		; get dest
	sta	ptr2
	stx	ptr2+1
	sta	ptr3  		; remember for function return
	stx    	ptr3+1

; Copy src -> dest up to size bytes

        ldx     tmp1            ; Load low byte of ones complement of size
	ldy	#$00
L1:     inx
        bne     L2
        inc     tmp2
        beq     L9

L2:     lda     (ptr1),y        ; Copy one character
        sta     (ptr2),y
        beq     L3              ; Bail out if terminator reached
        iny
        bne     L1
        inc     ptr1+1
        inc     ptr2+1          ; Bump high bytes
        bne     L1              ; Branch always
                   
; Fill the remaining bytes. A is zero if we come here

L3:     inx
        bne     L4
        inc     tmp2
        beq     L9

L4:     sta     (ptr2),y
        iny
        bne     L3
        inc     ptr2+1          ; Bump high byte
        bne     L3              ; Branch always

; Done, return dest

L9:     lda     ptr3
        ldx     ptr3+1
        rts

.endproc



