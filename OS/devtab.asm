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

;; devtab.asm - generic device table support.

.include "os3.inc"
.import SERIAL_IOCTL, SERIAL_GETC, SERIAL_PUTC
.import SD_IOCTL, SD_GETC, SD_PUTC, SD_OPEN, SD_CLOSE, SD_SEEK, SD_READ, SD_WRITE
.import TTY_IOCTL, TTY_GETC, TTY_OPEN, TTY_CLOSE
.import _print_string, hexits, _print_hex
.export dev_ioctl, dev_getc, dev_putc, setdevice, set_filename, set_filemode, dev_open
.export dev_close, init_devices, openfile
.export dev_seek, dev_get_status
.export dev_read, dev_write, dev_writestr, dev_write_hex


;CURRENT_DEVICE    = $0220
;DEVICE_CHANNEL    = $0221
;DEVICE_OFFSET     = $0222
;DEVICE_FILENAME   = $0223
;DEVICE_FILENAMEH  = $0224
;DEVICE_FILEMODE   = $0225



;.struct DEVENTRY
;	CHANNEL   .byte
;	FILEMODE  .byte   ; contains file mode if the dev is open, 0 if closed
;	INIT      .word
;	GETC	  .word
;	PUTC      .word
;	OPEN	  .word
;	CLOSE	  .word
;   SEEK      .word
;   READ      .word
;   WRITE     .word
;.endstruct
	
.rodata
.asciiz"rodata"


;.align 16
DEVTAB_TEMPLATE:		; an array of DEVENTRY structs
.byte 0, 0	; Serial Port
.word SERIAL_IOCTL, SERIAL_GETC, SERIAL_PUTC, NULLFN, NULLFN, NOSEEK, DEFAULT_READ, DEFAULT_WRITE

;.align 16
.byte 0, 0	; SD Card IO Channel
.word SD_IOCTL, SD_GETC, SD_PUTC, SD_OPEN, SD_CLOSE, NOSEEK, SD_READ, SD_WRITE

;.align 16
.byte 1, 0	; SD Card Data Channel 1
.word SD_IOCTL, SD_GETC, SD_PUTC, SD_OPEN, SD_CLOSE, SD_SEEK, SD_READ, SD_WRITE

;.align 16
.byte 2, 0	; SD Card Data Channel 2
.word SD_IOCTL, SD_GETC, SD_PUTC, SD_OPEN, SD_CLOSE, SD_SEEK, SD_READ, SD_READ

;.align 16
.byte 0, 0	; TTY Device; attaches on top of serial device
.word TTY_IOCTL, TTY_GETC, NULLFN, TTY_OPEN, TTY_CLOSE, NOSEEK, DEFAULT_READ, DEFAULT_WRITE

;.align 16
.byte 0, 0	; Empty Device 2
.word NULLFN, NULLFN, NULLFN, NULLFN, NULLFN, NOSEEK, NULLRW, NULLRW
DEVTAB_TEMPLATE_END:

.code

; A generic empty function. Any devtab entry that doesn't need a specific
; handler can point to this function. If called, returns P65_ENOSYS.
.proc		NULLFN
			lda #P65_ENOSYS
			ldx #$FF
			rts
.endproc

; An empty function that does stack cleanup required by read/write
.proc NULLRW
			pla
			pla
			lda #P65_ENOSYS
			ldx #$FF
			rts
.endproc


; A generic response for devices that cannot be seeked. 
; Returns error code in A.
.proc 		NOSEEK
			lda #P65_ESPIPE
			ldx #$FF
			rts
.endproc			


; Completely initializes all devices and the DEVTAB structure
.proc init_devices
		ldx #(DEVTAB_TEMPLATE_END - DEVTAB_TEMPLATE);64				; copy DEVTAB_TEMPLATE to DEVTAB in RAM
loop:	lda DEVTAB_TEMPLATE-1,X
		sta DEVTAB-1,X
		dex
		bne loop

		lda #0				; init serial port
		jsr	setdevice
		jsr init_io
		lda #1				; init SD card
		jsr	setdevice
		jsr init_io
		lda #4
		jsr setdevice
		jsr init_io
		rts
.endproc


; devnum in A
; Modifies AX
.proc 		setdevice
			sta		CURRENT_DEVICE
			asl						; x2
			asl						; x4
			asl						; x8
			adc 	CURRENT_DEVICE	; x9
			asl						; x18
			sta		DEVICE_OFFSET
			tax
			lda		DEVTAB + DEVENTRY::CHANNEL, X
			sta		DEVICE_CHANNEL
			rts
.endproc			


; Initialize the current device.
; Init is shorthand for IOCTL with A=0
; Returns 0 in AX for success, -1 for failure
; Modifies AX
.proc init_io
			lda #0
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::IOCTL,X)
.endproc



; Initialize the current device.
; device-specific operation is in A
; Returns 0 in AX for success, -1 for failure
; Modifies AX
; Standard Ioctl operations:
;    0 - initialize
;    1 - flush
.proc dev_ioctl
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::IOCTL,X)
.endproc


; Get a character from the current device.
; If no character is available yet, carry is clear
; If EOF is reached, AX holds $FFFF
; Otherwise, returns character in A (and 0 in X).
; Modifies AX
.proc dev_getc
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::GETC,X)
.endproc
			


; Writes the character in A to the current device.
; Modifies AX
.proc dev_putc
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::PUTC,X)
.endproc


; Opens the current file device
; Uses DEVICE_FILENAME and DEVICE_FILEMODE
; Modifies AXY
; Change to: On success, returns in AX:
;   0: stream
;   1: regular file
;   2: directory
; Returns a P65 error code on failure (-1 in X, code in A).
.proc dev_open
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::OPEN,X)
.endproc


; Closes the current device.
; Modifies AXY
.proc dev_close
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::CLOSE,X)
.endproc


; Seeks on the current device
; Arguments: A 4 byte offset in ptr1 and ptr2, a whence constant in A
; Returns: An error code in A. if A is P65_EOK, a 4 byte offset in ptr1 and ptr2. 
; Modifies AXY, ptr1, ptr2
.proc dev_seek
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::SEEK,X)
.endproc


; Read bytes from the current device. 
; ptr1 points to a buffer. AX contains # of bytes to be read.
; Returns number of bytes read in AX. 0 for end of file,
; or a P65 error code
; Uses AXY, tmp1, tmp2, tmp3, ptr1
.proc dev_read
			; Note that actual implementations read count from the stack.
			tay			; save low byte
			txa			; high byte to a
			and #$7F	; clear high bit - truncate count to max signed int
			pha			; push to stack
			tya			; retrieve low byte
			pha			; and push
			;phx	; note that actual implementations have to read # bytes from stack
			;and #$7F ; truncate count to max signed int size
			;pha
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::READ,X)
.endproc


; Write bytes to the current device. 
; ptr1 points to a buffer. AX contains # of bytes to write.
; Returns number of bytes written in AX, or a P65 error code.
; Uses AXY, tmp1, tmp2, tmp3, ptr1
.proc dev_write
			; Note that actual implementations read count from the stack.
			tay			; save low byte
			txa			; high byte to a
			and #$7F	; clear high bit - truncate count to max signed int
			pha			; push to stack
			tya			; retrieve low byte
			pha			; and push
			;phx	; note that actual implementations have to read # bytes from stack
			;and #$7F ; truncate count to max signed int size
			;pha
			ldx DEVICE_OFFSET
			jmp (DEVTAB + DEVENTRY::WRITE,X)
.endproc


.proc dev_get_status
			ldx DEVICE_OFFSET
			lda DEVTAB + DEVENTRY::FILEMODE,X
			rts
.endproc


; Sets the filename to use for the next open command.
; filename in AX
; the name is not copied; we just apply it to the next open call
.proc set_filename
			sta DEVICE_FILENAME
			stx DEVICE_FILENAME+1
			rts
.endproc


; Sets the filemode for the next open command.
; filemode in A, as a Unix style set of flags
.proc set_filemode
			sta DEVICE_FILEMODE
			rts
.endproc


; CJ This should be moved into Filesystem and be given some smarts
; to be able to open non-disk devices based on filename 
; (e.g. "SERIAL:9600" or something like that.)
; openfile is meant to be called by application code.
; pass the name of a file an AX, mode in Y
; returns a file handle in A, or -1 on failure
; Allocates a file handle if one is available.
; For now, I'm hardcoding it to look at devtab entries 2 & 3,
; but we could add special identifiers for serial port or 
; disk command channel...
.proc openfile
			pha		
			phx
			phy
			lda		#2
			jsr		setdevice
			ldx		DEVICE_OFFSET
			cmp		DEVTAB + DEVENTRY::FILEMODE,X
			beq		do_open
			lda 	#3
			jsr		setdevice
			ldx		DEVICE_OFFSET
			cmp		DEVTAB + DEVENTRY::FILEMODE,X
			beq		do_open
			lda		#P65_EMFILE			; No available file descriptors
			ldx		#$ff
			rts
do_open:
			pla		
			jsr		set_filemode
			plx
			pla	
			jsr		set_filename
			jsr		dev_open
			bne		on_fail			; return whatever dev_open returns
			lda		CURRENT_DEVICE
			ldx		#0				; because cc65 __fopen wants us to return a 16-bit value
on_fail:	rts
;on_fail:	lda		#$ff
;
;			rts
.endproc



; Writes a string to the current device, including terminating 0.
; Maximum string length is 256 bytes.
; String pointer in ax
; Uses A,X,Y, ptr1
.proc dev_writestr
			sta 	ptr1
			stx		ptr1h
			ldy 	#0
outloop:	lda 	(ptr1),y
			jsr		dev_putc
			cmp #0;lda (ptr1),y				 ; check eos after we've written 
			beq		doneoutputstring ; it to the stream
			iny
			bne 	outloop ; bne so we don't loop forever on long strings
doneoutputstring:
			rts
.endproc



; Writes A to the current device as a 2-character hex string.
; Now preserves A!
; Uses AX
.proc dev_write_hex
		pha
		ror
		ror
		ror
		ror
		and #$0F
		tax
		lda hexits,x
		jsr dev_putc
		pla
        pha
		and #$0F
		tax
		lda hexits,x
		jsr dev_putc
        pla
        rts
.endproc		



; Read bytes from the current device. 
; ptr1 points to a buffer. Top of stack contains # of bytes to be read.
; Returns number of bytes read in AX. 0 for end of file,
; or a P65 error value
; Uses AXY, tmp1, tmp2, tmp3, ptr1
.proc DEFAULT_READ
		pla				; Recover # of bytes to read from stack
		plx
        cmp #0
        bne nonzero
        cpx #0
        bne nonzero
        rts             ; count is 0, so we can just return 0.
nonzero:
        sta tmp1        ; low byte of count
        stx tmp2        ; high byte of count

        stz tmp3        ; initialize read count
        ldy #0
 
loop:
        jsr dev_getc
        bcc loop
        cpx #$FF
        beq done        ; $FF in X means we received EOF
        sta (ptr1),y    ; save character to buffer
        iny
        bne test_count
        inc tmp3        ; if Y rolled over, we need to increment
        inc ptr1+1      ; tmp3 and ptr1+1

test_count:
        ; if y/tmp3 == tmp1/tmp2, we are ready to exit.
        cpy tmp1
        bne loop
        lda tmp3
        cmp tmp2
        bne loop

done:
        tya             ; load read count into AX
        ldx tmp3
        rts
.endproc



; Write bytes to the current device. 
; ptr1 points to a buffer. Top of stack contains # of bytes to write.
; Returns number of bytes written in AX or a P65 error value
; Uses AXY, tmp1, tmp2, tmp3, ptr1
; This default implementation uses dev_putc, which can't actually
; report failure, so will always appear to write count bytes.
; Be advised, and address that as we become more of a real operating
; system.
.proc DEFAULT_WRITE
		pla				; Recover # of bytes to read from stack
		plx
        cmp #0
        bne nonzero
        cpx #0
        bne nonzero
        rts             ; count is 0, so we can just return 0.
nonzero:
        sta tmp1        ; low byte of count
        stx tmp2        ; high byte of count

        stz tmp3        ; initialize read count
        ldy #0
 
loop:
        lda (ptr1),y    ; save character to buffer
        jsr dev_putc
        iny
        bne test_count
        inc tmp3        ; if Y rolled over, we need to increment
        inc ptr1+1      ; tmp3 and ptr1+1

test_count:
        ; if y/tmp3 == tmp1/tmp2, we are ready to exit.
        cpy tmp1
        bne loop
        lda tmp3
        cmp tmp2
        bne loop

done:
        tya             ; load write count into AX
        ldx tmp3
        rts
.endproc



