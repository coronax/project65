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

.export _print_hex, _print_string, hexits
.import dev_putc, _print_char

; send_char => _print_char, _outputstring => _print_string, print_hex => _print_hex

; Prints a zero-terminated string of up to 255 characters to the console
; Uses AXY, ptr1		
.proc _print_string
        sta ptr1
        stx ptr1h
        ldy #0
outloop: lda (ptr1),y
        beq doneoutputstring
        jsr _print_char
        iny
        bne outloop ; bne so we don't loop forever on long strings
doneoutputstring:
        rts
.endproc



; Prints the value in A to the console as a 2-character hex representation.
; Does not preserve A!
; Uses AX
.proc _print_hex
		pha
		ror
		ror
		ror
		ror
		and #$0F
		tax
		lda hexits,x
		jsr _print_char
		pla
		and #$0F
		tax
		lda hexits,x
		jmp _print_char ; cheap rts
.endproc



.rodata

hexits:	.byte "0123456789ABCDEF"

