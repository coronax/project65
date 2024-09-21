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
.export _print_char, _read_char ; shortcuts to print/read direct from the terminal.
.export SERIAL_IOCTL, SERIAL_GETC, SERIAL_PUTC, Max3100_IRQ, Max3100_TimerIRQ
.import _print_string

; Max3100 Driver 2.0
; This version of the Max3100 driver is interrupt-based and uses
; better-optimized SPI routines. Note that this means that all
; SPI commands have to be performed with interrupts disabled,
; which isn't terrific.
;
; The Max3100 driver has partial support for RTS/CTS: It sets
; its RTS flag when the receive buffer reaches a high-water
; mark and releases it when the recieve buffer falls below
; a low-water mark.  On the other hand, the CTS signal from
; the other side of the connection is ignored. This setup
; is designed to favor terminals like TeraTerm, which
; unfortunately will keep CTS high forever if hardware
; handshaking is enabled.  Apparently, the issue is that
; TeraTerm is expecting a complete modem handshake with DTR
; and whatnot, but in the meantime it will still at least
; recognize what it sees as CTS (our RTS). This is probably
; good enough for our needs, since it keeps the P:65 from
; being overwhelmed, and the PC on the other end of the
; serial port can probably handle anything we throw at it.

; The IO code uses Port B for bitbanged SPI I/O.
;   PB0 = SPI Clock
;   PB1 = chip select for Max3100
;   PB2 = MOSI
;   PB3 = MISO - old
;   PB4 = SD card chip select
;	PB5 = interrupt from Max3100 (experimental) - NOT IMPLEMENTED
;	PB7 = MISO - new 
		
.code
;.byte "io.asm"




; CJ BUG these routines below only work for 128 byte buffers.
; do I really want to require 256 byte buffers? I mean that would
; have applications starting at like $0500 or something.

.define wrptr(buffername) .ident(.concat("wr_", .string(buffername)))
.define rdptr(buffername) .ident(.concat("rd_", .string(buffername)))

; Initialize buffer to empty
.macro INITBUFFER buffername
		STZ wrptr buffername
		STZ rdptr buffername
.endmacro
		
; Write A to the write buffer
; Note that this doesn't test if there's enough room.
; Modifies X
.if 0
.macro WRITEBUFFER buffername
		LDX wrptr buffername
		STA buffername, X
		INC wrptr buffername
		RMB7 wrptr buffername
.endmacro
.endif

.if 1
; version without rmb7 - larger but faster
.macro WRITEBUFFER buffername
		LDX wrptr buffername
		STA buffername, X
		
		txa
		inc
		and #$7F
		sta wrptr buffername
.endmacro
.endif

; Reads from the read buffer to A.
; doesn't check if there's available data.
; Modifies AX
.if 0
.macro READBUFFER buffername
		LDX rdptr buffername
		LDA buffername, X
		INC rdptr buffername
		RMB7 rdptr buffername
.endmacro
.endif

.if 1
; Reads from the read buffer to A.
; doesn't check if there's available data.
; Modifies AX
.macro READBUFFER buffername
		LDX rdptr buffername
		LDA buffername, X
		inx
		cpx #$80
		bne l1
		ldx #0
l1:		stx rdptr buffername
.endmacro
.endif

; Returns count of items in buffer in A
; Uses A
.macro COUNTBUFFER buffername
		LDA wrptr buffername
		SEC
		SBC rdptr buffername
		AND #%01111111
.endmacro

; These names are used in places where I want to send data out the
; serial port without using the device interface.
_read_char = SERIAL_GETC	
_print_char = SERIAL_PUTC



;=============================================================================
; SERIAL_PUTC
; PUTC Implementation for serial/Max3100 driver.
; Places the character in A into the send buffer. Busy waits if that buffer 
; is full.
; Modifies: None. The character written is returned in A in order to 
; satisfy EhBasic.
;=============================================================================
.proc SERIAL_PUTC
		pha
		phx
		pha			; store A AGAIN because COUNTBUFFER will overwrite
wait:
		sei
		COUNTBUFFER wbuffer
		cli			; re-enable IRQ in case we need to wait
		cmp #$30	; high water mark for write buffer.
		bpl wait
		pla			; Pull 1st A off
		sei
		WRITEBUFFER wbuffer
		; Call SendRecv here, otherwise our timer interrupt frequency limits
		; us to sending 100 bytes per second (unless we're also receving read
		; interrupts).
		jsr Max3100_SendRecv
		plx			; Pull AX off stack.
		pla			; Always return the character we wrote in AX.
		cli

		rts
.endproc



;=============================================================================
; SERIAL_GETC
; GETC Implementation for serial/Max3100 driver.
; If a character is available in the receive buffer, return it & set carry.
; If no character is available, clear carry.
; This function may do a SendRecv action if it is time to re-enable RTS.
; Modifies: A
;=============================================================================
.proc SERIAL_GETC
		phx
		sei	; stop interrupts for a moment
		COUNTBUFFER rbuffer
		beq empty

		; Check space in rbuffer. If count is equal to the low
		; water mark and rts is disabled (rts flag is 00) then
		; we need to set it to enabled.
		cmp #$10
		bpl read_char
		lda max3100_rts_flag
		bne read_char	; if already enabled, nevermind.

		; We need to send a command to enable rts & allow sends.
		; Can we just set the flag and do the IO during timer IRQ?
		; No, because timer IRQ only fires SendRecv if there's a
		; char in the output buffer.
		lda #SERIAL_RTS_ENABLE
		sta max3100_rts_flag
		jsr Max3100_SendRecv

		; and then we can go ahead and read the character
read_char:		
		READBUFFER rbuffer
		cli		; re-enable interrupts
		plx
;		cmp #0  ; need to fix up the status register flags N & Z
;		        ; to reflect the byte we're returning.
		sec		; set carry to indicate a character was read
		rts

empty:
		plx
		clc		; carry clear means no character to be read
		cli		; re-enable interrupts
		rts
		
.endproc



;=============================================================================
; Max3100_IRQ
; Interrupt Service Routine for Max3100 UART
;=============================================================================
Max3100_IRQ = Max3100_SendRecv
.if 0
.proc Max3100_IRQ
		; This interrupt came from the MAX3100 UART.
		; It indicates that there is data to be read.

		jmp Max3100_SendRecv
		;bcc done		; if we read a character, check space in buffer
		;jmp checkrts	; and RTS flag
done:
		;rts
.endproc
.endif


;=============================================================================
; Max3100_TimerIRQ
; Utility routine to be called during timer interrupts.
; Restarts sends in case we missed a transmit buffer empty
; interrupt.
;=============================================================================
.proc Max3100_TimerIRQ
		; also in the timer interrupt, check the write buffer
		; Eventually this should be removed and replaced by
		; smart handling of the 3100's transmit interrupt.
		COUNTBUFFER wbuffer
		beq done
		jmp Max3100_SendRecv
done:
		rts
.endproc



;=============================================================================
; SERIAL_IOCTL
; IO Control for serial driver
;=============================================================================
.proc SERIAL_IOCTL
		cmp #0
		beq SERIAL_INIT
		cmp #1
		beq io_flush
error:
		; Unidentified command value. return EINVAL.
		lda #P65_EINVAL
		ldx #$FF
		rts
io_flush:
		COUNTBUFFER wbuffer
		bne io_flush
		rts
.endproc



;=============================================================================
; SERIAL_INIT
; Device initialization for Max3100 UART
;=============================================================================
.proc SERIAL_INIT
		; initialize read & write buffers
		INITBUFFER rbuffer
		INITBUFFER wbuffer
		
        ; set up parallel port
        lda #$77		; reading on pins 3 (old miso) and 7 (new miso)
        sta VIA_DDRB
		;lda #$00
		;sta VIA_DDRA
        ;lda #$18		; ACR - disable latching & setup shift register
        ;sta VIA_ACR		; shift register shift out with phi2 clock
        ;ldx #0

        ; a restart signal for the logic analyzer
        lda #$10
        sta VIA_DATAB
        nop
        nop
        nop
        lda #$02		; max3100 ss high
        sta VIA_DATAB
		; was the above setting ss high part of my trouble? doesn't make sense...

        jsr Max3100_Init
		lda #P65_EOK
		tax
		rts
.endproc



;=============================================================================
; Max3100_Init
; Initialize Max3100 via SPI
;=============================================================================
.proc Max3100_Init
		; toggle device select - and include a full toggle just
		; in case the device select wasn't set correctly to
		; begin with.
		lda 	VIA_DATAB
		ora		#2
		sta		VIA_DATAB
		eor 	#2
		sta 	VIA_DATAB

		; send first 8 bits
		lda 	#%11001100	; write config; enable read buffer irq
		jsr		spibyte

		; send second 8 bits
		lda 	#SERIAL_BAUD_9600
		jsr 	spibyte

		lda #%011111010		; turn off device select
		sta VIA_DATAB

		; enable RTS (allow the other side to send).
		lda #SERIAL_RTS_ENABLE
		sta max3100_rts_flag
		jsr Max3100_SendRecv
		
		rts
.endproc

		

.proc Max3100_CheckConfig
		rts
		; toggle slave select - and include a full toggle just
		; in case the slave select wasn't set correctly to
		; begin with.
		lda 	VIA_DATAB
		ora		#2
		sta		VIA_DATAB
		eor 	#2
		sta 	VIA_DATAB

		; send first 8 bits
		lda 	#%01000000
		jsr		spibyte
		sta statusbyte

		; send second 8 bits
		lda 	#%00000000 ; 9600 baud
		jsr 	spibyte

		lda #%011111010		; turn off slave select
		sta VIA_DATAB

		lda statusbyte
		and #4	; test RM bit
		beq rm_not_set
		printstring rm_set_message
		bra done
rm_not_set:		
		printstring rm_notset_message		
done:
		rts
rm_set_message:	
.byte "RM has been set (good)!", 0
rm_notset_message:
.byte "RM has NOT been set (bad)!", 0

.endproc



;=============================================================================
; Max3100_SendRecv
;=============================================================================
; This function:
;    a) tries to transmit a byte if the send buffer is nonempty
;       and the 3100 xmit queue is empty
;    b) otherwise sends with TE high (transmit buffer disabled)
;    b) tries to read a byte.
; Sends the character in accumulator out the spi port.
; ONLY call with interrupts disabled!
;=============================================================================
.proc Max3100_SendRecv
		pha
		phx			; save x,y because spibyte uses them
		phy

		COUNTBUFFER wbuffer	; any characters waiting to be sent?
		beq empty_wbuffer
		lda #$40			; matches T flag in status byte
		sta max3100_sending ; remember that we want to send a byte
		lda #%10000000		; write command with /TE enabled
		bra check_rts
empty_wbuffer:
		stz max3100_sending
		lda #%10000100		; write command with /TE disabled
check_rts:

		ora max3100_rts_flag

		; send write command
		jsr spibyte

		; save the status byte we just read
		sta statusbyte

		; If the T (transmit buffer empty) flag of statusbyte is set and
		; max3100_sending is set, we need to grab a byte from the write
		; buffer to send.
		;lda #$cc	; dummy value for debugging
		and max3100_sending ; $00 or $40, AND with statusbyte
		beq send_data

		READBUFFER wbuffer	; a real value to be transmitted.

send_data:		
		; send & recv 8 bits of char data, real or dummy
		jsr spibyte
		tay					; stash received byte for later
			
		lda #%01111010		; turn off Max3100 chip select
		sta VIA_DATAB

		; if bit 7 of statusbyte is 1, then we read a
		; byte and need to put it into the fifo
		bit statusbyte		; test if we read a byte
		bpl done
		; Our rts handling should have guaranteed that there's room in the
		; read buffer, but if not there isn't really anything we can do 
		; about it...
		tya 				; grab the stashed received byte
		WRITEBUFFER rbuffer ; and add it to readbuffer

		; Having written to the buffer, we need to check if we hit the high-
		; water mark:
		COUNTBUFFER rbuffer
		cmp #$20				; compare to high-water mark
		bmi done
		; to disable /RTS, set rts_flag to zero. Takes affect during next
		; SendRecv.
		stz max3100_rts_flag

done:
		ply					; restore x, y
		plx
		pla
		rts
.endproc



;=============================================================================
; spibyte
;=============================================================================
; Sends the byte in A to the SPI port. Simultaneously reads a byte, which is
; returned in A (and in spi_readbuffer).
; ONLY call with interrupts disabled!
;=============================================================================
.proc spibyte
		sta spi_writebuffer
.repeat 8
.scope
		lda #%01111000		; base value with chip select
		rol spi_writebuffer
		bcc write_zero_bit
		ora #%00000100		; write a one bit to the output.
write_zero_bit:
		
;		lda #0
;		rol spi_writebuffer
;		rol
;		rol
;		rol
;		ora #%01111000 		; or with slave select bits. (active low!)
							; CJ BUG slave select hard coded. Should be
							; a parameter.

 		sta VIA_DATAB    	; set output bit
		inc VIA_DATAB    	; set clock high    

		lda	VIA_DATAB		; Read bit
		rol					; Shift MISO to carry flag
		rol spi_readbuffer	; Shift carry into readbuffer

		dec VIA_DATAB    	; set clock low
.endscope
.endrepeat

		lda spi_readbuffer	; result goes in A
		rts
.endproc

		


		
		
