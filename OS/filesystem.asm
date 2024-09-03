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

;; filesystem.asm - file & directory operations

.include "os3.inc"
.import set_filename, setdevice, dev_writestr, dev_getc, dev_putc, dev_read
.import mkdir_command, rmdir_command, rm_command, cp_command, mv_command
.import dev_open, dev_close, set_filemode, _print_char, _print_hex
.export mkdir, rmdir, rm, cp, mv, load_program


; Create a new directory 
; AX points to a string with a pathname relative to /.
; Returns: P65_EOK or error code in A
; Modifies AXY, ptr1
.proc mkdir
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<mkdir_command
        ldx #>mkdir_command
        jmp dos_singlearg
.endproc


; Remove a directory, which must be empty
; AX points to a string with a pathname relative to /.
; Returns: P65_EOK or error code in A
; Modifies AXY, ptr1
.proc rmdir
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<rmdir_command
        ldx #>rmdir_command
        jmp dos_singlearg
.endproc


; Remove a file.
; AX points to a string with a pathname relative to /.
; Returns: P65_EOK or error code in A
; Modifies AXY, ptr1
.proc rm
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<rm_command
        ldx #>rm_command
        jmp dos_singlearg
.endproc


; helper for mkdir etc. expects operator string in AX,
; argument in DEVICE_FILENAME, and current device is SD command.
; Modifiees AXY, ptr1
.proc dos_singlearg
        jsr dev_writestr
        lda DEVICE_FILENAME
        ldx DEVICE_FILENAME+1
        jsr dev_writestr
   	lda #0
        jsr dev_putc		; and a marker for end of array-of-strings
        jsr dev_getc        ; read response code
        rts
.endproc


; Copy a file
; AX is the src filename, ptr2 contains the dest name.
; Returns: P65_EOK or error code in A
; Modifies AXY, ptr1
.proc cp
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<cp_command
        ldx #>cp_command
        jmp dos_doublearg
.endproc


; Move a file
; AX is the src filename, ptr2 contains the dest name.
; Unfortunately this is a copy and then delete...
; Returns: P65_EOK or error code in A
; Modifies AXY, ptr1
.proc mv
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<mv_command
        ldx #>mv_command
        jmp dos_doublearg
.endproc


; helper for cp & mv. expects operator string in AX,
; src argument in DEVICE_FILENAME, dest argument in ptr2
; and current device is SD command channel.
; Modifiees AXY, ptr1, ptr2
.proc dos_doublearg
        jsr dev_writestr    ; Write the command string which is in AX
        lda DEVICE_FILENAME
        ldx DEVICE_FILENAME+1
        jsr dev_writestr    ; Write source name
        lda ptr2
        ldx ptr2h
        jsr dev_writestr    ; Write dest name
   		lda #0              ; and a marker for end of array-of-strings
		jsr dev_putc		
        jsr dev_getc        ; read response code
        rts
.endproc



; Load a program into memory
; Program name in AX
; returns error code in AX
; Uses AXY, tmp1-3, ptr1, ptr2 (mostly for read)
.proc load_program
        jsr set_filename
	; setup
	lda #O_RDONLY
	jsr set_filemode
		
	lda #2
	jsr setdevice
	jsr dev_open
		
	; check return code which is in A - we want a regular file (1)
	cmp #0
	bmi error
	beq open_success
	cmp #1
	beq open_success
		
	lda #P65_EISDIR	; and fall thru to error handler

	; an error happened.  Print the command response & return to command line
error:
        ldx #0
        rts
		
open_success:
	; read file content on device 2
	lda #2
	jsr	setdevice
		
        
get1:	
        jsr 	dev_getc
	bcc	get1
	sta	program_address_low
	sta	ptr2
get2:	
        jsr	dev_getc
	bcc 	get2
	sta	program_address_high
	sta	ptr2h

        ; Read using dev_read
loop:
        lda ptr2        ; read wants buffer pointer in ptr1
        sta ptr1
        lda ptr2h
        sta ptr1h
        lda #0
        ldx #4 ; read 1k at a time?
        jsr dev_read    ; 0 in AX indicates EOF
.if 0
        ; print read count
        phx
        pha
        txa
        jsr _print_hex
        pla
        pha
        jsr _print_hex
        lda #' '
        jsr _print_char
        pla
        plx
.endif
        cpx #0
        bne check_for_error
        cmp #0
        beq done        ; Read end of file
check_for_error:
        cpx #$ff        ; $FFFF in AX indicates an error
        bne inc_loop
        cmp #$ff
        bne inc_loop
        ; there was an error during the read. Return P65_EIO
        lda #P65_EIO
        bra error
inc_loop:
        clc             ; add the current value in AX to ptr1/ptr1h.
        adc ptr2        ; Usually this will just be $40 except for
        sta ptr2        ; the last read of the file.
        txa             ; This also ensures ptr1 points at 1 past end
        adc ptr2h       ; of program when we finish.
        sta ptr2h
.if 0
        ; print current addr
        lda #'!'
        jsr _print_char
        lda ptr2h
        jsr _print_hex
        lda ptr2
        jsr _print_hex
        lda #' '
        jsr _print_char
.endif
        bra loop

done:
	jsr dev_close

	; figure out end address just so i can print it correctly
	;clc
	;tya
	;adc	ptr1
	;sta	program_end_low
	;lda	ptr1h
	;adc 	#0
	;sta	program_end_high
        lda ptr2
        sta program_end_low
        lda ptr2h
        sta program_end_high

        lda #P65_EOK  ; Set return code
        tax
        rts
.endproc
