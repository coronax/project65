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

.import dev_getc, dev_putc
.export TTY_INIT, TTY_OPEN, TTY_CLOSE, TTY_GETC, TTY_PUTC

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

.proc TTY_INIT
        lda #1
        sta TTY + TTY_BLOCK::MODE
        lda #1
        sta TTY + TTY_BLOCK::ECHO
        stz TTY + TTY_BLOCK::IN_DEV
        stz TTY + TTY_BLOCK::IN_CHAN
        stz TTY + TTY_BLOCK::IN_OFFSET
        stz TTY + TTY_BLOCK::OUT_DEV
        stz TTY + TTY_BLOCK::OUT_CHAN
        stz TTY + TTY_BLOCK::OUT_OFFSET
        lda #4
        sta TTY + TTY_BLOCK::SELF_DEV
        stz TTY + TTY_BLOCK::SELF_CHAN
        lda #64
        sta TTY + TTY_BLOCK::SELF_OFFSET

        INITBUFFER ttybuffer
        rts
.endproc

.proc TTY_OPEN
        INITBUFFER ttybuffer
        rts
.endproc

.proc TTY_CLOSE
        rts
.endproc

.proc TTY_GETC
        lda TTY + TTY_BLOCK::ECHO
        bne cooked
        jmp TTY_GETC_RAW
cooked:
        jmp TTY_GETC_COOKED
.endproc

.proc TTY_GETC_RAW
        lda TTY + TTY_BLOCK::IN_DEV
        sta CURRENT_DEVICE
        lda TTY + TTY_BLOCK::IN_CHAN
        sta DEVICE_CHANNEL
        lda TTY + TTY_BLOCK::IN_OFFSET
        sta DEVICE_OFFSET
        jsr dev_getc
        sta TTY + TTY_BLOCK::TMPA
        stx TTY + TTY_BLOCK::TMPX
        bcc return              ; No character read, no echo, no carry
        cpx #$FF
        beq return_w_carry      ; read eof, no echo
        lda TTY + TTY_BLOCK::ECHO
        beq return_w_carry      ; echo off, no echo
        ; echo is on, so write to out
        lda TTY + TTY_BLOCK::OUT_DEV
        sta CURRENT_DEVICE
        lda TTY + TTY_BLOCK::OUT_CHAN
        sta DEVICE_CHANNEL
        lda TTY + TTY_BLOCK::OUT_OFFSET
        sta DEVICE_OFFSET
        lda TTY + TTY_BLOCK::TMPA
        ldx TTY + TTY_BLOCK::TMPX
        jsr dev_putc
        cmp #13;'\r'            ; Echo a newline after a carriage return.
        bne return_w_carry
        lda #10;'\n'
        jsr dev_putc
return_w_carry:
        sec                     ; We read a character, so we need to return with carry set
return:
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


.proc TTY_GETC_COOKED
        ; in cooked mode, we still need to read a character from in_dev and
        ; process it, but it may not be the character we return (and we may
        ; not return anything, if we're waiting for an enter).
        ; To prevent a deadlock, if the buffer is full we might need to 
        ; overwrite the last char? Or simply declare that the end of line?
        lda TTY + TTY_BLOCK::IN_DEV
        sta CURRENT_DEVICE
        lda TTY + TTY_BLOCK::IN_CHAN
        sta DEVICE_CHANNEL
        lda TTY + TTY_BLOCK::IN_OFFSET
        sta DEVICE_OFFSET
        jsr dev_getc
        ;bcc return_char         ; no character to process
        bcs skip1
        jmp return_char
skip1:
        sta TTY + TTY_BLOCK::TMPA
        stx TTY + TTY_BLOCK::TMPX
        ;bcc return              ; No character read, no echo, no carry
        cpx #$FF
        beq return_char          ; read eof, no echo, no char to process

        ; We have the newest read character in A (& TMPA). What do we do with it?
        ; We'll start with just considering carriage return and backspace.
        cmp #8  ; backspace
        bne check_for_cr
        ; we need to handle a backspace character. If there are any characters in 
        ; current line, we can roll back wrptr and store a 0.
        COUNTCL ttybuffer
        beq echo_bell         ; No characters to delete, so we just ignore this.
        lda wr_ttybuffer
        dec
        and #%01111111
        sta wr_ttybuffer
        bra echo

echo_bell:
        lda #7
        sta TTY + TTY_BLOCK::TMPA
        bra echo

check_for_cr:
        cmp #13 ; carriage return
        bne check_for_esc
        ; We need to handle a carriage return. This makes the current line available,
        ; which is to say we put the character in the buffer and make the new write
        ; pos cl_ttybuffer
        WRITEBUFFER ttybuffer
        lda wr_ttybuffer
        sta cl_ttybuffer
        bra echo

check_for_esc:
        cmp #27 ;esc
        bne handle_regular_char
        ; For now we're going to handle an escape char from the terminal by swapping
        ; it to a '+'
        lda #'+'
        sta TTY + TTY_BLOCK::TMPA
        ; and for this one we're just gonna follow thru to the writebuffer. shrug.

handle_regular_char:
        WRITEBUFFER ttybuffer
echo:
        ; Let's see what we need to echo
        lda TTY + TTY_BLOCK::ECHO
        beq return_char      ; echo off, no echo
        ; echo is on, so write to out
        lda TTY + TTY_BLOCK::OUT_DEV
        sta CURRENT_DEVICE
        lda TTY + TTY_BLOCK::OUT_CHAN
        sta DEVICE_CHANNEL
        lda TTY + TTY_BLOCK::OUT_OFFSET
        sta DEVICE_OFFSET
        lda TTY + TTY_BLOCK::TMPA
        ldx TTY + TTY_BLOCK::TMPX
        jsr dev_putc
        cmp #13;'\r'            ; Echo a newline after a carriage return.
        bne test_for_backspace
        lda #10;'\n'
        jsr dev_putc
        bra return_char
test_for_backspace:
        cmp #8
        bne return_char
        ;lda #127 ;DEL   ; followup backspace with a delete.
        ;jsr dev_putc
        ; that didn't work. let's try a space and another backspace
        lda #' '
        jsr dev_putc
        lda #8
        jsr dev_putc

return_char:
        ; is there an available character to return?
        ; make tty the active device again
        lda TTY + TTY_BLOCK::SELF_DEV
        sta CURRENT_DEVICE
        lda TTY + TTY_BLOCK::SELF_CHAN
        sta DEVICE_CHANNEL
        lda TTY + TTY_BLOCK::SELF_OFFSET
        sta DEVICE_OFFSET
        ;lda TTY + TTY_BLOCK::TMPA
        ;ldx TTY + TTY_BLOCK::TMPX

        ; Is there an available character to return?
        COUNTAVAIL ttybuffer
        bne char_available
        clc
        rts     ; no character available, just return with carry clear
char_available:
        READBUFFER ttybuffer
        sec
        rts


;return_w_carry:
;        sec                     ; We read a character, so we need to return with carry set
;return:
;        ; make tty the active device again
;        lda TTY + TTY_BLOCK::SELF_DEV
;        sta CURRENT_DEVICE
;        lda TTY + TTY_BLOCK::SELF_CHAN
;        sta DEVICE_CHANNEL
;        lda TTY + TTY_BLOCK::SELF_OFFSET
;        sta DEVICE_OFFSET
;        lda TTY + TTY_BLOCK::TMPA
;        ldx TTY + TTY_BLOCK::TMPX

;        rts
.endproc



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
