;
; Ullrich von Bassewitz, 22.06.2002
;
; Helper function for getpixel/setpixel. Load X/Y from stack and check if
; the coordinates are valid. Return carry clear if so.
;

        .include        "tgi-kernel.inc"

        .import         popax
        .importzp       ptr1, ptr2


tgi_getset:
        sta     ptr2            ; Y
        stx     ptr2+1
        jsr     popax
        sta     ptr1            ; X
        stx     ptr1+1

; Are the coordinates are out of range? First check if any ccord is negative.

        txa
        ora     ptr2+1
        asl     a
        bcs     @L9             ; Bail out if negative

; Check if X is larger than the maximum x coord. If so, bail out

        lda     ptr1
        cmp     _tgi_xres
        txa
        sbc     _tgi_xres+1
        bcs     @L9

; Check if Y is larger than the maximum y coord.

        lda     ptr2
        cmp     _tgi_yres
        lda     ptr2+1
        sbc     _tgi_yres+1
@L9:    rts


