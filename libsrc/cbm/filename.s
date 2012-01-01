;
; Ullrich von Bassewitz, 16.11.2002
;
; File name handling for CBM file I/O
;

        .export         fnparse, fnparsename, fnset
        .export         fnadd, fnaddmode, fncomplete, fndefunit
        .export         fnunit, fnlen, fncmd, fnbuf

        .import         SETNAM
        .import         __curunit, __filetype
        .importzp       ptr1, tmp1

        .include        "ctype.inc"


;------------------------------------------------------------------------------
; fnparsename: Parse a filename (without drive spec) passed in in ptr1 and y.

.proc   fnparsename

        lda     #0
        sta     tmp1            ; Remember length of name

nameloop:
        lda     (ptr1),y        ; Get next char from filename
        beq     namedone        ; Jump if end of name reached

; Check the maximum length, store the character

        ldx     tmp1
        cpx     #16             ; Maximum length reached?
        bcs     invalidname
        lda     (ptr1),y        ; Reload char
        jsr     fnadd           ; Add character to name
        iny                     ; Next char from name
        inc     tmp1            ; Increment length of name
        bne     nameloop        ; Branch always

; Invalid file name

invalidname:
        lda     #33             ; Invalid file name

; Done, we've successfully parsed the name.

namedone:
        rts

.endproc


;------------------------------------------------------------------------------
; fnparse: Parse a full filename passed in in a/x. Will set the following
; variables:
;
;       fnlen   -> length of filename
;       fnbuf   -> filename including drive spec
;       fnunit  -> unit from spec or default unit
;
; Returns an error code in A or zero if all is ok.

.proc   fnparse

        sta     ptr1
        stx     ptr1+1          ; Save pointer to name

; For now we will always use the default unit

        jsr     fndefunit

; Check the name for a drive spec

        ldy     #0
        lda     (ptr1),y
        cmp     #'0'
        beq     digit
        cmp     #'1'
        bne     nodrive

digit:  sta     fnbuf+0
        iny
        lda     (ptr1),y
        cmp     #':'
        bne     nodrive

; We found a drive spec, copy it to the buffer

        sta     fnbuf+1
        iny                     ; Skip colon
        bne     drivedone       ; Branch always

; We did not find a drive spec, always use drive zero

nodrive:
        lda     #'0'
        sta     fnbuf+0
        lda     #':'
        sta     fnbuf+1
        ldy     #$00            ; Reposition to start of name

; Drive spec done. We do now have a drive spec in the buffer.

drivedone:
        lda     #2              ; Length of drive spec
        sta     fnlen

; Copy the name into the file name buffer. The subroutine returns an error
; code in A and zero flag set if the were no errors.

        jmp     fnparsename

.endproc

;--------------------------------------------------------------------------
; fndefunit: Use the default unit

.proc   fndefunit

        lda     __curunit
        sta     fnunit
        rts

.endproc

;--------------------------------------------------------------------------
; fnset: Tell the kernal about the file name

.proc   fnset

        lda     fnlen
        ldx     #<fnbuf
        ldy     #>fnbuf
        jmp     SETNAM

.endproc

;--------------------------------------------------------------------------
; fncomplete: Complete a filename by adding ",t,m" where t is the file type
; and m is the access mode passed in in the A register
;
; fnaddmode: Add ",m" to a filename, where "m" is passed in A

fncomplete:
	pha	   		; Save mode
        lda     __filetype
        jsr     fnaddmode       ; Add the type
        pla
fnaddmode:
        pha
        lda     #','
        jsr     fnadd
        pla

fnadd:  ldx     fnlen
        inc     fnlen
        sta     fnbuf,x
        rts

;--------------------------------------------------------------------------
; Data

.bss

fnunit: .res    1
fnlen:  .res    1

.data
fncmd:  .byte   's'     ; Use as scratch command
fnbuf:  .res    35      ; Either 0:0123456789012345,t,m
                        ; Or     0:0123456789012345=0123456789012345
