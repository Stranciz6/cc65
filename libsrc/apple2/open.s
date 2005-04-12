;
; Oliver Schmidt, 30.12.2004
;
; int open (const char* name, int flags, ...);
;
; Be sure to keep the value priority of closeallfiles lower than that of
; closeallstreams (which is the high level C file I/O counterpart and must be
; called before closeallfiles).

        .export 	_open, closedirect, freebuffer
        .destructor	closeallfiles, 17

        .import		pushname, popname
        .import 	errnoexit, oserrexit
        .import		__aligned_malloc, _free
        .import 	addysp, incsp4, pushax, popax

        .include	"zeropage.inc"
        .include	"errno.inc"
        .include	"fcntl.inc"
        .include	"mli.inc"
        .include	"filedes.inc"

_open:
        ; Throw away all parameters except name
        ; and flags occupying together 4 bytes
        dey
        dey
        dey
        dey
        jsr	addysp

        ; Start with first fdtab slot
        ldy	#$00

        ; Check for free fdtab slot
:       lda	fdtab + FD::REF_NUM,y
        beq	found

        .if	.sizeof(FD) = 4

        ; Advance to next fdtab slot
        iny
        iny
        iny
        iny

        .else
        .error	"Assertion failed"
        .endif

        ; Check for end of fdtab
        cpy	#MAX_FDS * .sizeof(FD)
        bcc	:-

        ; Load errno codes
        lda	#ENOMEM ^ EMFILE
enomem: eor	#ENOMEM

        ; Cleanup stack
        jsr	incsp4		; Preserves A

        ; Return errno
        jmp	errnoexit

        ; Save fdtab slot
found:  tya
        pha

        ; Alloc I/O buffer
        lda	#$00
        ldx	#>$0400
        jsr	pushax		; Preserves A
        ldx	#>$0100
        jsr	__aligned_malloc

        ; Restore fdtab slot
        pla
        tay

        ; Get and check I/O buffer high byte
        txa
        beq	enomem

        ; Set I/O buffer high byte (low byte remains zero)
        sta	fdtab + FD::BUFFER+1,y

        sty	tmp2		; Save fdtab slot

        ; Get and save flags
        jsr	popax
        sta	tmp3

        ; Get and push name
        jsr	popax
        jsr	pushname
        bne	oserr1

        ; Set pushed name
        lda	sp
        ldx	sp+1
        sta	mliparam + MLI::OPEN::PATHNAME
        stx	mliparam + MLI::OPEN::PATHNAME+1

        ; Check for create flag
        lda	tmp3		; Restore flags
        and	#O_CREAT
        beq	open

        .if	MLI::CREATE::PATHNAME = MLI::OPEN::PATHNAME

        ; PATHNAME already set

        .else
        .error	"Assertion failed"
        .endif

        ; Set all other parameters from template
        ldx	#(MLI::CREATE::CREATE_TIME+1) - (MLI::CREATE::PATHNAME+1) - 1
:       lda	CREATE,x
        sta	mliparam + MLI::CREATE::ACCESS,x
        dex
        bpl	:-

        ; Create file
        lda	#CREATE_CALL
        ldx	#CREATE_COUNT
        jsr	callmli
        bcc	open

        ; Check for ordinary errors
        cmp	#$47		; "Duplicate filename"
        bne	oserr2

        ; Check for exclusive flag
        lda	tmp3		; Restore flags
        and	#O_EXCL
        beq	open

        lda	#$47		; "Duplicate filename"
        
        ; Cleanup name
oserr2: jsr	popname		; Preserves A

oserr1: ldy	tmp2		; Restore fdtab slot

        ; Cleanup I/O buffer
        pha			; Save oserror code
        jsr	freebuffer
        pla			; Restore oserror code
        
        ; Return oserror
        jmp	oserrexit

open:   ldy	tmp2		; Restore fdtab slot

        ; Set allocated I/O buffer
        ldx	fdtab + FD::BUFFER+1,y
        sta	mliparam + MLI::OPEN::IO_BUFFER		; A = 0
        stx	mliparam + MLI::OPEN::IO_BUFFER+1

        ; Open file
        lda	#OPEN_CALL
        ldx	#OPEN_COUNT
        jsr	callmli
        bcs	oserr2

        ; Get and save fd
        ldx	mliparam + MLI::OPEN::REF_NUM
        stx	tmp1		; Save fd

        ; Set flags and check for truncate flag
        lda	tmp3		; Restore flags
        sta	fdtab + FD::FLAGS,y
        and	#O_TRUNC
        beq	done

        ; Set fd and zero size
        stx	mliparam + MLI::EOF::REF_NUM
        ldx	#$02
        lda	#$00
:       sta	mliparam + MLI::EOF::EOF,x
        dex
        bpl	:-

        ; Set file size
        lda	#SET_EOF_CALL
        ldx	#EOF_COUNT
        jsr	callmli
        bcc	done

        ; Cleanup file
        pha			; Save oserror code
        lda	tmp1		; Restore fd
        jsr	closedirect
        pla			; Restore oserror code
        bne	oserr2		; Branch always

        ; Store fd
done:   lda	tmp1		; Restore fd
        sta	fdtab + FD::REF_NUM,y

        .if	.sizeof(FD) = 4

        ; Convert fdtab slot to handle
        tya
        lsr
        lsr

        .else
        .error	"Assertion failed"
        .endif

        ; Cleanup name
        jsr	popname		; Preserves A
        
        ; Return success
        ldx	#$00
        rts

freebuffer:
        ; Free I/O buffer
        lda	#$00
        ldx	fdtab + FD::BUFFER+1,y
        jmp	_free

closeallfiles:
        ; All open files
        lda	#$00

closedirect:
        ; Set fd
        sta	mliparam + MLI::CLOSE::REF_NUM

        ; Call close
        lda	#CLOSE_CALL
        ldx	#CLOSE_COUNT
        jmp	callmli

        .rodata

CREATE: .byte	%11000011	; ACCESS:	Standard full access
        .byte	$06		; FILE_TYPE:	Standard binary file
        .word	$0000		; AUX_TYPE:	Load address N/A
        .byte	$01		; STORAGE_TYPE:	Standard seedling file
        .word	$0000		; CREATE_DATE:	Current date
        .word	$0000		; CREATE_TIME:	Current time
