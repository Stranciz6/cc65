;
; Based on code by Debrune J�r�me <jede@oric.org>
; 2016-03-17, Greg King
;

        ; The following symbol is used by the linker config. file
        ; to force this module to be included into the output file.
        .export __ORIXHDR__:abs = 1

        ; These symbols, also, come from the configuration file.
        .import __AUTORUN__, __PROGFLAG__
        .import __BASHEAD_START__, __MAIN_LAST__


; ------------------------------------------------------------------------
; Oric cassette-tape header

.segment        "ORIXHDR"

        .byte   $01, $00          ; 

	.byte "ORI"

	.byte $01 ; version
	.byte $00,$00 ; mode
	.byte $00,$00 ; cpu type
	.byte $00,$00 ; OS

        .byte   $00                     ;  reserved
        .byte   $00                     ; auto 


	.dbyt   __BASHEAD_START__       ; Address of start of file
	.dbyt   __MAIN_LAST__ - 1       ;  Address of end of file
	.dbyt   __BASHEAD_START__       ;  Address of start of file

