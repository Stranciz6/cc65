;
; Driver for the 1351 proportional mouse. Parts of the code are from
; the Commodore 1351 mouse users guide.
;
; Ullrich von Bassewitz, 2003-12-29
;

        .include        "zeropage.inc"
        .include        "mouse-kernel.inc"
	.include	"c64.inc"

        .macpack        generic

; ------------------------------------------------------------------------
; Header. Includes jump table

.segment        "JUMPTABLE"

HEADER:

; Driver signature

        .byte   $6d, $6f, $75           ; "mou"
        .byte   MOUSE_API_VERSION       ; Mouse driver API version number

; Jump table.

        .addr   INSTALL
        .addr   UNINSTALL
        .addr   HIDE
        .addr   SHOW
        .addr   BOX
        .addr   MOVE
        .addr   BUTTONS
        .addr   POS
        .addr   INFO
        .addr   IOCTL
        .addr   IRQ

; Callback table, set by the kernel before INSTALL is called

CHIDE:  jmp     $0000                   ; Hide the cursor
CSHOW:  jmp     $0000                   ; Show the cursor
CMOVEX: jmp     $0000                   ; Move the cursor to X coord
CMOVEY: jmp     $0000                   ; Move the cursor to Y coord


;----------------------------------------------------------------------------
; Constants

SCREEN_HEIGHT   = 200
SCREEN_WIDTH    = 320

;----------------------------------------------------------------------------
; Global variables. The bounding box values are sorted so that they can be
; written with the least effort in the BOX routine, so don't reorder them.

.bss

Vars:
OldPotX:   	.res   	1	     	; Old hw counter values
OldPotY:	.res   	1

YPos:           .res    2               ; Current mouse position, Y
XPos:           .res    2               ; Current mouse position, X
YMax:		.res	2	     	; Y2 value of bounding box
XMax:		.res	2	     	; X2 value of bounding box
YMin:		.res	2	     	; Y1 value of bounding box
XMin:		.res	2	     	; X1 value of bounding box

OldValue:	.res   	1	     	; Temp for MoveCheck routine
NewValue:	.res   	1	     	; Temp for MoveCheck routine

; Default values for above variables

.rodata

.proc   DefVars
        .byte   0, 0                    ; OldPotX/OldPotY
        .word   SCREEN_HEIGHT/2         ; YPos
        .word   SCREEN_WIDTH/2          ; XPos
        .word   SCREEN_HEIGHT           ; YMax
        .word   SCREEN_WIDTH            ; XMax
        .word   0                       ; YMin
        .word   0                       ; XMin
.endproc

.code

;----------------------------------------------------------------------------
; INSTALL routine. Is called after the driver is loaded into memory. If
; possible, check if the hardware is present.
; Must return an MOUSE_ERR_xx code in a/x.

INSTALL:

; Initialize variables. Just copy the default stuff over

        ldx     #.sizeof(DefVars)-1
@L1:    lda     DefVars,x
        sta     Vars,x
        dex
        bpl     @L1

; Be sure the mouse cursor is invisible and at the default location. We
; need to do that here, because our mouse interrupt handler doesn't set the
; mouse position if it hasn't changed.

        sei
        jsr     CHIDE
        lda     XPos
        ldx     XPos+1
        jsr     CMOVEX
        lda     YPos
        ldx     YPos+1
        jsr     CMOVEY
        cli

; Done, return zero (= MOUSE_ERR_OK)

        ldx     #$00
        txa
        rts                             ; Run into UNINSTALL instead

;----------------------------------------------------------------------------
; UNINSTALL routine. Is called before the driver is removed from memory.
; No return code required (the driver is removed from memory on return).

UNINSTALL       = HIDE                  ; Hide cursor on exit

;----------------------------------------------------------------------------
; HIDE routine. Is called to hide the mouse pointer. The mouse kernel manages
; a counter for calls to show/hide, and the driver entry point is only called
; if the mouse is currently visible and should get hidden. For most drivers,
; no special action is required besides hiding the mouse cursor.
; No return code required.

HIDE:   sei
        jsr     CHIDE
        cli
        rts

;----------------------------------------------------------------------------
; SHOW routine. Is called to show the mouse pointer. The mouse kernel manages
; a counter for calls to show/hide, and the driver entry point is only called
; if the mouse is currently hidden and should become visible. For most drivers,
; no special action is required besides enabling the mouse cursor.
; No return code required.

SHOW:   sei
        jsr     CSHOW
        cli
        rts

;----------------------------------------------------------------------------
; BOX: Set the mouse bounding box. The parameters are passed as they come from
; the C program, that is, maxy in a/x and the other parameters on the stack.
; The C wrapper will remove the parameters from the stack when the driver
; routine returns.
; No checks are done if the mouse is currently inside the box, this is the job
; of the caller. It is not necessary to validate the parameters, trust the
; caller and save some code here. No return code required.

BOX:    ldy     #5
        sei
        sta     YMax
        stx     YMax+1

@L1:    lda     (sp),y
        sta     XMax,y
        dey
        bpl     @L1

        cli
       	rts

;----------------------------------------------------------------------------
; MOVE: Move the mouse to a new position. The position is passed as it comes
; from the C program, that is: X on the stack and Y in a/x. The C wrapper will
; remove the parameter from the stack on return.
; No checks are done if the new position is valid (within the bounding box or
; the screen). No return code required.
;

MOVE:   sei                             ; No interrupts

        sta     YPos
        stx     YPos+1                  ; New Y position
        jsr     CMOVEY                  ; Set it

        ldy     #$01
        lda     (sp),y
        sta     XPos+1
        tax
        dey
        lda     (sp),y
        sta     XPos                    ; New X position

        jsr     CMOVEX			; Move the cursor

	cli                             ; Allow interrupts
       	rts

;----------------------------------------------------------------------------
; BUTTONS: Return the button mask in a/x.

BUTTONS:
        lda	#$7F
     	sei
     	sta	CIA1_PRA
     	lda	CIA1_PRB                ; Read joystick #0
     	cli
        ldx     #0
     	and	#$1F
     	eor	#$1F
        rts

;----------------------------------------------------------------------------
; POS: Return the mouse position in the MOUSE_POS struct pointed to by ptr1.
; No return code required.

POS:    ldy    	#MOUSE_POS::XCOORD      ; Structure offset

	sei	    			; Disable interrupts
	lda     XPos			; Transfer the position
	sta	(ptr1),y
	lda	XPos+1
	iny
	sta	(ptr1),y
      	lda	YPos
        iny
        sta     (ptr1),y
	lda	YPos+1
	cli	    			; Enable interrupts

        iny
        sta     (ptr1),y                ; Store last byte

    	rts	    			; Done

;----------------------------------------------------------------------------
; INFO: Returns mouse position and current button mask in the MOUSE_INFO
; struct pointed to by ptr1. No return code required.
;
; We're cheating here to keep the code smaller: The first fields of the
; mouse_info struct are identical to the mouse_pos struct, so we will just
; call _mouse_pos to initialize the struct pointer and fill the position
; fields.

INFO:   jsr	POS

; Fill in the button state

    	jsr     BUTTONS                 ; Will not touch ptr1
    	ldy	#MOUSE_INFO::BUTTONS
    	sta	(ptr1),y

      	rts

;----------------------------------------------------------------------------
; IOCTL: Driver defined entry point. The wrapper will pass a pointer to ioctl
; specific data in ptr1, and the ioctl code in A.
; Must return an error code in a/x.
;

IOCTL:  lda     #<MOUSE_ERR_INV_IOCTL     ; We don't support ioclts for now
        ldx     #>MOUSE_ERR_INV_IOCTL
        rts

;----------------------------------------------------------------------------
; IRQ: Irq handler entry point. Called as a subroutine but in IRQ context
; (so be careful). The routine MUST return carry set if the interrupt has been
; 'handled' - which means that the interrupt source is gone. Otherwise it
; MUST return carry clear.
;

IRQ:    lda	SID_ADConv1		; Get mouse X movement
      	ldy	OldPotX
      	jsr	MoveCheck  		; Calculate movement vector
      	sty	OldPotX

; Skip processing if nothing has changed

        bcc     @SkipX

; Calculate the new X coordinate (--> a/y)

       	add	XPos
      	tay	      			; Remember low byte
      	txa
      	adc	XPos+1
    	tax

; Limit the X coordinate to the bounding box

   	cpy	XMin
   	sbc	XMin+1
   	bpl	@L1
       	ldy    	XMin
       	ldx	XMin+1
    	jmp	@L2
@L1:	txa

    	cpy	XMax
    	sbc	XMax+1
    	bmi	@L2
    	ldy	XMax
    	ldx	XMax+1
@L2:	sty	XPos
   	stx	XPos+1

; Move the mouse pointer to the new X pos

        tya
        jsr     CMOVEX

; Calculate the Y movement vector

@SkipX: lda	SID_ADConv2	 	; Get mouse Y movement
 	ldy	OldPotY
 	jsr	MoveCheck	 	; Calculate movement
 	sty	OldPotY

; Skip processing if nothing has changed

        bcc     @SkipY

; Calculate the new Y coordinate (--> a/y)

      	sta	OldValue
      	lda	YPos
      	sub	OldValue
      	tay
      	stx	OldValue
      	lda	YPos+1
      	sbc	OldValue
      	tax

; Limit the Y coordinate to the bounding box

   	cpy	YMin
 	sbc	YMin+1
 	bpl	@L3
       	ldy    	YMin
       	ldx	YMin+1
    	jmp	@L4
@L3:	txa

    	cpy	YMax
    	sbc	YMax+1
    	bmi	@L4
    	ldy	YMax
    	ldx	YMax+1
@L4:	sty	YPos
 	stx	YPos+1

; Move the mouse pointer to the new X pos

        tya
        jsr     CMOVEY                                         

; Done

        clc                             ; Interrupt not handled
@SkipY: rts

; --------------------------------------------------------------------------
;
; Move check routine, called for both coordinates.
;
; Entry:   	y = old value of pot register
;     	   	a = current value of pot register
; Exit:	   	y = value to use for old value
;     	   	x/a = delta value for position
;

MoveCheck:
      	sty	OldValue
      	sta	NewValue
      	ldx 	#$00

      	sub	OldValue	   	; a = mod64 (new - old)
      	and	#%01111111
      	cmp	#%01000000	   	; if (a > 0)
      	bcs	@L1 		   	;
      	lsr	a   		   	;   a /= 2;
      	beq	@L2 		   	;   if (a != 0)
      	ldy   	NewValue     	   	;     y = NewValue
        sec
      	rts   	    		   	;   return

@L1:  	ora   	#%11000000	   	; else or in high order bits
      	cmp   	#$FF		   	; if (a != -1)
      	beq   	@L2
      	sec
      	ror   	a   		   	;   a /= 2
       	dex			   	;   high byte = -1 (X = $FF)
      	ldy   	NewValue
        sec
      	rts

@L2:   	txa			   	; A = $00
        clc
      	rts

