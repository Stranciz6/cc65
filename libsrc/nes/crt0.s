;
; Startup code for cc65 (NES version)
;
; by Groepaz/Hitmen <groepaz@gmx.net>
; based on code by Ullrich von Bassewitz <uz@cc65.org>
;
; This must be the *first* file on the linker command line
;

        .export         _exit
	.import		initlib, donelib
	.import	        push0, _main, zerobss
        .import         ppubuf_flush

        ; Linker generated symbols
	.import		__RAM_START__, __RAM_SIZE__
	.import		__SRAM_START__, __SRAM_SIZE__
	.import		__ROM0_START__, __ROM0_SIZE__
	.import		__STARTUP_LOAD__,__STARTUP_RUN__, __STARTUP_SIZE__
	.import		__CODE_LOAD__,__CODE_RUN__, __CODE_SIZE__
	.import		__RODATA_LOAD__,__RODATA_RUN__, __RODATA_SIZE__
	.import		__DATA_LOAD__,__DATA_RUN__, __DATA_SIZE__
        
        .include        "zeropage.inc"
	.include        "nes.inc"


; ------------------------------------------------------------------------
; 16 bytes INES header

.segment        "HEADER"

;    +--------+------+------------------------------------------+
;    | Offset | Size | Content(s)                               |
;    +--------+------+------------------------------------------+
;    |   0    |  3   | 'NES'                                    |
;    |   3    |  1   | $1A                                      |
;    |   4    |  1   | 16K PRG-ROM page count                   |
;    |   5    |  1   | 8K CHR-ROM page count                    |
;    |   6    |  1   | ROM Control Byte #1                      |
;    |        |      |   %####vTsM                              |
;    |        |      |    |  ||||+- 0=Horizontal mirroring      |
;    |        |      |    |  ||||   1=Vertical mirroring        |
;    |        |      |    |  |||+-- 1=SRAM enabled              |
;    |        |      |    |  ||+--- 1=512-byte trainer present  |
;    |        |      |    |  |+---- 1=Four-screen mirroring     |
;    |        |      |    |  |                                  |
;    |        |      |    +--+----- Mapper # (lower 4-bits)     |
;    |   7    |  1   | ROM Control Byte #2                      |
;    |        |      |   %####0000                              |
;    |        |      |    |  |                                  |
;    |        |      |    +--+----- Mapper # (upper 4-bits)     |
;    |  8-15  |  8   | $00                                      |
;    | 16-..  |      | Actual 16K PRG-ROM pages (in linear      |
;    |  ...   |      | order). If a trainer exists, it precedes |
;    |  ...   |      | the first PRG-ROM page.                  |
;    | ..-EOF |      | CHR-ROM pages (in ascending order).      |
;    +--------+------+------------------------------------------+

        .byte   $4e,$45,$53,$1a	; "nes\n"
	.byte   2	        ; ines prg  - Specifies the number of 16k prg banks.
	.byte   1               ; ines chr  - Specifies the number of 8k chr banks.
	.byte   %00000011       ; ines mir  - Specifies VRAM mirroring of the banks.
	.byte   %00000000       ; ines map  - Specifies the NES mapper used.
	.byte   0,0,0,0,0,0,0,0	; 8 zeroes


; ------------------------------------------------------------------------
; Create an empty LOWCODE segment to avoid linker warnings

.segment        "LOWCODE"

; ------------------------------------------------------------------------
; Place the startup code in a special segment.

.segment       	"STARTUP"

start:

; setup the CPU and System-IRQ

        sei
        cld
	ldx     #0
	stx     VBLANK_FLAG

  	stx     ringread
	stx     ringwrite
	stx     ringcount

        txs

        lda     #$20
@l:     sta     ringbuff,x
	sta     ringbuff+$0100,x
	sta     ringbuff+$0200,x
        inx
	bne     @l

; Clear the BSS data

        jsr	zerobss

; Copy the .data segment to RAM

        lda     #<(__ROM0_START__ + __STARTUP_SIZE__+ __CODE_SIZE__+ __RODATA_SIZE__)
        sta     ptr1
        lda     #>(__ROM0_START__ + __STARTUP_SIZE__+ __CODE_SIZE__+ __RODATA_SIZE__)
        sta     ptr1+1
        lda     #<(__DATA_RUN__)
        sta     ptr2
        lda     #>(__DATA_RUN__)
        sta     ptr2+1

        ldx     #>(__DATA_SIZE__)

@l2:    beq     @s1    	        ; no more full pages

        ; copy one page
        ldy     #0
@l1:    lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     @l1

        inc     ptr1+1
        inc     ptr2+1
        dex
        bne     @l2

        ; copy remaining bytes
@s1:

        ; copy one page
        ldy     #0
@l3:    lda     (ptr1),y
        sta     (ptr2),y
        iny
        cpy     #<(__DATA_SIZE__)
        bne     @l3

; setup the stack

        lda     #<(__SRAM_START__ + __SRAM_SIZE__)
        sta	sp
        lda	#>(__SRAM_START__ + __SRAM_SIZE__)
       	sta	sp+1            ; Set argument stack ptr

; Call module constructors

	jsr	initlib

; Push arguments and call main()

       	jsr    	callmain

; Call module destructors. This is also the _exit entry.

_exit:  jsr	donelib		; Run module destructors

; Reset the NES

   	jmp start

; ------------------------------------------------------------------------
; System V-Blank Interupt
; updates PPU Memory (buffered)
; updates VBLANK_FLAG and _tickcount
; ------------------------------------------------------------------------

nmi:    pha
        tya
        pha
        txa
        pha

        lda     #1
        sta     VBLANK_FLAG

        inc     _tickcount
        bne     @s
        inc     _tickcount+1

@s:     jsr     ppubuf_flush

        ; reset the video counter
        lda     #$20
        sta     PPU_VRAM_ADDR2
        lda     #$00
        sta     PPU_VRAM_ADDR2

        ; reset scrolling
        sta     PPU_VRAM_ADDR1
        sta     PPU_VRAM_ADDR1

        pla
        tax
        pla
        tay
        pla

; Interrupt exit

irq2:
irq1:
timerirq:
irq:
        rti

; ------------------------------------------------------------------------
; hardware vectors
; ------------------------------------------------------------------------

.segment "VECTORS"

        .word   irq2        ; $fff4 ?
        .word   irq1        ; $fff6 ?
        .word   timerirq    ; $fff8 ?
        .word   nmi         ; $fffa vblank nmi
        .word   start	    ; $fffc reset
   	.word	irq         ; $fffe irq / brk

; ------------------------------------------------------------------------
; character data
; ------------------------------------------------------------------------

.segment "CHARS"

        .incbin "nes/neschar.bin"


