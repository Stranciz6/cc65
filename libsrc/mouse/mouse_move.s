;
; Ullrich von Bassewitz, 2003-12-30
;
; void __fastcall__ mouse_move (int x, int y);
; /* Set the mouse cursor to the given position. If a mouse cursor is defined
;  * and currently visible, the mouse cursor is also moved.
;  * NOTE: This function does not check if the given position is valid and
;  * inside the bounding box.
;  */
;

        .import         ptr1: zp

        .include        "mouse-kernel.inc"

.proc   _mouse_move

        sta     ptr1
        stx     ptr1+1                  ; Store x into ptr1
        jsr     popax
        jmp     mouse_move              ; Call the driver

.endproc





