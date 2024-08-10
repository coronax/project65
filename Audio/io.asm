
.export init_io, _outputstring, _sendchar, readchar, _print_hex , _getch, _sleep

	;;  I increasingly don't need the IO here, but i do need a sleep fn.
	;; And I need to cycle count for that.

	
CR		=        $0d                ; carriage return
LF      =        $0a                ; line feed
ESC     =        $1b                ; ESC to exit

;buffer=$1000
chartosend=$207         ; store send char for _sendchar
readbuffer=$209         ; temp read buffer for rwbit
writebuffer=$20a        ; temp write buffer for rwbit
statusbyte=$20b         ; store status byte for readchar
program_address_low=$20c
program_address_high=$20d
; monitor variables
;breakpoint_low=$240
;breakpoint_high=$241
;breakpoint_value=$242

; zero-page scratch space
ptr1	:= $fb
ptr1h	:= $fc
ptr2	:= $fd
ptr2h	:= $fe


_sleep:
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


;_outstring:
;	.byte <_outputstring, >_outputstring

.macro printstring string
        lda #<string
        ldx #>string
        jsr _outputstring
.endmacro


VIA_ADDRESS 	= $C000
VIA_DDRA		= VIA_ADDRESS + $3
VIA_DDRB		= VIA_ADDRESS + $2
VIA_DATAA		= VIA_ADDRESS + $1
VIA_DATAB		= VIA_ADDRESS + $0
VIA_ACR			= VIA_ADDRESS + $B          ; auxiliary control register
VIA_SHIFT		= VIA_ADDRESS + $A


.code
;.byte "io.asm"

; The IO code uses Port B for bitbanged SPI I/O.
;   PB0 = SPI Clock
;   PB1 = chip select for Max3100
;   PB2 = MOSI
;   PB3 = MISO (old)
;   PB4 = SD card chip select
;	PB5 = interrupt from Max3100 (experimental)
;   PB7 = MISO (new)

init_io:
		; disable interrupts - insitu is all-polling for now
		sei
		
        ; set up parallel port
        lda #$77		; reading on pins 3 (old miso) and 7 (new miso)
        sta VIA_DDRB
		lda #$00
		sta VIA_DDRA
        lda #$18		; ACR - disable latching & setup shift register
        sta VIA_ACR		; shift register shift out with phi2 clock
        ldx #0

        ; a restart signal for the logic analyzer
        lda #$10
        sta VIA_DATAB
        nop
        nop
        nop
        lda #$02		; max3100 ss high
        sta VIA_DATAB

        jsr initmax3100
		rts



; initialize max3100 via SPI
initmax3100:
			; toggle slave select - and include a full toggle just
			; in case the slave select wasn't set correctly to
			; begin with.
			lda 	VIA_DATAB
			ora		#2
			sta		VIA_DATAB
			eor 	#2
			sta 	VIA_DATAB

			; send first 8 bits
			lda 	#%11000000
			jsr		spibyte

			; send second 8 bits
			lda 	#%00001010 ; 9600 baud
			jsr 	spibyte

			lda		VIA_DATAB
			ora 	#2
			sta 	VIA_DATAB	; turn off slave select			

			rts


; Sends the character in accumulator out the spi port.
; If the max3100 reports that the send buffer is full,
; we retry until we're successful.
_sendchar:
			sta 	chartosend
retrysend:
			; set slave select
			lda 	VIA_DATAB
			eor		#2
			sta 	VIA_DATAB

			; send write command
			lda 	#$82	; enable RTS always
			jsr 	spibyte

			; save the status byte we just read
			sta 	statusbyte

			; send 8 bits of char data
			lda 	chartosend
			jsr 	spibyte
			
			lda		VIA_DATAB
			ora 	#2
			sta 	VIA_DATAB	; turn off slave select			

			; if bit 6 of the first byte we read (command)
			; is 0, the buffer was full and the send didn't
			; go thru.
			lda 	statusbyte
			and 	#$40
			beq 	retrysend

			; for ehbasic, we need to get the original char
			; back in A.  Conveniently, we've rotated all the
			; way around.
			lda 	chartosend
			rts


; Blocking read character
_getch:
			clc
get:     	jsr readchar
			bcc get
			rts


; read character
readchar:
			; toggle slave select
			lda 	VIA_DATAB
			eor		#2
			sta 	VIA_DATAB

			; send 8 bit read command; read status
			; to send a bit, we rol to send the
			; high bit of command to the carry flag,
			; and then call sendcarrybit
			lda 	#$00
			jsr 	spibyte
			sta 	statusbyte

			; send $00, read 8 bits of char data
			lda 	#0
			jsr 	spibyte

			lda		VIA_DATAB
			ora 	#2
			sta 	VIA_DATAB	; turn off slave select			

			; high bit of statusbyte is 1 iff we read
			; a real character. rol to put that bit in
			; carry, and load whatever we read into A
			rol 	statusbyte
			lda 	readbuffer  
			rts

spibyte:
			sta 	writebuffer
			jsr 	rwbit
			jsr 	rwbit
			jsr 	rwbit
			jsr 	rwbit
			jsr 	rwbit
			jsr 	rwbit
			jsr 	rwbit
			jsr 	rwbit
			lda 	readbuffer
			rts


; rwbit writes one bit of writebuffer and reads
; one bit into readbuffer.  Both buffers are
; left shifted by one place.
rwbit:
			rol 	writebuffer
			bcs 	nonzero
			lda		VIA_DATAB
			and		#%11111011		; set MOSI low
nzret:   	sta 	VIA_DATAB    ; set output bit
			inc 	VIA_DATAB    ; set clock high    
			; receive bit
			asl 	readbuffer
			lda 	VIA_DATAB    ; read input bit
			and 	#$80
			beq 	reczero
			inc 	readbuffer   ; add a 1 bit 
reczero:
			dec 	VIA_DATAB    ; set clock low
			rts
nonzero: 
			lda 	VIA_DATAB
			ora		#4				; set MOSI high
			jmp 	nzret

			

; Prints a zero-terminated string of up to 255 characters	
; Uses A, Y, and ptr1		
_outputstring:
        sta ptr1
        stx ptr1h
        ldy #0
outloop: lda (ptr1),y
        beq doneoutputstring
        jsr _sendchar
        iny
        bne outloop ; bne so we don't loop forever on long strings
doneoutputstring:
        rts

		
		
; prints the 2-character hex representation
; of the value in A
_print_hex:
			pha
		   ; sta temp
			ror
			ror
			ror
			ror
			and #$0F
			tax
			lda hexits,x
			jsr _sendchar
			pla
		   ; lda temp
			and #$0F
			tax
			lda hexits,x
			jmp _sendchar ; cheap rts

hexits:
			.byte "0123456789ABCDEF"
			

		
