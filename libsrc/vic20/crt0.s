;
; Startup code for cc65 (Vic20 version)
;

        .export         _exit
        .export         __STARTUP__ : absolute = 1      ; Mark as startup
        .import         initlib, donelib
        .import         zerobss, push0
        .import         callmain
        .import         RESTOR, BSOUT, CLRCH
        .import         __RAM_START__, __RAM_SIZE__     ; Linker generated
        .import         __STACKSIZE__                   ; Linker generated
        .importzp       ST

        .include        "zeropage.inc"
        .include        "vic20.inc"

; ------------------------------------------------------------------------
; Startup code

.segment        "STARTUP"

Start:

; Save the zero-page locations that we need.

        ldx     #zpspace-1
L1:     lda     sp,x
        sta     zpsave,x
        dex
        bpl     L1

; Switch to the second charset.

        lda     #14
        jsr     BSOUT

; Clear the BSS data.

        jsr     zerobss

; Save some system stuff; and, set up the stack.

        tsx
        stx     spsave          ; Save the system stack ptr

        lda     #<(__RAM_START__ + __RAM_SIZE__ + __STACKSIZE__)
        sta     sp
        lda     #>(__RAM_START__ + __RAM_SIZE__ + __STACKSIZE__)
        sta     sp+1            ; Set argument stack ptr

; Call the module constructors.

        jsr     initlib

; Push the command-line arguments; and, call main().

        jsr     callmain

; Back from main() [this is also the exit() entry]. Run the module destructors.

_exit:  pha                     ; Save the return code on stack
        jsr     donelib

; Copy back the zero-page stuff.

        ldx     #zpspace-1
L2:     lda     zpsave,x
        sta     sp,x
        dex
        bpl     L2

; Place the program return code into BASIC's status variable.

        pla
        sta     ST

; Restore the stack pointer.

        ldx     spsave
        txs

; Back to BASIC.

        rts

; ------------------------------------------------------------------------

.segment        "ZPSAVE"

zpsave: .res    zpspace

; ------------------------------------------------------------------------

.bss

spsave: .res    1
