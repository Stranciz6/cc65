; divt.s
;
; 2002-10-22, Greg King
;
; This signed-division function returns both the quotient and the remainder,
; in this structure:
;
; typedef struct {
;     int rem, quot;
; } div_t;
;
; div_t __fastcall__ div (int numer, int denom);
;
; Both sides of an assignment-expression must be cast to (long)
; because cc65 functions can't return structures -- yet.  Example:
;
; #include <stdlib.h>
; div_t answer;
;
; (long)answer = (long)div(-41, +3);
; printf("The quotient is %d, and the remainder is %d.\n",
;	answer.quot, answer.rem);

	.export		_div

	.import		tosdivax, negax
	.importzp	sreg, ptr1, tmp1

_div:	jsr	tosdivax	; Division-operator does most of the work
        sta     sreg		; Quotient is in sreg
        stx     sreg+1
        lda     ptr1  		; Unsigned remainder is in ptr1
        ldx     ptr1+1

; Adjust the sign of the remainder.
; It must be the same as the sign of the dividend.
;
	bit	tmp1		; Load high-byte of left argument
	bpl	Pos		; Jump if sign-of-result is positive
	jmp	negax		; Result is negative, adjust the sign

Pos:	rts

