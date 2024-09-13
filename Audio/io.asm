

.export _msleep

	;;  I increasingly don't need the IO here, but i do need a sleep fn.
	;; And I need to cycle count for that.

	
;CR		=        $0d                ; carriage return
;LF      =        $0a                ; line feed
;ESC     =        $1b                ; ESC to exit

;buffer=$1000
;chartosend=$207         ; store send char for _sendchar
;readbuffer=$209         ; temp read buffer for rwbit
;writebuffer=$20a        ; temp write buffer for rwbit
;statusbyte=$20b         ; store status byte for readchar
;program_address_low=$20c
;program_address_high=$20d


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




;VIA_ADDRESS 	= $C000
;VIA_DDRA		= VIA_ADDRESS + $3
;VIA_DDRB		= VIA_ADDRESS + $2
;VIA_DATAA		= VIA_ADDRESS + $1
;VIA_DATAB		= VIA_ADDRESS + $0
;VIA_ACR			= VIA_ADDRESS + $B          ; auxiliary control register
;VIA_SHIFT		= VIA_ADDRESS + $A

		
