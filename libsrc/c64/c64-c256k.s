;
; Extended memory driver for the C256K memory expansion
; Marco van den Heuvel, 2010-01-27
;

        .include	"zeropage.inc"

        .include	"em-kernel.inc"
        .include	"em-error.inc"


        .macpack	generic


; ------------------------------------------------------------------------
; Header. Includes jump table

.segment        "JUMPTABLE"

; Driver signature

        .byte	$65, $6d, $64		; "emd"
        .byte	EMD_API_VERSION		; EM API version number

; Jump table.

        .word	INSTALL
        .word	UNINSTALL
        .word	PAGECOUNT
        .word	MAP
        .word	USE
        .word	COMMIT
        .word	COPYFROM
        .word	COPYTO

; ------------------------------------------------------------------------
; Constants

BASE                    = $4000
PAGES                   = 3 * 256
CHECKC256K              = $0200
TRANSFERC256K           = $0200
STASHOPCODE             = $91
pia                     = $DFC0

; ------------------------------------------------------------------------
; Data.

.data


; This function is used to copy code from and to the extended memory
.proc   c256kcopycode
.org    ::TRANSFERC256K         ; Assemble for target location
        stx     pia
::STASHC256K    := *            ; Location and opcode is patched at runtime
::VECC256K      := *+1
        lda     ($00),y
        ldx     #$dc
        stx     pia
        rts
.reloc
.endproc

; This function is used to check for the existence of the extended memory
.proc   c256kcheckcode
.org    ::CHECKC256K
        ldy     #$00            ; Assume hardware not present

        lda     #$fc
        sta     pia
        lda     $01
        tax
        and     #$f8
        sta     $01
        lda     $4000
        cmp     $c000
        bne     done            ; Jump if not found
        inc     $c000
        cmp     $4000
        beq     done            ; Jump if not found

        ; Hardware is present
        iny
done:   stx     $01
        ldx     #$dc
        stx     pia
        rts
.reloc
.endproc

; Since the functions above are copied to $200, the current contents of this
; memory area must be saved into backup storage. Calculate the amount of
; space necessary.
.if     .sizeof (c256kcopycode) > .sizeof (c256kcheckcode)
backupspace     = .sizeof (c256kcopycode)
.else
backupspace     = .sizeof (c256kcheckcode)
.endif


.bss

curpage:        .res   	2       ; Current page number
curbank:        .res	1	; Current bank
backup:         .res   	backupspace     ; Backup area of data in the location
                                ; where the copy and check routines will be
window:         .res   	256    	; Memory "window"


.code
; ------------------------------------------------------------------------
; INSTALL routine. Is called after the driver is loaded into memory. If
; possible, check if the hardware is present and determine the amount of
; memory available.
; Must return an EM_ERR_xx code in a/x.
;

INSTALL:
        lda	pia+1		; Select Peripheral Registers
        ora	#4
        sta	pia+1
        tax
        lda	pia+3
        ora	#4
        sta	pia+3
        tay

        lda	#$DC		; Set the default memory bank data
        sta	pia
        lda	#$FE
        sta	pia+2

        txa			; Select Data Direction Registers
        and	#$FB
        sta	pia+1
        tya
        and	#$FB
        sta	pia+3

        lda	#$FF		; Set the ports to output
        sta	pia
        sta	pia+2

        txa
        and	#$C7
        ora	#$30		; Set CA1 and
        sta	pia+1		; select Peripheral Registers
        sty	pia+3

        jsr	backup_and_setup_check_routine
        jsr	CHECKC256K
        cli
        ldx	#.sizeof (c256kcheckcode) - 1
        jsr	restore_data
        cpy	#$01
        beq	@present
        lda	#<EM_ERR_NO_DEVICE
        ldx	#>EM_ERR_NO_DEVICE
        rts

@present:
        lda	#<EM_ERR_OK
        ldx	#>EM_ERR_OK
;       rts			; Run into UNINSTALL instead

; ------------------------------------------------------------------------
; UNINSTALL routine. Is called before the driver is removed from memory.
; Can do cleanup or whatever. Must not return anything.
;

UNINSTALL:
        rts


; ------------------------------------------------------------------------
; PAGECOUNT: Return the total number of available pages in a/x.
;

PAGECOUNT:
        lda	#<PAGES
        ldx	#>PAGES
        rts

; ------------------------------------------------------------------------
; MAP: Map the page in a/x into memory and return a pointer to the page in
; a/x. The contents of the currently mapped page (if any) may be discarded
; by the driver.
;

MAP:
        sei
        sta	curpage		; Remember the new page
        stx	curpage+1
        jsr	adjust_page_and_bank
        stx	curbank
        clc
        adc	#>BASE
        sta	ptr1+1
        ldy	#0
        sty	ptr1
        jsr	backup_and_setup_copy_routine
        ldx	#<ptr1
        stx     VECC256K
@L1:
        ldx	curbank
        jsr	TRANSFERC256K
        ldx	ptr1
        sta	window,x
        inc	ptr1
        bne	@L1

; Return the memory window

        jsr	restore_copy_routine
        lda	#<window
        ldx	#>window		; Return the window address
        cli
        rts

; ------------------------------------------------------------------------
; USE: Tell the driver that the window is now associated with a given page.

USE:    sta	curpage		; Remember the page
        stx	curpage+1
        lda	#<window
        ldx	#>window		; Return the window
        rts

; ------------------------------------------------------------------------
; COMMIT: Commit changes in the memory window to extended storage.

COMMIT:
        sei
        lda	curpage		; Get the current page
        ldx	curpage+1

        jsr	adjust_page_and_bank
        stx	curbank
        clc
        adc	#>BASE
        sta	ptr1+1
        ldy	#0
        sty	ptr1
        jsr	backup_and_setup_copy_routine
        ldx	#<ptr1
        stx     VECC256K
        ldx	#<STASHOPCODE
        stx	STASHC256K
@L1:
        ldx	ptr1
        lda	window,x
        ldx	curbank
        jsr	TRANSFERC256K
        inc	ptr1
        bne	@L1

; Return the memory window

        jsr	restore_copy_routine
done:
        cli
        rts

; ------------------------------------------------------------------------
; COPYFROM: Copy from extended into linear memory. A pointer to a structure
; describing the request is passed in a/x.
; The function must not return anything.
;


COPYFROM:
        sei
        jsr	setup
        jsr	backup_and_setup_copy_routine

; Setup is:
;
;   - ptr1 contains the struct pointer
;   - ptr2 contains the linear memory buffer
;   - ptr3 contains -(count-1)
;   - ptr4 contains the page memory buffer plus offset
;   - tmp1 contains zero (to be used for linear memory buffer offset)
;   - tmp2 contains the bank value

        lda	#<ptr4
        sta     VECC256K
        jmp	@L3

@L1:
        ldx	tmp2
        ldy	#0
        jsr	TRANSFERC256K
        ldy	tmp1
        sta	(ptr2),y
        inc	tmp1
        bne	@L2
        inc	ptr2+1
@L2:
        inc	ptr4
        beq	@L4

; Bump count and repeat

@L3:
        inc	ptr3
        bne	@L1
        inc	ptr3+1
        bne	@L1
        jsr	restore_copy_routine
        cli
        rts

; Bump page register

@L4:
        inc	ptr4+1
        lda	ptr4+1
        cmp	#$80
        bne	@L3
        lda	#>BASE
        sta	ptr4+1
        lda	tmp2
        clc
        adc	#$10
        sta	tmp2
        jmp	@L3

; ------------------------------------------------------------------------
; COPYTO: Copy from linear into extended memory. A pointer to a structure
; describing the request is passed in a/x.
; The function must not return anything.
;

COPYTO:
        sei
        jsr	setup
        jsr	backup_and_setup_copy_routine

; Setup is:
;
;   - ptr1 contains the struct pointer
;   - ptr2 contains the linear memory buffer
;   - ptr3 contains -(count-1)
;   - ptr4 contains the page memory buffer plus offset
;   - tmp1 contains zero (to be used for linear memory buffer offset)
;   - tmp2 contains the bank value

        lda	#<ptr4
        sta     VECC256K
        lda  	#<STASHOPCODE
        sta  	STASHC256K
        jmp  	@L3

@L1:
        ldy  	tmp1
        lda  	(ptr2),y
        ldx  	tmp2
        ldy  	#0
        jsr  	TRANSFERC256K
        inc  	tmp1
        bne  	@L2
        inc  	ptr2+1
@L2:
        inc  	ptr4
        beq  	@L4

; Bump count and repeat

@L3:
        inc  	ptr3
        bne  	@L1
        inc  	ptr3+1
        bne  	@L1
        jsr  	restore_copy_routine
        cli
        rts

; Bump page register

@L4:
        inc  	ptr4+1
        lda  	ptr4+1
        cmp     #$80
        bne  	@L3
        lda  	#>BASE
        sta  	ptr4+1
        lda  	tmp2
        clc
        adc  	#$10
        sta  	tmp2
        jmp  	@L3

; ------------------------------------------------------------------------
; Helper function for COPYFROM and COPYTO: Store the pointer to the request
; structure and prepare data for the copy

setup:
        sta	ptr1
        stx	ptr1+1					; Save passed pointer

; Get the page number from the struct and adjust it so that it may be used
; with the hardware. That is: ptr4 has the page address and page offset
; tmp2 will hold the bank value

        ldy	#EM_COPY::PAGE+1
        lda	(ptr1),y
        tax
        ldy	#EM_COPY::PAGE
        lda	(ptr1),y
        jsr	adjust_page_and_bank
        clc
        adc	#>BASE
        sta	ptr4+1
        stx	tmp2

; Get the buffer pointer into ptr2

        ldy	#EM_COPY::BUF
        lda	(ptr1),y
        sta	ptr2
        iny
        lda	(ptr1),y
        sta	ptr2+1

; Get the count, calculate -(count-1) and store it into ptr3

        ldy	#EM_COPY::COUNT
        lda	(ptr1),y
        eor	#$FF
        sta	ptr3
        iny
        lda	(ptr1),y
        eor	#$FF
        sta	ptr3+1

; Get the page offset into ptr4 and clear tmp1

        ldy	#EM_COPY::OFFS
        lda	(ptr1),y
        sta	ptr4
        lda	#0
        sta	tmp1

; Done

        rts

; Helper routines for copying to and from the +256k ram

backup_and_setup_copy_routine:
        ldx	#.sizeof (c256kcopycode) - 1
@L1:
        lda    	TRANSFERC256K,x
        sta	backup,x
        lda	c256kcopycode,x
        sta	TRANSFERC256K,x
        dex
        bpl	@L1
        rts

backup_and_setup_check_routine:
        ldx	#.sizeof (c256kcheckcode) - 1
@L1:
        lda	CHECKC256K,x
        sta	backup,x
        lda	c256kcheckcode,x
        sta	CHECKC256K,x
        dex
        bpl	@L1
        rts

restore_copy_routine:
        ldx	#.sizeof (c256kcopycode) - 1
restore_data:
        lda	backup,x
        sta	CHECKC256K,x
        dex
        bpl	restore_data
        rts

; Helper routine to correct for the bank and page
adjust_page_and_bank:
        sta	tmp4
        lda	#$0C
        sta     tmp3
        lda	tmp4
        and	#$c0
        lsr
        lsr
        ora	tmp3
        sta	tmp3
        txa
        asl
        asl
        asl
        asl
        asl
        asl
        ora	tmp3
        tax
        lda	tmp4
        and	#$3f
        rts
