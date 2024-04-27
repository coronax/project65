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

.pc02			; allow 65c02 instructions
.feature string_escapes

CR		=        $0d                ; carriage return
LF      =        $0a                ; line feed
ESC     =        $1b                ; ESC to exit

;Device Filemode values - These match the values used by CC65's fcntl.inc.
O_RDONLY        = $01
O_WRONLY        = $02
O_RDWR          = $03
O_CREAT         = $10
O_TRUNC         = $20
O_APPEND        = $40
O_EXCL          = $80

; DEVTAB entry format
.struct DEVENTRY
	CHANNEL   .byte
	FILEMODE  .byte   ; contains file mode if the dev is open.
	INIT      .word
	GETC	  .word
	PUTC      .word
	OPEN	  .word
	CLOSE	  .word
.endstruct

;=============================================================================
; Page Two usage
;=============================================================================

; $0200-$0205 are for IRQ and NMI redirection

; Real-time clock
		; # of seconds since started (16 bit)
tod_seconds 	= $0206
tod_secondsH	= $0207

		; countdown # of interrupts per second.
tod_int_counter = $0208

spi_readbuffer	= $0209	; Used by spibyte
spi_writebuffer	= $020a	; Used by spibyte

program_address_low=$20f		; pointers to begin/end of current loaded 
program_address_high=$210		
program_end_low=$211			; *Is end the last byte or one past?
program_end_high=$212

; monitor variables
breakpoint_low=$213
breakpoint_high	= $214
breakpoint_value  = $215

SCRATCH = $216			; scratch for table jumps. 2 bytes.
SCRATCHH = $217

; devtab management
CURRENT_DEVICE    = $0220	; devtab index of current device
DEVICE_CHANNEL    = $0221	; channel of current device
DEVICE_OFFSET     = $0222   ; offset used for navigating devtab structs
DEVICE_FILENAME   = $0223   ; filename for an open request
DEVICE_FILENAMEH  = $0224
DEVICE_FILEMODE   = $0225   ; Unix-style filemode for an open request.

buffer          = $0240	  	; Command line input buffer from $240 to $27f.
argc		    = $0280   	; The input buffer will be parsed into a c-like
argv0L          = $0281   	; argv array, with the pointers pointing into
argv0H          = $0282   	; the buffer itself.  
argv1L          = $0283
argv1H          = $0284
argv2L          = $0285
argv2H          = $0286
argv3L          = $0287
argv3H          = $0288

DEVTAB          = $0290   	; This is the devtab we actually use. 64 bytes, up to $02d0
NEXT_UNUSED     = $02D0		; One past end of DEVTAB

;=============================================================================
; Page Three usage
;=============================================================================

; serial port buffers
rbuffer 	= $0300		; read buffer. 128 bytes
wbuffer 	= $0380		; write buffer. 128 bytes

;=============================================================================
; Page Zero usage
;=============================================================================

wr_rbuffer 	= $00e2		; Max3100 read buffer read index
rd_rbuffer 	= $00e3
wr_wbuffer 	= $00e4
rd_wbuffer 	= $00e5		; Max3100 write buffer write index

statusbyte		= $e6	; Used by Max3100 code & other SPI drivers
max3100_rts_status = $e7
max3100_sending = $e8	; Used during Max3100_SendRecv
; we also have a kludge below using $EE

ptr1	= $e9
ptr1h	= $ea
ptr2	= $eb
ptr2h	= $ec

;=============================================================================
; Macros
;=============================================================================

.macro printstring string
        lda #<string
        ldx #>string
        jsr _outputstring
.endmacro

.macro writedevice fd, string
		lda #fd
		jsr _SETDEVICE
		lda #<string
		ldx #>string
		jsr write
.endmacro

VIA_ADDRESS 	= $C000
VIA_DDRA		= VIA_ADDRESS + $3
VIA_DDRB		= VIA_ADDRESS + $2
VIA_DATAA		= VIA_ADDRESS + $1
VIA_DATAB		= VIA_ADDRESS + $0
VIA_ACR			= VIA_ADDRESS + $B      ; auxiliary control register
VIA_SHIFT		= VIA_ADDRESS + $A
VIA_PCR			= VIA_ADDRESS + $C		; Peripheral Control Register, e.g. PB2
VIA_IFR			= VIA_ADDRESS + $D		; Interrupt Flag Register
VIA_IER			= VIA_ADDRESS + $E		; Interrupt Enable Register
VIA_TIMER1_CL	= VIA_ADDRESS + $4		; timer 1 low-order latch/counter
VIA_TIMER1_CH	= VIA_ADDRESS + $5		; timer 1 high-order counter
VIA_TIMER1_LL	= VIA_ADDRESS + $6		; timer 1 low-order latch
VIA_TIMER1_LH	= VIA_ADDRESS + $7		; timer 1 high-order latch

