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


.proc TTY_INIT
        stz TTY + TTY_BLOCK::MODE
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
        rts
.endproc

.proc TTY_OPEN
        rts
.endproc

.proc TTY_CLOSE
        rts
.endproc

.proc TTY_GETC
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
