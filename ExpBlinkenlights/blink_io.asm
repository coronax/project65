;; ExpBlinkenlights
;; Copyright (c) 2024 Christopher Just
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
	
.export _getch, _sleep

	;;  I increasingly don't need the IO here, but i do need a sleep fn.
	;; And I need to cycle count for that.

.code
	
; zero-page scratch space - copied from OS
ptr1	:= $fb
ptr1h	:= $fc
ptr2	:= $fd
ptr2h	:= $fe


	;; 	char __fastcall__ getch (void)
.proc _getch
	clc
    jsr $FFE9
	bcs done
	lda #0						; Return 0 if no char available
done:
	rts
.endproc


	;; void __fastcall__ sleep ()
.proc _sleep
	;; expect low byte of time in ms in A, high byte in x.
	sta ptr1
	stx ptr1h
	sec
	;; I should have 4000 clocks per ms. but it doesn't account for
	;; interrupts at all. Should use timer interrupts for this.
	;; or timeofday clock or something.
l5:	
	;; This loop should take 1276 clocks
	ldx #$ff
l1:	dex
	bne	l1
	ldx #$ff
l2:	dex
	bne	l2
	ldx #$ff
l3:	dex
	bne	l3
	;; 172 clocks left
	ldx #$22	
l4:	dex
	bne	l4

	lda ptr1
	sbc #1
	sta ptr1
	bcs l5
	lda ptr1h
	sbc #1
	sta ptr1h
	bcs l5
	rts

	
	dec ptr1					;2nd time thru this should wrap ptr1 back to ff.
	bne l5
	dec ptr1h
	bne	l5
	rts
.endproc


