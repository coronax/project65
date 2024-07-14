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

; TTY Driver. Essentially, this is going to sit on top of the serial
; device and can be switched between raw and cooked modes. We'll start
; with a simple raw mode with the option to echo characters back to the
; output, which is what I most need just at the moment.

.import dev_getc, dev_putc, setdevice, sendchar
.export TTY_IOCTL, TTY_OPEN, TTY_CLOSE, TTY_GETC

.include "os3.inc"

; Support code for cooked-mode buffer named ttybuffer. 
; With pointers:
;    wr_ttybuffer points at first free spot for writing
;    rd_ttybuffer points at next character to read
; if wr_ttybuffer == rd_ttybuffer then the buffer is empty. 
; if wr_ttybuffer is one to the left of rd_ttybuffer, buffer is full, so
; we do waste a char in that case.
; We also need
;    cl_ttybuffer, pointer to start of current line being edited.
; Which starts = to wr_ttybuffer, and is advanced whenever the user
; types enter.
; Read can't read cl_ttybuffer or anywhere past it.

; CJ BUG these routines below only work for 128 byte buffers.

.define wrptr(buffername) .ident(.concat("wr_", .string(buffername)))
.define rdptr(buffername) .ident(.concat("rd_", .string(buffername)))
.define clptr(buffername) .ident(.concat("cl_", .string(buffername)))

; Initialize buffer to empty
.macro INITBUFFER buffername
		STZ wrptr buffername
		STZ rdptr buffername
                stz clptr buffername
.endmacro
		
; Write A to the write buffer
; Note that this doesn't test if there's enough room.
; Modifies X
.macro WRITEBUFFER buffername
		LDX wrptr buffername
		STA buffername, X
		INC wrptr buffername
		RMB7 wrptr buffername
.endmacro

; Reads from the read buffer to A.
; doesn't check if there's available data.
; Modifies AX
.macro READBUFFER buffername
		LDX rdptr buffername
		LDA buffername, X
		INC rdptr buffername
		RMB7 rdptr buffername
.endmacro

; Returns count of items in buffer in A
; Uses A
.macro COUNTBUFFER buffername
		LDA wrptr buffername
		SEC
		SBC rdptr buffername
		AND #%01111111
.endmacro

; COUNTAVAIL, in cooked mode, is the # of bytes 
; available to be read. ie number of bytes until start of the line
; currently being edited.
.macro COUNTAVAIL buffername
		LDA clptr buffername
		SEC
		SBC rdptr buffername
		AND #%01111111
.endmacro

; COUNTCL, in cooked mode, is # of bytes in the line currently being edited.
.macro COUNTCL buffername
		LDA wrptr buffername
		SEC
		SBC clptr buffername
		AND #%01111111
.endmacro

IO_INIT        = 0
IO_ECHO_ON     = 32
IO_ECHO_OFF    = 33
IO_RAW_MODE    = 34
IO_COOKED_MODE = 35

;=============================================================================
; TTY_IOCTL
; IO Control for tty driver
;=============================================================================
.proc TTY_IOCTL
	cmp #IO_INIT
	beq init
        cmp #IO_ECHO_ON
        beq echo_on
        cmp #IO_ECHO_OFF
        beq echo_off
        cmp #IO_RAW_MODE
        beq raw_mode
        cmp #IO_COOKED_MODE
        beq cooked_mode
error:
	; This would probably be EINVAL if we had a way to set errno
	lda #$FF
	tax
	rts

init:   jmp TTY_INIT

echo_on:
        lda #1
        sta TTY + TTY_BLOCK::ECHO
        lda #0
        tax
        rts

echo_off:
        stz TTY + TTY_BLOCK::ECHO
        lda #0
        tax
        rts

raw_mode:
        stz TTY + TTY_BLOCK::MODE       ; 0 for raw mode
        lda #0
        tax
        rts

cooked_mode:
        lda #1                          ; 1 for cooked mode
        sta TTY + TTY_BLOCK::MODE
        INITBUFFER ttybuffer            ; throw out previously buffered IO
        lda #0
        tax
        rts
.endproc



.proc TTY_INIT
        lda #1
        sta TTY + TTY_BLOCK::MODE
        lda #1
        sta TTY + TTY_BLOCK::ECHO
        stz TTY + TTY_BLOCK::IN_DEV
        stz TTY + TTY_BLOCK::OUT_DEV
        lda #4
        sta TTY + TTY_BLOCK::SELF_DEV
        stz TTY + TTY_BLOCK::EOF

        INITBUFFER ttybuffer
        lda #0
        tax
        rts
.endproc



.proc TTY_OPEN
        lda #1
        sta TTY + TTY_BLOCK::MODE ; open in cooked mode
        lda #1
        sta TTY + TTY_BLOCK::ECHO ; echo on
        stz TTY + TTY_BLOCK::EOF  ; clear eof
        INITBUFFER ttybuffer
        lda #'1'                ; we need to fix this return value at some point
        ldx #0
        rts
.endproc

.proc TTY_CLOSE
        rts
.endproc

.proc TTY_GETC
        lda TTY + TTY_BLOCK::MODE
        bne cooked
        jmp TTY_GETC_RAW
cooked:
        jmp TTY_GETC_COOKED
.endproc



.proc TTY_GETC_RAW
        lda TTY + TTY_BLOCK::IN_DEV
        jsr setdevice
        jsr dev_getc                    ; Read a character from the input device.
        bcc return_nothing              ; No character available, so we just return

        sta TTY + TTY_BLOCK::TMPA
        stx TTY + TTY_BLOCK::TMPX
        cpx #$FF                        ; Test for EOF.
        bne not_eof
        stx TTY + TTY_BLOCK::EOF        ; mark EOF. This also counts as an end of line,
        lda wr_ttybuffer                ; so we should update cl_ttybuffer like if we'd
        sta cl_ttybuffer                ; read a CR.
        bra return_w_carry              ; need to echo anything.

not_eof:
        lda TTY + TTY_BLOCK::ECHO
        beq return_w_carry              ; echo disabled, skip to return

        ; echo is on, so write to out
        lda TTY + TTY_BLOCK::OUT_DEV
        jsr setdevice
        lda TTY + TTY_BLOCK::TMPA
        jsr dev_putc                    ; Echo the character to output device.
        cmp #13 ;'\r'                   ; When we output a carriage return, 
        bne return_w_carry              ; we insert a newline afterwards.
        lda #10 ;'\n'
        jsr dev_putc

return_w_carry:
        lda TTY + TTY_BLOCK::SELF_DEV
        jsr setdevice                   ; Make TTY Device active again
        lda TTY + TTY_BLOCK::TMPA       ; Load the return value to AX
        ldx TTY + TTY_BLOCK::TMPX
        sec                             ; Set carry when we return a character
        rts

return_nothing:
        lda TTY + TTY_BLOCK::SELF_DEV
        jsr setdevice
        clc                             ; Clear carry to indicate no return
        rts
.endproc



.proc TTY_GETC_COOKED
        ; in cooked mode, we still need to read a character from in_dev and
        ; process it, but it may not be the character we return (and we may
        ; not return anything, if we're waiting for an enter).
        ; To prevent a deadlock, if the buffer is full we might need to 
        ; overwrite the last char? Or simply declare that the end of line?
        ; Note that the processing here is entirely dependent on the client
        ; application calling getc, which means it can't do any interrupt-
        ; style processing like Ctrl-C. To fix that we would probably need
        ; something interrupt based.

        ; we need to save y because we clobber it during the echo part
        phy

        ; first off, if we've already received an EOF, we don't need to do
        ; anything on the input side
        lda TTY + TTY_BLOCK::EOF
        beq not_eof
        jmp return_char
not_eof:

        lda TTY + TTY_BLOCK::IN_DEV
        jsr setdevice

        ; Is there any available room in the buffer for a character?
        COUNTBUFFER ttybuffer
        cmp #126
        bmi have_room_1         ; If there's no room in the buffer, we return
        jmp return_char         ; an existing char and try again later.
have_room_1:
        jsr dev_getc            ; Read a new character from the device!
        bcs skip1
        jmp return_char         ; no new character available
skip1:
        sta TTY + TTY_BLOCK::TMPA       ; Save newly-read character
        stx TTY + TTY_BLOCK::TMPX
        cpx #$FF                        ; Is new character an EOF?
        bne not_eof2
        stx TTY + TTY_BLOCK::EOF  ; mark that we've hit EOF
        jmp return_char
not_eof2:

        ; We have the newest read character in A (& TMPA). What do we do with it?

        ; If the current line is too long we might starve. So we'll set a maximum
        ; length of 63 bytes plus room for a cr. This works with the command-line
        ; buffer length limit in the kernel.

        ; First let's check for a backspace
        cmp #8  ; backspace
        beq HandleBackspace

        cmp #13 ; carriage return
        beq HandleCR

        cmp #27 ;esc
        beq ReadEscapeSequence


        ; We're reading a regular character that goes in the buffer.
        ; Though we could consider filtering out other nonprintables, but
        ; I'm not going to worry about that right now.

        ; check if we have room in the current line for a regular character
        ; If not, we'll reject it and output a beep
        
        COUNTCL ttybuffer
        cmp #63
        bpl echo_bell2                  ; No room! Toss the character & send bell.
        lda TTY + TTY_BLOCK::TMPA
        WRITEBUFFER ttybuffer
        lda TTY + TTY_BLOCK::ECHO       ; and see if we need to echo
        beq return_char
        lda TTY + TTY_BLOCK::OUT_DEV
        jsr setdevice
        lda TTY + TTY_BLOCK::TMPA
        jsr dev_putc
        bra return_char



; we need to handle a backspace character. If there are any characters in 
; current line, we can roll back wrptr and store a 0.
HandleBackspace:
        COUNTCL ttybuffer
        beq echo_bell2         ; No characters to delete, so we echo a bell.
        lda wr_ttybuffer
        dec
        and #%01111111
        sta wr_ttybuffer
        lda TTY + TTY_BLOCK::ECHO
        beq return_char
        lda TTY + TTY_BLOCK::OUT_DEV
        jsr setdevice
        lda #8 ; backspace char
        jsr dev_putc
        lda #' '
        jsr dev_putc
        lda #8
        jsr dev_putc
        bra return_char
        

echo_bell2:
        lda TTY + TTY_BLOCK::ECHO
        beq return_char
        lda TTY + TTY_BLOCK::OUT_DEV
        jsr setdevice
        lda #7
        jsr dev_putc
        bra return_char



ReadEscapeSequence:
        ; Entering this function, we'll have already read the esc character.
        ; For now, I want to focus on recognizing cursor keys and ignoring
        ; them so that they don't mess up line editing. We can think about
        ; actually supporting left/right cursor some other time. So
        ; we're looking for [ followed by A, B, C, or D. This could get 
        ; really complex if we wanted to be complete, but I don't think
        ; that's necessary.
get1:
        jsr dev_getc
        bcc get1
        cmp #'['
        bne done
get2:
        jsr dev_getc
        bcc get2
done:
        bra return_char


HandleCR:
        ; We need to handle a carriage return. This makes the current line available,
        ; which is to say we put the character in the buffer and make the new write
        ; pos cl_ttybuffer
        WRITEBUFFER ttybuffer
        lda wr_ttybuffer
        sta cl_ttybuffer        ; advance the start of 'current' line to after the CR
        ; If echo is set, we need to write both CR and newline
        lda TTY + TTY_BLOCK::ECHO
        beq return_char
        lda TTY + TTY_BLOCK::OUT_DEV
        jsr setdevice
        lda #13;'\r'
        jsr dev_putc
        lda #10;'\n'
        jsr dev_putc
        ; And we'll just fall thru to return_char
        ;bra return_char


return_char:
        ; is there an available character to return?
        ; make tty the active device again
        lda TTY + TTY_BLOCK::SELF_DEV
        jsr setdevice

        ; Is there an available character to return?
        COUNTAVAIL ttybuffer
        bne return_char_from_buffer
        ; If ttybuffer is empty and EOF is set, we should return
        ; EOF. Otherwise, just return with carry clear
        lda TTY + TTY_BLOCK::EOF
        bne return_eof
        ply
        clc
        rts     ; no character available, just return with carry clear
return_eof:
        lda #$FF
        tax
        ply     ; remember we pushed Y way at the start of this
        sec
        rts
return_char_from_buffer:
        READBUFFER ttybuffer
        ;jsr sendchar
        ldx #0
        ply     ; remember we pushed Y way at the start of this
        sec
        rts     ; And finally return from TTY_GETC_COOKED!



.endproc


.if 0
.proc TTY_PUTC
        sta TTY + TTY_BLOCK::TMPA
        stx TTY + TTY_BLOCK::TMPX
        lda TTY + TTY_BLOCK::OUT_DEV
        sta CURRENT_DEVICE
        lda TTY + TTY_BLOCK::OUT_CHAN
        sta DEVICE_CHANNEL
        lda TTY + TTY_BLOCK::OUT_OFFSET
        sta DEVICE_OFFSET
        lda TTY + TTY_BLOCK::TMPA
        ldx TTY + TTY_BLOCK::TMPX
        jsr dev_putc
        ; make tty the active device again
        lda TTY + TTY_BLOCK::SELF_DEV
        sta CURRENT_DEVICE
        lda TTY + TTY_BLOCK::SELF_CHAN
        sta DEVICE_CHANNEL
        lda TTY + TTY_BLOCK::SELF_OFFSET
        sta DEVICE_OFFSET
        lda TTY + TTY_BLOCK::TMPA
        ldx TTY + TTY_BLOCK::TMPX
        rts
.endproc
.endif
