
.include "os3.inc"
.export TokenizeCommandLine, test_tokenizer
.import _outputstring, _commandline, crlf
.import print_printable, print_hex, sendchar


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
        lda #>buffer
        sta ptr1h           ; We're taking advantage of the fact that buffer
        sta argv0H          ; fits in a single page, so the high byte of any
        sta argv1H          ; argv is the same as the high byte of buffer's
        sta argv2H          ; address.
        sta argv3H
        lda #<argv0L
        sta ptr2
        lda #>argv0L
        sta ptr2h
        ldy #0
        stz argc            ; argc = 0

        ; skip whitespace
skipwhitespace:
        lda buffer,y
        beq done_ws         ; found end of string
        cmp #' '
        beq continue_ws
        cmp #$9             ; tab
        beq continue_ws
        bra done_ws
continue_ws:
        iny
        bra skipwhitespace
done_ws:

        lda buffer,y
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
scantoken:
        lda buffer,y
        beq done_tokenize   ; if we hit end-of-string, we're just done
        cmp #' '
        beq endoftoken
        cmp #$9             ; tab
        beq endoftoken
        iny
        bra scantoken
endoftoken:
        lda #0              
        sta (ptr1),y        ; write an end of string to end of token
        iny

        ; if we have space in the token table, loop
        lda argc
        cmp #4
        bne skipwhitespace


done_tokenize:
        rts
.endproc

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
        jsr _outputstring

        ldx #>crlf
        lda #<crlf
        jsr _outputstring

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


; Put the start address in ptr2
.proc PrintMemory
		; print 8 rows of 8 characters of output
		; this is broken because print_hex uses x.
		ldx #8
@loop:  
		phx
        lda #'m'
        jsr sendchar
        lda #'.'
        jsr sendchar

        lda ptr2h
        jsr print_hex
        lda ptr2
        jsr print_hex
        lda #' '
        jsr sendchar
        jsr sendchar

        ldy #0
        lda (ptr2),y
        jsr print_hex
        lda #' '
        jsr sendchar
        iny
        lda (ptr2),y
        jsr print_hex
        lda #' '
        jsr sendchar
        iny
        lda (ptr2),y
        jsr print_hex
        lda #' '
        jsr sendchar
        iny
        lda (ptr2),y
        jsr print_hex
        iny
		
        lda #' '
        jsr sendchar

        lda (ptr2),y
        jsr print_hex
        lda #' '
        jsr sendchar
        iny
        lda (ptr2),y
        jsr print_hex
        lda #' '
        jsr sendchar
        iny
        lda (ptr2),y
        jsr print_hex
        lda #' '
        jsr sendchar
        iny
        lda (ptr2),y
        jsr print_hex
		
		lda #' '
		jsr sendchar
		
		ldy #0
@loop2:	lda (ptr2),y
		jsr print_printable
		iny
		cpy #8
		bne @loop2
		
        ;printstring crlf
		lda #CR
		jsr sendchar
		lda #LF
		jsr sendchar

		clc					; increment loop
		lda ptr2
		adc #8
		sta ptr2
		lda ptr2h
		adc #0
		sta ptr2h
		plx
		dex
		beq @done
		jmp @loop
@done:	rts	; return to command line parser
.endproc
