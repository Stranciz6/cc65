;
; Ullrich von Bassewitz, 22.11.2002
;
; int __fastcall__ fclose (FILE* f);
; /* Close a file */
;

        .export         _fclose

        .import         _close
        .importzp       ptr1                                   

        .include        "errno.inc"
        .include        "_file.inc"

; ------------------------------------------------------------------------
; Code

.proc   _fclose

        sta     ptr1
        stx     ptr1+1          ; Store f

; Check if the file is really open

        ldy     #_FILE_f_flags
        lda     (ptr1),y
        and     #_FOPEN
        bne     @L1

; File is not open

        lda     #EINVAL
        sta     __errno
        ldx     #0
        stx     __errno+1
        dex
        txa
        rts

; File is open. Reset the flags and close the file.

@L1:    lda     #_FCLOSED
        sta     (ptr1),y

        ldy     #_FILE_f_fd
        lda     (ptr1),y
        ldx     #0
        jmp     _close          ; Will set errno and return an error flag

.endproc

