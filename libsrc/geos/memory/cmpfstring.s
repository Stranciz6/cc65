
;
; Maciej 'YTM/Elysium' Witkowiak
;
; 22.12.99, 29.07.2000

; char CmpFString (char length, char *dest, char* source);

	    .import DoubleSPop, SetPtrXY
	    .import popa
	    .export _CmpFString

	    .include "../inc/jumptab.inc"

_CmpFString:
	    jsr DoubleSPop
	    jsr popa
	    jsr SetPtrXY
	    jsr CmpFString
	    bne L1
	    jmp return0
L1:	    jmp return1
