;
; This must be the *second* file on the linker command line
; (.cvt header must be the *first* one)

; Maciej 'YTM/Elysium' Witkowiak
; 26.10.99, 10.3.2000, 15.8.2001, 23.12.2002

	.import		__RAM_START__, __RAM_SIZE__	; Linker generated
	.import		initlib, donelib
       	.import	       	pushax
	.import		callmain
	.import		_MainLoop, _EnterDeskTop
	.import		zerobss
	.importzp	sp
	.export		_exit

; ------------------------------------------------------------------------
; Create an empty LOWCODE segment to avoid linker warnings

.segment        "LOWCODE"

; ------------------------------------------------------------------------
; Place the startup code in a special segment.

.segment       	"STARTUP"

; Clear the BSS data

	jsr	zerobss

; Setup stack

	lda    	#<(__RAM_START__ + __RAM_SIZE__)
	sta	sp
	lda	#>(__RAM_START__ + __RAM_SIZE__)
       	sta	sp+1   		; Set argument stack ptr

; Call module constructors

	jsr	initlib

; Push arguments and call main()

	cli
       	jsr    	callmain

; Call module destructors. This is also the _exit entry which must be called
; explicitly by the code.

_exit:	jsr	donelib	 	; Run module destructors

	jmp	_EnterDeskTop	; return control to the system
