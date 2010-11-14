;
; Ullrich von Bassewitz, 2010-11-14
;
; This module supplies a small BASIC stub program that jumps to the machine
; language code that follows it using SYS.
;

        ; The following symbol is used by linker config to force the module
        ; to get included into the output file
        .export         __EXEHDR__: absolute = 1

.segment        "EXEHDR"

        .addr   Next
        .word   .version        ; Line number
        .byte   $9E             ; SYS token
        .byte   <(((Start /  1000) .mod 10) + '0')
        .byte   <(((Start /   100) .mod 10) + '0')
        .byte   <(((Start /    10) .mod 10) + '0')
        .byte   <(((Start /     1) .mod 10) + '0')
        .byte   $00             ; End of BASIC line
Next:   .word   0               ; BASIC end marker
Start:




