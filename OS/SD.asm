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

; 2024: Updated the signalling with CA1 and CA2 to be closer to EPP
; handshaking. This was because of occasional stalls during file reads.
; Not 100% sure if this was a complete fix, or if there are other
; issues involved.
; See http://wearcam.org/seatsale/programs/www.beyondlogic.org/epp/epp.htm
; for more info.

.include "OS3.inc"
.export SD_IOCTL, SD_GETC, SD_PUTC, SD_OPEN, SD_CLOSE, SD_SEEK
.import _print_hex, _print_char, dev_write_hex

;.pc02
		
.proc SD_IOCTL
			; Only current IOCTL for SD is init, A=0
			cmp #0
			bne error
			; Initialization 
			lda 	#%00001111
			sta		VIA_PCR		; set CA2 high, CA1 positive edge trigger
			stz		VIA_DDRA	; start in read mode
			lda 	VIA_ACR		
			;ora		#1
			and		#%11111110
			sta 	VIA_ACR		; disable latching for PA
			lda #0				; return P65_EOK
			tax
			rts
error:
			; Invalid ioctl number
			lda #P65_EINVAL
			ldx #$FF
			rts
.endproc



.macro Wait_CA1
.scope
wait:
			lda	VIA_IFR			; busy wait for IFR bit 1 to go high
			and	#$2
			beq	wait
			lda #$02
			sta VIA_IFR			; Clear IFR bit 1 (A == 2)
.endscope
.endmacro



.proc WriteByte
; problem: how do we make sure CA1 is low before we do this? or are we just safe?
			ldx 	#$ff		; DDR to write mode
			stx		VIA_DDRA

			sta		VIA_DATAA

			lda		#%00001101
			sta		VIA_PCR		; set CA2 low, CA1 still positive edge trigger

			Wait_CA1 			; CA1 high means peripheral has read the data

			stz		VIA_DDRA	; back to read mode
			ldx		#%00001110
			stx		VIA_PCR		; set CA2 high, CA1 negative

			pha
			Wait_CA1 			; CA1 trigger means peripheral has read the data
			pla
			;lda #'a'
			;jsr PutChar


			nop 
			nop
			nop

			rts
.endproc



.proc ReadByte
; problem: how do we make sure CA1 is low before we do this? or are we just safe?
			;ldx 	#$ff		; DDR to write mode
			;stx		VIA_DDRA

			;sta		VIA_DATAA

			lda		#%00001101
			sta		VIA_PCR		; set CA2 low, CA1 still positive edge trigger

			Wait_CA1 			; CA1 high means peripheral has prepared the data

			lda			VIA_DATAA

			;stz		VIA_DDRA	; back to read mode
			ldx		#%00001110
			stx		VIA_PCR		; set CA2 high, CA1 negative edge trigger

			pha
			Wait_CA1 			; CA1 trigger means peripheral has read the data
			pla

			nop
			nop 
			nop

			rts
.endproc


;=============================================================================
; SD_GETC
; Read a character from the SD card device.
;=============================================================================
; Get character.  Remember: non-blocking.  Carry is true if a character
; is ready.  
; Expects _SETDEVICE to be called to set the channel...
; Returns the character in A.  If EOF was received, X is $ff and carry is set.
; Uses A,X
;=============================================================================
.proc SD_GETC
		;ldx DEVICE_OFFSET
		;lda DEVTAB + DEVENTRY::CHANNEL,x
		lda DEVICE_CHANNEL
		jsr	WriteByte
		lda		#$19		; "gimme a byte" command
		jsr	WriteByte
		jsr	ReadByte		; esc-FF = EOF, esc-0 = none available, esc-esc = escape, rest are chars
							
		cmp #$1b	;esc
		bne return_byte
		jsr	ReadByte		; read 2nd byte of escape sequence
		cmp #$ff
		beq eof
		cmp #$00
		beq nochar
		; 2nd char wasn't 0 or -1, so return it (was probably an eactual ESC)
return_byte:
		sec 				; set carry to indicate a byte returned
		ldx #$00
		rts
		
nochar:
		clc
		rts					; no char read, so clear carry & return
eof:
		sec					; I guess eof counts as a token for this purpose, so set carry
		tax					; Copy the FF in A to X
		rts
.endproc




;=============================================================================
; SD_PUTC
;=============================================================================
; Write the character in A to the current SD DEVICE_CHANNEL. 
; Returns the character written in A.
; Uses A,X
;=============================================================================
.proc SD_PUTC
			phy					; save y, and save A in y
			tay
			;ldx DEVICE_OFFSET
			;lda DEVTAB + DEVENTRY::CHANNEL,x
			lda 	DEVICE_CHANNEL
			jsr WriteByte
			lda 	#$18 		; "send a byte" command
			jsr WriteByte
			tya
			jsr WriteByte
			tya					; return character written
			ply					; restore y
			rts
.endproc


; writes a pointer to SD_PUTC without the trailing 0
; argument is a pointer, which doesn't live in page 0.
.macro 		sd_writestr pointer
.scope
			lda pointer
			sta ptr1
			lda pointer+1
			sta ptr1h
			ldy #0
loop:		lda (ptr1),y
			beq done
			jsr SD_PUTC
			iny
			bne loop
done:		nop
.endscope
.endmacro
			


;=============================================================================
; SD_OPEN
;=============================================================================
; Open a file on the currently set DEVICE_CHANNEL.
; DEVICE_FILEMODE and DEVICE_FILENAME should be set with appropriate values.
; To open a file, we send an open message to the command channel (channel 0).
; For this driver, we're building up a command string starting with "o", the
; channel number, the file mode, and the filename.
; Filemode is byte value, not a character representation.
; The file mode is now a 1-byte set of UNIX-like flags, and probably not 
; printable. But also not ever 0!
; Uses: A,X,Y
;       ptr1
;=============================================================================
.proc SD_OPEN
			lda 	DEVICE_CHANNEL
			pha		; store device channel for later
			stz		DEVICE_CHANNEL	; send to command channel to send open.
			tay		; device channel in y
			
			lda		#'o'
			jsr		SD_PUTC
			
			tya		; channel back in a
			clc
			adc		#'0'	; convert number to ascii on the cheap
			jsr 	SD_PUTC
			
			lda		DEVICE_FILEMODE
			jsr		SD_PUTC
			
			sd_writestr DEVICE_FILENAME
			
			lda		#0			; explicit end-of-string
			jsr		SD_PUTC
			lda     #0
			jsr     SD_PUTC		; end of array-of-strings
			
			jsr		SD_GETC			; read back the return code
			; return code is in A.  1 is true, 0 is false
			
			cmp #0			; A >= 0 is success
			bpl return_ok
			ldx #$ff
			bra return				; 
return_ok:
			tay						; save result code
			ldx DEVICE_OFFSET
			lda DEVICE_FILEMODE
			sta DEVTAB + DEVENTRY::FILEMODE, X	; save device filemode to DEVTAB
			tya						; restore result code
			;lda #P65_EOK				; We're just returning EOK
			ldx #0

return:		ply						; pull dev channel off of stack
			sty		DEVICE_CHANNEL	; restore device channel
			rts
;return_err:	ply						; pull dev channel off of stack
;			sty		DEVICE_CHANNEL	; restore device channel
;			ldx #$ff
;			rts
			
.endproc



;=============================================================================
; SD_CLOSE
;=============================================================================
; Closes the file open on channel DEVICE_CHANNEL.
; The format sent to the Arduino is "cn", where n is the channel number.
; Uses A,X,Y
;=============================================================================
.proc SD_CLOSE
			lda 	DEVICE_CHANNEL
			tay		; device channel in y
			stz		DEVICE_CHANNEL
			
			lda		#'c'
			jsr		SD_PUTC
			
			tya
			clc
			adc		#'0'	; convert number to ascii on the cheap
			jsr 	SD_PUTC
			
			lda 	#0
			jsr		SD_PUTC
			lda 	#0
			jsr		SD_PUTC ; end of array-of-strings
			
			sty		DEVICE_CHANNEL	; restore device channel
			
			ldx     DEVICE_OFFSET
			stz 	DEVTAB + DEVENTRY::FILEMODE,x ; clear channel filemode

			rts
.endproc



;=============================================================================
; SD_SEEK
;=============================================================================
; Performs a seek operation on current DEVICE_CHANNEL.
; Arguments: A 4 byte offset in ptr1 and ptr2, a whence constant in A
; Returns: Return code in A. if A is P65_EOK, a 4 byte offset in ptr1 & ptr2. 
; Modifies AXY, ptr1, ptr2
;=============================================================================
.proc SD_SEEK
			; The command sent to the Arduino is a 7-byte sequence:
			; channel#, 0x1a, offset (32-bit little endian), whence
			pha		; Save whence value for later
			
			lda		DEVICE_CHANNEL
			jsr		WriteByte

			lda		#$1a			; seek command 
			jsr		WriteByte

			; offset is 4-bytes - little endian
			lda 	ptr1
			jsr		WriteByte
			lda		ptr1h
			jsr		WriteByte
			lda		ptr2
			jsr		WriteByte
			lda		ptr2h
			jsr		WriteByte

			pla		; recover whence
			jsr		WriteByte

			jsr 	ReadByte		; read return value
			cmp		#P65_EOK
			bne		ret_err

			; read back a 4-byte value - little endian
			jsr		ReadByte
			sta		ptr1
			jsr		ReadByte
			sta		ptr1h
			jsr		ReadByte
			sta		ptr2
			jsr		ReadByte
			sta		ptr2h

			lda		#P65_EOK
			tax
ret_err:
			rts
.endproc

