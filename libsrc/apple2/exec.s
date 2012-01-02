;
; Oliver Schmidt, 2011-01-26
;
; int __fastcall__ exec (const char* progname, const char* cmdline);
;

        .export 	_exec
        .import		pushname, popname
        .import		popax, done, _exit

        .include	"zeropage.inc"
        .include	"errno.inc"
        .include	"apple2.inc"
        .include	"mli.inc"

        ; Wrong file type
typerr: lda	#$4A		; "Incompatible file format"

        ; Cleanup name
oserr:  jsr	popname		; Preserves A
        
        ; Set __oserror
        jmp	__mappederrno

_exec:
        ; Get and push name
        jsr	popax
        jsr	pushname
        bne	oserr

        ; Set pushed name
        lda	sp
        ldx	sp+1
        sta	mliparam + MLI::INFO::PATHNAME
        stx	mliparam + MLI::INFO::PATHNAME+1

        ; Get file_type and aux_type
        lda	#GET_INFO_CALL
        ldx	#GET_INFO_COUNT
        jsr	callmli
        bcs	oserr

        ; If we get here the program file at least exists so we copy
        ; the loader stub right now and patch it later to set params
        ldx	#size - 1
:       lda	source,x
        sta	target,x
        dex
        bpl	:-

        ; Check program file type
        lda	mliparam + MLI::INFO::FILE_TYPE
        cmp	#$FF		; SYS file?
        bne	binary		; No, check for BIN file

        ; ProDOS TechRefMan, chapter 5.1.5.1:
        ; "The complete or partial pathname of the system program
        ;  is stored at $280, starting with a length byte."
        ldy	#$00
        lda	(sp),y
        tay
:       lda	(sp),y
        sta	$0280,y
        dey
        bpl	:-
        
        ; SYS programs replace BASIC.SYSTEM so set in the ProDOS system bit map
        ; protection for pages $80 - $BF just in case BASIC.SYSTEM is there now
        ldx	#$0F		; Start with protection for pages $B8 - $BF
        lda	#%00000001	; Protect only system global page
:       sta	$BF60,x		; Set protection for 8 pages
        lda	#$00		; Protect no page
        dex
        bpl	:-
        bmi	prodos		; Branch always

binary: cmp	#$06		; BIN file?
        bne	typerr		; No, wrong file type

        ; Set BIN program load addr
        lda	mliparam + MLI::INFO::AUX_TYPE
        ldx	mliparam + MLI::INFO::AUX_TYPE+1
        sta	data_buffer
        stx	data_buffer+1

        ; Check ProDOS system bit map for presence of BASIC.SYSTEM
        lda	$BF6F		; Protection for pages $B8 - $BF
        cmp	#%00000001	; Exactly system global page is protected
        beq	setvec

        ; Get highest available mem addr from BASIC.SYSTEM
        ldx	HIMEM+1		; High byte
        bne	setbuf		; Branch always

        ; BIN programs are supposed to quit through one of the two DOS
        ; vectors so we set up those to point to the ProDOS dispatcher
setvec: ldx	#$03 - 1	; Size of JMP opcode
:       lda	dosvec,x
        sta	DOSWARM,x	; DOS warm start
        sta	DOSWARM + 3,x	; DOS cold start
        dex
        bpl	:-

        ; No BASIC.SYSTEM so use addr of ProDOS system global page
prodos: ldx	#>$BF00		; High byte

        ; The I/O buffer needs to be page aligned
setbuf: lda	#$00		; Low byte

        ; The I/O buffer needs four pages
        dex
        dex
        dex
        dex
        
        ; Set I/O buffer
        sta	mliparam + MLI::OPEN::IO_BUFFER
        stx	mliparam + MLI::OPEN::IO_BUFFER+1

        ; PATHNAME already set
        .assert MLI::OPEN::PATHNAME = MLI::INFO::PATHNAME, error

        ; Lower file level to avoid program file
        ; being closed by C libary shutdown code
        ldx	LEVEL
        stx	level
        beq	:+
        dec	LEVEL
        
        ; Open file
:       lda	#OPEN_CALL
        ldx	#OPEN_COUNT
        jsr	callmli

        ; Restore file level
        ldx	level
        stx	LEVEL
        bcc	:+
        jmp	oserr

        ; Get and save fd
:       lda	mliparam + MLI::OPEN::REF_NUM
        sta	read_ref
        sta	close_ref

        .ifdef  __APPLE2ENH__
        ; Calling the 80 column firmware needs the ROM switched
        ; in, otherwise it copies the F8 ROM to the LC (@ $CEF4)
        bit	$C082

        ; ProDOS TechRefMan, chapter 5.3.1.3:
        ; "80-column text cards -- and other Apple IIe features -- can
        ;  be turned off using the following sequence of instructions:"
        lda	#$15
        jsr	$C300

        ; Switch in LC bank 2 for R/O
        bit	$C080
        .endif

        ; Call loader stub after C libary shutdown
        lda	#<target
        ldx	#>target
        sta	done
        stx	done+1

        ; Initiate C libary shutdown
        jmp	_exit

        .bss

level : .res	1

        .rodata

        ; Read whole program file
source: jsr	$BF00
        .byte	READ_CALL
        .word	read_param
        bcs	:+

        ; Close program file
        jsr	$BF00
        .byte	CLOSE_CALL
        .word	close_param
        bcs	:+

        ; Go for it ...
        jmp	(data_buffer)

read_param      = * - source + target
        .byte	$04		; PARAM_COUNT
read_ref        = * - source + target
        .byte	$00		; REF_NUM
data_buffer     = * - source + target
        .addr	$2000		; DATA_BUFFER
        .word	$FFFF		; REQUEST_COUNT
        .word	$0000		; TRANS_COUNT

close_param     = * - source + target
        .byte	$01		; PARAM_COUNT
close_ref       = * - source + target
        .byte	$00		; REF_NUM

        ; Quit to ProDOS dispatcher
quit            = * - source + target
:       jsr	$BF00
        .byte	$65		; QUIT
        .word	quit_param

quit_param      = * - source + target
        .byte	$04		; PARAM_COUNT
        .byte	$00		; QUIT_TYPE
        .word	$0000		; RESERVED
        .byte	$00		; RESERVED
        .word	$0000		; RESERVED

size            = * - source

target          = DOSWARM - size

dosvec: jmp	quit
