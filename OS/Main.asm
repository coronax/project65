;; Project:65 OS 3
;; Copyright (c) 2013 Christopher Just
;; All rights reserved.
;;
;; Redistribution and use in source and binary forms, with or without 
;; modification, are permitted provided that the following conditions 
;; are met:
;;
;;    Redistributions of source code must retain the above copyright 
;;    notice, this list of conditions and the following disclaimer.
;;
;;    Redistributions in binary form must reproduce the above 
;;    copyright notice, this list of conditions and the following 
;;    disclaimer in the documentation and/or other materials 
;;    provided with the distribution.
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
;; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
;; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
;; FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
;; COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
;; INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
;; BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
;; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
;; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
;; STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
;; ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
;; OF THE POSSIBILITY OF SUCH DAMAGE.

.export _commandline, RESET, crlf, print_printable
;.export program_address_high, program_address_low
.export mkdir_command, rmdir_command, rm_command, cp_command, mv_command ; strings shared with filesystem
.import XModem, _print_string, _print_char, _read_char, _print_hex
.import setdevice, init_io, Max3100_IRQ, Max3100_TimerIRQ, SERIAL_PUTC
.import dev_getc, dev_writestr, dev_putc, dev_open, dev_close, set_filename, set_filemode, init_devices
.import dev_read
.import TokenizeCommandLine, test_tokenizer
.import mkdir, rmdir, rm, cp, mv, load_program

; TODO
;
; - Move command-line processing to its own file?
; - command-line tokenizer.
; - consistency pass re: register usage of I/O commands
; - sample programs using IO
; - redirection of command-line? irrelevant for now...
		
; This is the version actually written for Project:65,
; and not for a c64.  This version is working to be
; compatible with a max3100 SPI-to-serial chip.
; Test file for sending text back and forth to an arduino.
; Characters typed on the keyboard are transmitted to the F
; SPI slave, and text received from the slave is printed to
; the screen.
; This code uses a very simple bit-banging version of the
; SPI protocol, using 4 of the parallel port lines to
; implement the protocol
; Here's the pin mapping for the parallel port of the 
; user port:
;    PIN   BIT VALUE   USE              Arduino Uno pin
;     C      $01      Clock               13
;     D      $02      CS (chip select)    10
;     E      $04      Output              11
;     F      $08      Input               12

.include "os3.inc"


.code 

	;.byte "main.asm"
RESET:
    ; initialize
	SEI				; disable interrupts
    CLD             ; clear decimal mode
    LDX #$FF        ; empty stack
    TXS             ; set the stack

	; IO card test stuff
	LDA #$ff
	sta $A000
	sta $A001
	sta $A002
	lda #$fe
	sta $a000
	LDA #$fd
	sta $A000

	jsr WARMBOOT

	; Post-initialization RESET includes memtest & printing a banner.
    ldy #$ff
arbitrarydelay:
    dey
    bne arbitrarydelay

    printstring banner
    jsr memtest
    printstring banner2
	; and fall kind of gracelessly into _commandline
	jmp _commandline

SOFT_RESET:
        ; initialize
		SEI				; disable interrupts
        CLD             ; clear decimal mode
        LDX #$FF        ; empty stack
        TXS             ; set the stack
		JSR WARMBOOT
		jmp _commandline

WARMBOOT:

       ; setup irq & nmi vectors
        lda #$4c
        sta $0200
        sta $0203
        lda #<default_nmi
        sta $0201
        lda #>default_nmi
        sta $0202
        lda #<default_irq
        sta $0204
        lda #>default_irq
        sta $0205


        ;lda #0
        ;sta echo
        ;sta chartosend
        ;sta readbuffer

		jsr init_devices

		; initialize time of day clock
		stz tod_seconds
		stz tod_seconds+1
		stz tod_seconds+2
		stz tod_seconds+3
		stz tod_seconds100
		;lda #$64
		;sta tod_int_counter

		; initialize VIA Timer 1 to generate 100 Hz timer. At 4 MHz
		; that should require a 40000 ($9C40)clock timer.
		lda #$40
		sta VIA_TIMER1_CL
		lda #$9C
		sta VIA_TIMER1_CH
		lda VIA_ACR			; Set mode for continuous interrupts
		and #%00111111		; without disturbing the rest of ACR.
		ora #%01000000
		sta VIA_ACR

;		lda #%00100000		; PCR enable CB2 as negative-triggered interrupt
;		sta VIA_PCR
		
		lda #%11000000		; Set interrupt enable. Timer1 & not!CB2
		sta VIA_IER

		lda #$FF
		sta VIA_IFR			; clear all interrupt flags. CJBUG probably dumb.
		;sta VIA_DATAB		; and clear datab, maybe clears CB2 flag. also dumb.
		
		cli					; turn on interrupts
		rts



        
; command line parser routine
.proc _commandline
		cli						; Some of my programs disable interrupts, so just in case we
								; get returned to from one of those, let's re-enable.

        printstring prompt
        ldx #0
loop:
        jsr _read_char
        bcc loop
        jsr _print_char           ; echo
        sta buffer,x
        cmp #13                 ; carriage return
        beq outputsection
        cmp #10                 ; line feed
        beq outputsection
		cmp #08					; backspace. if we're not at the
		bne @skipbackspace		; start of buffer, decrement x
		cpx #0
		beq @skipbackspace
		dex
@skipbackspace:
        inx
        ; wraparound x
        txa
        and #$3F				; max 64-character line
        tax
        jmp loop

outputsection:
        lda #0
        sta buffer,x			; add terminating 0

		printstring crlf

		jsr TokenizeCommandLine

		lda argc				; if we have 0 arguments, just return to _commandline
		beq _commandline

		lda argv0L				; save argv to ptr1 so we can do string compares...
		sta ptr1
		lda argv0H
		sta ptr1h

.macro DispatchCommandLine string, fn
.local skip
		lda #<string
		ldx #>string
		jsr IsEqual
		beq skip
		jmp fn
skip:
.endmacro

		DispatchCommandLine g_command, process_g
		DispatchCommandLine run_command, process_run
		DispatchCommandLine x_command, process_x
		DispatchCommandLine b_command, process_b
		DispatchCommandLine m_command, process_m
		DispatchCommandLine load_command, ProcessLoadCommand
		DispatchCommandLine ls_command, ProcessLsCommand
		DispatchCommandLine mkdir_command, ProcessMkdirCommand
		DispatchCommandLine more_command, ProcessMoreCommand
		DispatchCommandLine rm_command, ProcessRmCommand
		DispatchCommandLine cp_command, ProcessCpCommand
		DispatchCommandLine mv_command, ProcessMvCommand
		DispatchCommandLine rmdir_command, ProcessRmdirCommand
		DispatchCommandLine save_command, ProcessSaveCommand
		DispatchCommandLine uptime_command, UptimeCommand

		; OK, well maybe it's a program to be run. We can try running from current directory or from /c
		lda argv0L
		ldx argv0H
		jsr load_program
		cmp #0
		bne error
		jmp execute_program
error:
		jsr perror
		jmp _commandline

		; Print a message for unrecognized command
;        printstring string1
;        printstring buffer
;        printstring string2

;        jmp _commandline	; and loop back to the start of the processor
.endproc

.proc execute_program
        printstring loadmsg
        lda program_address_high
        jsr _print_hex
        lda program_address_low
        jsr _print_hex
		printstring loadmsg2
        lda program_end_high
        jsr _print_hex
        lda program_end_low
        jsr _print_hex
        printstring crlf

		; at this point we should wait for the send buffer to
		; empty before starting execution of the program. This
		; is especially important for something like the insitu
		; loader, because it's going to blow up the IO system.
		
		
		; now we have to do another faux jsr into our program
		; push a fake return address onto the stack and then jmp
		lda #>program_return
		pha
		lda #<program_return
		pha
		jmp	(program_address_low)

program_return:
		nop
		jmp _commandline
		
loadmsg:   	.asciiz "Program located at $"
loadmsg2:	.asciiz " to $"
error:
		jsr perror
		jmp _commandline

		; Print a message for unrecognized command
;        printstring string1
;        printstring buffer
;        printstring string2

;        jmp _commandline	; and loop back to the start of the processor
.endproc


.rodata
; command names. A few of these are shared with filesystem.
b_command:		.asciiz "b"
g_command:		.asciiz "g"		
ls_command:		.asciiz "ls"
mkdir_command:	.asciiz "mkdir"
rmdir_command:	.asciiz "rmdir"
rm_command:		.asciiz "rm"
cp_command:     .asciiz "cp"
mv_command: 	.asciiz "mv"
m_command:		.asciiz "m"
more_command:	.asciiz "more"
load_command:	.asciiz "load"
save_command:	.asciiz "save"
uptime_command:	.asciiz "uptime"
x_command:		.asciiz "x"
run_command:    .asciiz "r"
.code



; Returns 1 in A if the string in AX equals the string in ptr1
; Uses: AY
;       ptr1, ptr2
.proc IsEqual
		sta ptr2
		stx ptr2h
		ldy #0
loop:	lda (ptr1),y
		cmp (ptr2),y
		bne done_false	; didn't match buffer, return false
		cmp #0
		beq done_true	; end of string, return true
		iny
		bra loop
done_true:
		lda #1
		rts
done_false:
		lda #0
		rts
.endproc 

;==============================================================
; Prints an error message for the error in A
;==============================================================
.proc perror
	and #$7f	; clear high bit
	pha
	jsr _print_hex
	lda #':'
	jsr _print_char
	lda #' '
	jsr _print_char
	pla
	asl
	tay
	lda errtable,y
	sta ptr1
	lda errtable+1,y
	sta ptr1h
	ldy #0
loop:
	lda (ptr1),y
	beq done
	jsr _print_char
	iny
	bra loop
done:
	printstring crlf
	rts
.rodata
m0:	 .asciiz "OK"
m1:  .asciiz "File not found"
m7:  .asciiz "Invalid parameter"
m9:  .asciiz "File exists"
m11: .asciiz "IO error"
m3:  .asciiz "Unknown"
m4:  .asciiz "Syntax error"
m5:  .asciiz "Not a directory"
m6:  .asciiz "Is a directory"
m22: .asciiz "Bad DOS cmd"
errtable:
.byte <m0, >m0
.byte <m1, >m1
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m7, >m7
.byte <m3, >m3
.byte <m9, >m9
.byte <m3, >m3
.byte <m11, >m11
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m3, >m3
.byte <m4, >m4
.byte <m5, >m5
.byte <m6, >m6
.byte <m22, >m22
.code

.endproc



; LS is still very oldschool and just expects a string of text from the device.
; This needs to be replaced with a proper directory reading routine.
.proc ProcessLsCommand
		lda #2
		jsr setdevice

		; we need to write each argument, separated by spaces
		lda argc
		cmp #1
		bne ckarg2
		; if there was only one arg, we need to stick a "/" string somewhere.
		; We can use the command buffer since we don't need it anymore.
		lda #'/'
		sta buffer
		stz buffer+1
		lda #<buffer
		ldx #>buffer
		jsr set_filename
		bra set_mode
ckarg2:
		cmp #2
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
		lda argv1L
		ldx argv1H
		jsr set_filename
set_mode:
		lda #O_RDONLY
		jsr set_filemode

		jsr dev_open
		; we should really check if the result is a directory...
		cmp #0
		bmi error
		beq read_loop
		cmp #2
		beq read_loop

		lda #P65_ENOTDIR
		bra error

		; OK, now we need some place to write a dirent to. Let's again use the
		; buffer. We need to read & process individual dirents until we hit
		; end of file.

read_loop:
		; I need a version of read...
		;lda #'*'
		;jsr sendchar

		lda #<buffer	; calling dev_read may change the value stored in 
		sta ptr1		; ptr1, so we always refresh it.
		lda #>buffer
		sta ptr1h

		lda #18
		ldx #0
		jsr dev_read
		cpx #0
		bne done
		cmp #18
		bne done

		; OK, filename is 1st element of dirent, should just be at buffer.
		printstring buffer
.if 0
		ldy #0
printloop:
		lda buffer,y
		beq doneprintname
		jsr sendchar
		iny
		bra printloop
doneprintname:
.endif
		printstring crlf
		bra read_loop
done:
		printstring donestring
		bra cleanup
donestring:
.asciiz "Done.\r\n"

error:
		jsr perror

cleanup:
		jsr dev_close
		jmp _commandline
.endproc



; handler for mkdir shell command
.proc ProcessMkdirCommand
		; there should be exactly 2 arguments
		lda argc
		cmp #2
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
		lda argv1L
		ldx argv1H
		jsr mkdir		; Call mkdir!
		cmp #0			; Check return code
		beq cleanup

error:
		jsr perror

cleanup:
		jmp _commandline
.endproc



; handler for rmdir shell command
.proc ProcessRmdirCommand
		; there should be exactly 2 arguments
		lda argc
		cmp #2
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
		lda argv1L
		ldx argv1H
		jsr rmdir		; Call mkdir!
		cmp #0			; Check return code
		beq cleanup

error:
		jsr perror

cleanup:
		jmp _commandline
.endproc



; handler for rm shell command. We want to allow multiple arguments. Maybe later?
.proc ProcessRmCommand
		; there should be exactly 2 arguments
		lda argc
		cmp #2
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
		lda argv1L
		ldx argv1H
		jsr rm			; Call rm!
		cmp #0			; Check return code
		beq cleanup

error:
		jsr perror

cleanup:
		jmp _commandline
.endproc



; handler for rm shell command. We want to allow multiple arguments. Maybe later?
.proc ProcessCpCommand
		; there should be exactly 2 arguments
		lda argc
		cmp #3
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
		lda argv2H		; let's try to stick arg2 in ptr2
		sta ptr2h
		lda argv2L
		sta ptr2
		lda argv1L		; arg1 in AX
		ldx argv1H
		jsr cp			; Call cp function!
		cmp #0			; Check return code
		beq cleanup

error:
		jsr perror

cleanup:
		jmp _commandline
.endproc



; handler for mv shell command. 
.proc ProcessMvCommand
		; there should be exactly 2 arguments
		lda argc
		cmp #3
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
		lda argv2H		; let's try to stick arg2 in ptr2
		sta ptr2h
		lda argv2L
		sta ptr2
		lda argv1L		; arg1 in AX
		ldx argv1H
		jsr mv			; Call cp function!
		cmp #0			; Check return code
		beq cleanup

error:
		jsr perror

cleanup:
		jmp _commandline
.endproc



.proc ProcessMoreCommand
		; setup
		lda #O_RDONLY
		jsr set_filemode
		lda argc
		cmp #2
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
		lda argv1L
		ldx argv1H
		jsr set_filename
		
		lda #3
		jsr setdevice
		jsr	dev_open
		
		; check return code which is in A. We want a stream (0) or regular file (1)
		cmp #0
		bmi error
		beq open_success
		cmp #1
		beq open_success
		
		lda #P65_EISDIR
error:
		; an error happened.  Print the command response & return to command line
		jsr perror
		bra cleanup
		
open_success:
		jsr PrintStream
		writedevice 0, crlf

cleanup:
		lda #3
		jsr setdevice
		jsr dev_close

		jmp _commandline
.endproc



.proc ProcessLoadCommand
		; setup
		lda #O_RDONLY
		jsr set_filemode
		lda argc
		cmp #2
		beq filename_ok
		lda #P65_ESYNTAX
		bra error
filename_ok:
		lda argv1L
		ldx argv1H
		jsr set_filename
		
		lda #2
		jsr setdevice
		jsr	dev_open
		
		; check return code which is in A - we want a regular file (1)
		cmp #0
		bmi error
		beq open_success
		cmp #1
		beq open_success
		
		lda #P65_EISDIR	; and fall thru to error handler

		; an error happened.  Print the command response & return to command line
	error:
		; A successful load will launch straight into the program, so we need to
		; do all the cleanup here.
		jsr perror
		;lda #2
		;jsr setdevice
		jsr dev_close
		jmp _commandline
		
open_success:
		; read file content on device 2
		lda #2
		jsr		setdevice
		
get1:	jsr 	dev_getc
		bcc		get1
		sta		program_address_low
		sta		ptr1
get2:	jsr		dev_getc
		bcc 	get2
		sta		program_address_high
		sta		ptr1h
		
		ldy		#0
loop:	jsr		dev_getc
		bcc		loop
		cpx		#$FF
		beq 	done
		sta		(ptr1),y

		iny
		bne		loop
		inc		ptr1h
		lda		#'.'
		jsr		SERIAL_PUTC
		bra		loop
done:
		jsr dev_close

		; figure out end address just so i can print it correctly
		clc
		tya
		adc		ptr1
		sta		program_end_low
		lda		ptr1h
		adc 	#0
		sta		program_end_high

        printstring loadmsg
        lda program_address_high
        jsr _print_hex
        lda program_address_low
        jsr _print_hex
		printstring loadmsg2
        lda program_end_high
        jsr _print_hex
        lda program_end_low
        jsr _print_hex
        printstring crlf

		; at this point we should wait for the send buffer to
		; empty before starting execution of the program. This
		; is especially important for something like the insitu
		; loader, because it's going to blow up the IO system.
		
		
		; now we have to do another faux jsr into our program
		; push a fake return address onto the stack and then jmp
		lda #>program_return
		pha
		lda #<program_return
		pha
		jmp	(program_address_low)

program_return:
		nop
		jmp _commandline
		
loadmsg:   	.asciiz "Program located at $"
loadmsg2:	.asciiz " to $"

.endproc



; not sure how to make this work because of xmodem overwriting the 
; command buffer. hm.  let's try just saving the current program in
; memory?  but buffer pos is still a problem!
.proc ProcessSaveCommand
		; setup
		lda #(O_WRONLY | O_TRUNC | O_CREAT)
		jsr set_filemode
		lda argc
		cmp #2
		beq filename_ok
		lda #P65_ESYNTAX
		bra error
filename_ok:
		lda argv1L
		ldx argv1H
		jsr set_filename
		
		lda #2				; We'll explicitly grab device #2, the 1st file slot.
		jsr setdevice
		jsr	dev_open
		
		; check return code which in A. We want a regular file (1).
		cmp #0
		;bmi error
		beq open_success
		cmp #1
		beq open_success

error:
		jsr perror
		bra cleanup

open_success:
		; read file content on device 2
		lda #2
		jsr		setdevice
		
		lda		program_address_low		; The first two bytes of the saved file are
		sta		ptr1					; the address where it should be loaded into
		jsr		dev_putc				; memory. Output those, and set ptr1 to the
		lda		program_address_high 	; start location
		sta		ptr1h
		jsr		dev_putc

loop:	lda		(ptr1)					; Loop & write data until we hit the
		jsr 	dev_putc				; program_end pointer.
		; compare to end of data
		lda		ptr1
		cmp		program_end_low
		bne		notdone
		lda		ptr1h
		cmp		program_end_high
		beq		cleanup
notdone:
		; increment ptr1
		inc		ptr1		
		bne		loop	; if zero, we need inc the high byte too
		inc		ptr1h
		bra		loop

cleanup:
		jsr		dev_close					; close device 2
		jmp 	_commandline
.endproc



.proc UptimeCommand
		lda tod_seconds+3
		jsr _print_hex
		lda tod_seconds+2
		jsr	_print_hex
		lda tod_seconds+1
		jsr _print_hex
		lda tod_seconds
		jsr _print_hex
		printstring msg
		jmp _commandline
msg:
.asciiz " seconds\r\n"
.endproc


; a klugey utility function that prints a file stream to the console,
; using hardcoded serial out.
.proc PrintStream
@loop:	jsr dev_getc
		bcc @loop
		cpx #$FF		; check for EOF
		beq @done2
		jsr _print_char
		cmp #10			; if we just wrote a newline
		bne @loop
		lda #13			; add a carriage return
		jsr _print_char
		bra @loop
@done2:	rts
.endproc


; add/remove breakpoint
.proc process_b
        ;cpx #1  ; one character = remove breakpoint_high
		lda argc
		cmp #1
        bne set_breakpoint_command
        ; 0 args - trying to remove breakpoint.
        jsr remove_breakpoint
        beq breakpoint_not_set
        printstring breakpoint_removed_msg
        jmp _commandline
breakpoint_not_set:
        printstring breakpoint_not_set_msg
        jmp _commandline
set_breakpoint_command:
		cmp #2
		beq syntax_ok1
        lda #P65_ESYNTAX
		jsr perror
		bra cleanup
syntax_ok1:
		lda argv1L
		ldx argv1H
        jsr parse_address
        sta ptr1        ; saving parsed value in ptr1
        stx ptr1h
        jsr remove_breakpoint ; try to remove old breakpoint
        ; now we can set new breakpoint
        lda ptr1
        ldx ptr1h
        sta breakpoint_low
        stx breakpoint_high
        ldy #0
        lda (ptr1),y
        sta breakpoint_value
        lda #00 ; BRK opcode
        sta (ptr1),y
        printstring breakpoint_set_msg
cleanup:
        jmp _commandline

breakpoint_removed_msg:
		.asciiz "\r\nBreakpoint removed\r\n"
breakpoint_not_set_msg:
		.asciiz "\r\nBreakpoint not set\r\n"
breakpoint_set_msg:
		.asciiz "\r\nBreakpoint set\r\n"

; If a breakpoint is set, remove it and return 1 in A.
; If not, return 0.
remove_breakpoint:
        lda breakpoint_low
        ldx breakpoint_high
        cmp #$FF
        bne remove_needed
        cpx #$FF
        bne remove_needed
        lda #0  ; return value 0 - nothing happened
        rts     ; remove not needed
remove_needed:
        sta ptr2
        stx ptr2h
        lda breakpoint_value
        ldy #0
        sta (ptr2),y
        ; put a junk value in breakpoint_low/high
        lda #$FF
        sta breakpoint_low
        sta breakpoint_high
        lda #1  ; return value 1 - a breakpoint was removed
        rts
.endproc



.proc process_m
		lda argc
		cmp #2
		beq syntax_ok
		lda #P65_ESYNTAX
		bra error
syntax_ok:
        ; ok, now parse a hexadecimal address
        ; and put it somewhere. um...
		lda argv1L
		ldx argv1H
        jsr parse_address
        sta ptr2
        stx ptr2h
		
		; print 8 rows of 8 characters of output
		; this is broken because print_hex uses x.
		ldx #8
line_loop:  
		phx
        lda #'m'
        jsr _print_char
        lda #'.'
        jsr _print_char

        lda ptr2h
        jsr _print_hex
        lda ptr2
        jsr _print_hex
        lda #' '
        jsr _print_char
        jsr _print_char

        ldy #0				; Print 8 bytes of memory in hex
lp1:    lda (ptr2),y
        jsr _print_hex
        lda #' '
        jsr _print_char
        iny
		cpy #8
		bne lp1
		
		ldy #0				; Print the same 8 bytes as actual 
lp2:	lda (ptr2),y		; characters.
		jsr print_printable
		iny
		cpy #8
		bne lp2
		
        ;printstring crlf
		lda #CR
		jsr _print_char
		lda #LF
		jsr _print_char

		clc					; increment loop
		lda ptr2
		adc #8
		sta ptr2
		lda ptr2h
		adc #0
		sta ptr2h
		plx
		dex
		bne line_loop
		lda #P65_EOK
error:
		jsr perror
cleanup:
		jmp _commandline	; return to command line parser
.endproc

		

; Utility for printing memory
; print the character if it's alphanumeric or punctuation.
; print a '.' for any control character
.proc print_printable
		cmp #$20
		bcc unprintable
		cmp #$7F
		bcc printable
		cmp #$A0
		bcc unprintable
		jmp printable	; a0 to FF - probably printable
unprintable:
		lda #'.'
printable:
		jmp _print_char
.endproc



;=============================================================================
; process_g
;=============================================================================
; Example: g 0500 [arg2] [arg3]
; Handler for the g command, which executes code at a given location.
; The code emulates an indirect jsr, so if the called code returns with an
; rts, control will return to the command parser.
; Argument is a hex address.
;=============================================================================
.proc process_g
		lda argc
		cmp #1
		bne read_address
use_default_address:
		lda program_address_low		; No arguments; use default program
		sta ptr2					; location.
		lda program_address_high
		sta ptr2h
		bra run_code
read_address:						; Read program start address from the 
		lda argv1L					; command line (argv[1]).
		ldx argv1H
        jsr parse_address
        sta ptr2
        stx ptr2h
run_code:		
		; Fake an indirect jsr by pushing return address to stack manually 
		; and then doing an indirect jmp.
		lda #>program_return
		pha
		;jsr _print_hex
		lda #<program_return
		pha
		;jsr _print_hex
		stz program_ret		; Initialize program return value.
		jmp	(ptr2)
program_return:
		nop					; The jsr return will skip over this instruction.
		printstring return_msg
		lda program_ret
		jsr _print_hex
		printstring crlf
		; Jumping to soft reset at the end of the program is the safest option
		; because it guarantees we'll be in a known state regardless of how
		; messily the program ended. But any output still in the write buffer
		; will be cut off.
		;jmp SOFT_RESET 
		; So for now we'll try the less safe option of just returning into the
		; command line routine.
cleanup:
		jmp _commandline

return_msg:	.asciiz "Program returned "
.endproc



;=============================================================================
; process_run
;=============================================================================
; Example: r [arg1] [arg2] [arg3]
; Handler for the r command, which executes the currently loaded program
; The code emulates an indirect jsr, so if the called code returns with an
; rts, control will return to the command parser.
; Effectively this is like g, without specifying memory location
;=============================================================================
.proc process_run
		lda argc
;		cmp #1
;		bne read_address
;use_default_address:
		lda program_address_low		; No arguments; use default program
		sta ptr2					; location.
		lda program_address_high
		sta ptr2h
;		bra run_code
;read_address:						; Read program start address from the 
;		lda argv1L					; command line (argv[1]).
;		ldx argv1H
;        jsr parse_address
;        sta ptr2
;        stx ptr2h
;run_code:		
		; Fake an indirect jsr by pushing return address to stack manually 
		; and then doing an indirect jmp.
		lda #>program_return
		pha
		lda #<program_return
		pha
		stz program_ret		; Initialize program return value.
		jmp	(ptr2)
program_return:
		nop					; The jsr return will skip over this instruction.
		;printstring return_msg
		;lda program_ret
		;jsr _print_hex
		;printstring crlf
		; Jumping to soft reset at the end of the program is the safest option
		; because it guarantees we'll be in a known state regardless of how
		; messily the program ended. But any output still in the write buffer
		; will be cut off.
		;jmp SOFT_RESET 
		; So for now we'll try the less safe option of just returning into the
		; command line routine.
;cleanup:
		jmp _commandline
;return_msg:	.asciiz "Program returned "
.endproc



;=============================================================================
; process_x
;=============================================================================
; Handles the x command - initiates an XModem download.
; program_address and program_end are filled in with the start and ending
; locations of the program.
;=============================================================================
.proc process_x
		stz program_address_low
		stz program_address_high
        printstring crlf
        jsr XModem
        cmp #$FE
        beq xmodem_escaped
        cmp #$FD
        beq xmodem_error
        cmp #$FC
        beq xmodem_error
        printstring crlf
        printstring xmsg
        lda program_address_high
        jsr _print_hex
        lda program_address_low
        jsr _print_hex
		printstring xmsg2
        lda program_end_high
        jsr _print_hex
        lda program_end_low
        jsr _print_hex
        printstring crlf
		; So how can we jsr into our downloaded program?  
		; One option is to push the address onto the stack and then rts
		; to jump back.
		; First, we push the return address of our faux-jsr, high-byte 
		; first.  Note that we'll return to one byte past the value we
		; store, which is why I have that NOP down there.
		; Then we push the address we want to jsr to (minus 1),
		; again pushing the high byte first, and then we rts.
		; The rts will jump into the program we want to execute, and the
		; rts at the end of the program will jump back to program_return
		; (plus 1).
		
		; fake an indirect jsr by pushing return address manually 
		; and then doing an indirect jmp
;		lda #>program_return
;		pha
;		lda #<program_return
;		pha
;		jmp	(program_address_low)

program_return:
		nop
		jmp _commandline
		
xmodem_escaped:
        printstring crlf
        printstring xmodem_escaped_msg
        jmp _commandline
xmodem_error:
        printstring crlf
        printstring xmodem_error_msg
        jmp _commandline
		
xmsg:   .asciiz "Program located at $"
xmsg2:	.asciiz " to $"
xmodem_escaped_msg:
        .asciiz "Quitting XModem upload\r\n"
xmodem_error_msg:
        .asciiz "Error during XModem upload\r\n"		
.endproc

		
crlf:   .byte CR, LF, 0


buf_ptr = ptr1
result_ptr = ptr2
; parses a string of 4 hex characters to a 2-byte address
; x contains the high byte of the string pointer
; a contains the low byte of the string pointer
; x returns the high byte of the parsed address
; a returns the low byte of the parsed address
.proc parse_address
			sta buf_ptr
			stx buf_ptr+1
			ldy #0
			lda (buf_ptr),y
			;lda #101
			jsr parse_hexit
			asl
			asl
			asl
			asl
			sta result_ptr+1
			iny
			lda (buf_ptr),y
			;lda #48
			jsr parse_hexit
			ora result_ptr+1
			sta result_ptr+1
			iny
			lda (buf_ptr),y
			;lda #48
			jsr parse_hexit
			asl
			asl
			asl
			asl
			sta result_ptr
			iny
			lda (buf_ptr),y
			;lda #48
			jsr parse_hexit
			ora result_ptr
			sta result_ptr
			ldx result_ptr+1
			lda result_ptr
			rts
.endproc

; parse a hex digit.  A contains a character (0-9,A-F,a-f)
; A returns the value of that hexit
; Uses only A.
.proc parse_hexit
		cmp #97				; 'a'
		bpl lowercase
		cmp #65				; 'A"
		bpl uppercase
		; default: assume it's between '0' and '9'
		sec
		sbc #48 			; '0'
		rts
lowercase:
		sec
		sbc #87				; 'a' - 10
		rts
uppercase:
		sec
		sbc #55				; 'A' - 10
		rts
.endproc
.if 0
parse_hexit:
			sec
			sbc #48         ;"0"
			cmp #11
			bcs parse_hexit_1
			rts
parse_hexit_1:
			sec
			sbc #17         ;"A"-"0"
			cmp #6
			bcs parse_hexit_2
			clc
			adc #10
			rts
parse_hexit_2:
			sec
			sbc #32         ;"a"-"A"
			cmp #6
			bcs parse_hexit_3
			clc
			adc #10
			rts
parse_hexit_3:
			lda #0
			rts
.endif



banner:
        .byte ESC, "[2J"         ; clear screen
        .byte ESC, "[;H"         ; home
		.byte 201,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,187,CR,LF
        .byte 186,"   Project:65 Computer   ",186,CR, LF
        .byte 186,"   v0.08 (Jan 2024)      ",186,CR, LF
		.byte 199,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,196,182,CR,LF,0

banner2:
		.asciiz "\r\nMonitor ready\r\n"


string1:
		.byte CR, LF, ESC, "[31mYOU SAID ", '"', 0

string2: .byte '"', ESC, "[37m", CR, LF, 0


ram_test_string1:
        .byte 186,"   RAM: $0000-$", 0
ram_test_string2:
        .byte "      ",186,CR, LF, 186,"   ROM: $E000-$FFFF      ",186, CR, LF
		.byte 200,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,205,188,CR,LF,CR,LF,0 

prompt:
        .asciiz "> " 





.proc default_nmi
		rti
.endproc

.proc default_irq
		pha
		phx
		phy

		lda VIA_IFR
		rol ; irq flag
		bcc max3100_irq	; not really 6522 - max3100
		rol ; timer1
		bcs s6522_timer1
		rol ; timer2
		rol ; cb1
		rol ; cb2
		bcs max3100_irq	; max3100 via cb2
		bra done

s6522_timer1:	
		; if it was, the only thing we're doing now is timer1
		lda VIA_TIMER1_CL	; read to clear interrupt flag
		;dec tod_int_counter
		;bne done_timer
		;lda #$64	; so we update counter 1 every 100th interrupt
		;sta tod_int_counter

		inc tod_seconds100
		lda tod_seconds100
		cmp #100
		bne done_timer
		stz tod_seconds100	; 
		inc tod_seconds
		bne done_timer		; if we rollover to 0, inc counter+1 also.
		inc tod_seconds+1
		bne done_timer
		inc tod_seconds+2
		bne done_timer
		inc tod_seconds+3

done_timer:
		jsr Max3100_TimerIRQ
.if 0
		; also in the timer interrupt, check the write buffer
		; Eventually this should be removed and replaced by
		; smart handling of the 3100's transmit interrupt.
		COUNTBUFFER wbuffer
		beq done_write
		jsr Max3100_SendRecv
.endif
done_write:

		; lda VIA_TIMER1_CL	; read to clear interrupt flag
		bra done

max3100_irq:
		; This interrupt came from the MAX3100 UART.
		; It indicates that there is data to be read.
		jsr Max3100_IRQ

done:
		ply
		plx
		pla
		rti
.endproc




; finds the top of RAM and prints an info
; message.  Starts at $0401, and
; doesn't modify memory below that. (besides
; a couple bytes of zero page).
; Based on ehbasic memtest code
ADDR = $42
memtest:
			lda 	#$00
			sta 	ADDR
			lda 	#$04
			sta 	ADDR+1
			ldy 	#$01        	; Y is only here so we can use indirect address
        
LAB_2D93:
			inc 	ADDR            ; increment temporary integer low byte
			bne 	LAB_2D99        ; branch if no overflow
			inc 	ADDR+1          ; increment temporary integer high byte
			lda 	ADDR+1
			beq 	endmemtest  	; wrapped around to start of memory

LAB_2D99:
			lda 	#$55        	; set test byte
			sta 	(ADDR),y     	; save via temporary integer
			cmp 	(ADDR),y     	; compare via temporary integer
			bne 	endmemtest  	; branch if fail

			asl             		; shift test byte left (now $AA)
			sta 	(ADDR),y     	; save via temporary integer
			cmp 	(ADDR),y     	; compare via temporary integer
			beq 	LAB_2D93    	; if ok go do next byte

endmemtest:
			printstring ram_test_string1
			lda 	ADDR+1
			jsr 	_print_hex
			lda 	ADDR
			jsr 	_print_hex
			printstring ram_test_string2
			rts



