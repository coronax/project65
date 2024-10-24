

.export _msleep, _EnableInterrupt, _RemoveInterrupt
.import _SongInterrupt, _int_sp, _next_note_at
.import _s, _current_span, _next_span_at, _current_note, _t, _done, _songlen, _range_end
.importzp sp



MULTIPLEX_DELAY = 3  ; # of interrupts between multiplexing notes.
r = $a000
iptr1 = $34 ; a zero page spot allocated for use by interrupt routines.

.proc _AsmSongInterrupt
	lda _done
	beq continue
	rts
continue:

	; increment t
	inc _t
	bne nocarry
	inc _t+1
nocarry:

	; is t >= next_span_at  ??
	lda _t+1
	cmp _next_span_at+1
	beq test_low_byte
	bpl start_new_span
test_low_byte:
	lda _t
	cmp _next_span_at
	bpl start_new_span
	beq start_new_span ; redundant?
	bra done_span

start_new_span:
	; increment current span
	inc _current_span
	bne nocarry2
	inc _current_span+1
nocarry2:

	; is current_span == songlen? if so, done
	lda _current_span
	cmp _range_end
	bne next_span
	lda _current_span+1
	cmp _range_end+1
	bne next_span
	; Here we're finished. end audio & set done
	lda #$FF
	sta _done
	sta r
	rts
next_span:
	; here there is a next span to start.
	; first, increment the pointer s by size of span & save s to iptr1
	clc
	lda _s
	adc #6 ; sizeof struct Span
	sta _s
	sta iptr1
	lda _s+1
	adc #0 ; add carry
	sta _s+1
	sta iptr1+1

	stz _t	; zero t
	stz _t+1
	stz _current_note ; zero current_note
	;lda _s
	;sta iptr1
	;lda _s+1
	;sta iptr1+1

	; _next_span_at = s.Time
	ldy #1
	lda (iptr1),y
	;lsr		; we're gonna double speed for now
	sta _next_span_at+1
	ldy #0
	lda (iptr1),y
	;lsr
	sta _next_span_at

	; r = s->Notes[current_note]
	ldy _current_note
	iny
	iny
	lda (iptr1),y ; current note value
	sta r        ; write to audio card
	; _next_note_at = MULTIPLEX_DELAY
	lda #MULTIPLEX_DELAY
	sta _next_note_at
	stz _next_note_at+1

done_span:

	; Next we would check if t == next_note_at so we can do multiplexing
	lda _t+1
	cmp _next_note_at+1
	bne done
	lda _t
	cmp _next_note_at
	bne done
	; ok, probably time to multiplex a note
	; first update next note time
	clc
	lda _next_note_at
	adc #MULTIPLEX_DELAY
	sta _next_note_at
	lda _next_note_at+1
	adc #0
	sta _next_note_at+1
	; Now increment current note
	lda _current_note
	inc
	and #3 ; rollover if >3
	sta _current_note
	; update if current note is 0 or the note value is nonzero
	tay ; current note in y and x, cuz we'll need it later
	iny
	iny ; add 2 to y so we'll point at the current note value
	tax
	lda _s
	sta iptr1
	lda _s+1
	sta iptr1+1
	lda (iptr1),y ; retrieve curent note value
	bne do_update_note  ; update if note value is not zero
	cpx #0
	beq do_update_note  ; also update if note index _is_ zero
	bra done
do_update_note:
	sta r




done:
	rts
.endproc



stored_interrupt:	.word $0000
stored_sp: .word $0000
.proc interrupt
		pha
		phx 
		phy
;		lda sp
;		sta stored_sp
;		lda sp+1
;		sta stored_sp+1
;		lda _int_sp
;		sta sp
;		lda _int_sp+1
;		sta sp

		jsr _AsmSongInterrupt		; call out song function

;		lda stored_sp
;		sta sp
;		lda stored_sp+1
;		sta sp+1
		ply		; Take these values off the stack so that the next handler in
		plx		; the chain can put them back on the stack so the last handler
		pla		; can pull them before doing rti.

		jmp (stored_interrupt)	; jump to regular interrupt
.endproc


.proc _EnableInterrupt
		sei		; disable interrupts
		lda $0204
		sta stored_interrupt
		lda $0205
		sta stored_interrupt+1
		lda #<interrupt
		sta $0204
		lda #>interrupt
		sta $0205
		cli
		rts
.endproc


.proc _RemoveInterrupt
		sei		; disable interrupts
		lda stored_interrupt
		sta $0204
		lda stored_interrupt+1
		sta $0205
		cli
		rts
.endproc



	;;  I increasingly don't need the IO here, but i do need a sleep fn.
	;; And I need to cycle count for that.



; zero-page scratch space
;ptr1	:= $fb
;ptr1h	:= $fc
;ptr2	:= $fd
;ptr2h	:= $fe
ptr1: .byte 00
ptr1h: .byte 00

_msleep:
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

