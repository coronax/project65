
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

.include "os3.inc"
.export TokenizeCommandLine
.import _print_string, _commandline, crlf
.import print_printable, _print_hex, _print_char


; Tokenize the command line. The command line can be found in buffer ($0240).
; We need to break it up and fill in argc and argv0-3.
; Uses: A, X, Y, ptr1, ptr1h, ptr2, ptr2h  in 0 page
; argc & argv in page 2
.proc TokenizeCommandLine
        lda #<buffer
        sta ptr1
        sta argv0L          ; we'll add our index to these lsb's when we
        sta argv1L          ; get to them.
        sta argv2L
        sta argv3L
        sta argv4L
        lda #>buffer
        sta ptr1h           ; We're taking advantage of the fact that buffer
        sta argv0H          ; fits in a single page, so the high byte of any
        sta argv1H          ; argv is the same as the high byte of buffer's
        sta argv2H          ; address.
        sta argv3H
        sta argv4H
        lda #<argv0L
        sta ptr2
        lda #>argv0L
        sta ptr2h
        ldy #0
        stz argc            ; argc = 0

        ; skip whitespace
skipwhitespace:
        lda buffer,y
        beq done_tokenize    ; found end of string
        cmp #' '
        beq continue_ws
        cmp #TAB             ; tab
        beq continue_ws
        bra done_ws
continue_ws:
        iny
        bra skipwhitespace
done_ws:

        lda buffer,y
        stz arg_is_quoted
        cmp #QUOTE          ; check for opening quote
        bne not_quoted
        sta arg_is_quoted   ; set arg_is_quoted nonzero
        iny                 ; and skip ahead to next char
        lda buffer,y
not_quoted:

        beq done_tokenize   ; we're at the end of the buffer

        ; OK. we're at the start of a token. we need to save the value of
        ; buffer + y to one of the argv variables. ptr2 points to 
        ; that value...
        clc
        tya
        tax                 ; save y val for later
        adc #<buffer        ; y + lsb of buffer address
        ldy #0
        sta (ptr2),y        ; save the pointer to argv*L
        inc argc            ; and increment argc

        txa
        tay                 ; recover y index

        inc ptr2
        inc ptr2            ; point ptr2 at the next argv L val

        ; Now we need to scan until we find whitespace or eol
        ; non-quote version
        lda arg_is_quoted  
        bne scantokenforquote

scantoken:
        lda buffer,y
        beq done_tokenize   ; if we hit end-of-string, we're just done
        cmp #' '
        beq endoftoken
        cmp #$9             ; tab
        beq endoftoken
        iny
        bra scantoken

scantokenforquote:
        lda buffer,y
        beq done_tokenize   ; if we hit end-of-string, we're just done
        cmp #QUOTE
        beq endoftoken
        iny
        bra scantokenforquote

endoftoken:
        lda #0              
        sta (ptr1),y        ; write an end of string to end of token
        iny

        ; if we have space in the token table, loop
        lda argc
        cmp #4
        bne skipwhitespace

done_tokenize:
        ; Need to zero-out the unused elements
        lda ptr2
        cmp #<argv_end
        beq done
        lda #0
        tay
        sta (ptr2),y
        iny
        sta (ptr2),y
        inc ptr2
        inc ptr2
        bra done_tokenize

done:
        rts
.endproc



.if 0
testmsg1: .byte "Finished parsing command line", CR, LF, 0
testmsg2: .byte "Finished printing args", CR, LF, 0

.proc test_tokenizer
        jsr TokenizeCommandLine
        lda #<buffer        ; print memory dump starting at buffer so we can 
        sta ptr2            ; see if we've put nul characters at right places.
        lda #>buffer
        sta ptr2h
        jsr PrintMemory
        printstring testmsg1
        lda #<argv0L
        sta ptr2
        lda #>argv0L
        sta ptr2h
        ldx argc
        phx                 ; save x on stack
print_token:

;        lda #<ptr1
;        sta ptr2
;        lda #>ptr1
;        sta ptr2h
;        jsr PrintMemory

        ldy #1
        lda (ptr2),y
        tax
        dey
        lda (ptr2),y
        jsr _print_string

        ldx #>crlf
        lda #<crlf
        jsr _print_string

        plx
        dex
        beq done
        phx
        inc ptr2
        inc ptr2            ; point at next argv

        bra print_token
done:
        printstring testmsg2
        jmp _commandline
.endproc
.endif



; Put the start address in ptr2
.proc PrintMemory
	; print 8 rows of 8 characters of output
	ldx #8
loop:  
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

        ; Print the hexadecimal representation of memory
        ldy #0
loop2:  lda (ptr2),y
        jsr _print_hex
        lda #' '
        jsr _print_char
        iny
        cpy #8
        bne loop2

        ; Print the ascii representation of the memory
	ldy #0
loop3:	lda (ptr2),y
	jsr print_printable
	iny
	cpy #8
	bne loop3
		
	lda #CR         ; end of line
	jsr _print_char
	lda #LF
	jsr _print_char

	clc	        ; increment memory pointer
	lda ptr2
	adc #8
	sta ptr2
	lda ptr2h
	adc #0
	sta ptr2h

	plx             ; decrement row count & loop
	dex
        bne loop

@done:	rts	; return to command line parser
.endproc
