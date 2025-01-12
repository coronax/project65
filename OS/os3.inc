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

.p816			; allow 65c02 instructions
.feature string_escapes

BEL             = $07	; bell
TAB             = $09
LF              = $0a   ; line feed
ESC             = $1b   ; ESC to exit
CR              = $0d	; carriage return
QUOTE           = $22	; Quote mark
CTRL_D			= $04

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
	IOCTL     .word
	GETC	  .word
	PUTC      .word
	OPEN	  .word
	CLOSE	  .word
	SEEK	  .word
	READ	  .word
	WRITE     .word
.endstruct

; Control block for the TTY device. The tricky part is that it needs to be 
; able to quickly switch between the input and output devices and restore
; itself at the end.
.struct TTY_BLOCK
	MODE	.byte	; 0 for raw, 1 for cooked
	ECHO	.byte	; 0 for off, 1 for on.
	IN_DEV	.byte	; device index for input source
	OUT_DEV	.byte	; device index for echo output
	SELF_DEV .byte  ; our own device index
	TMPA		.byte ; store the last character read from input
	TMPX		.byte ; high byte of last character read from input
	EOF			.byte ; nonzero if the source has sent EOF (cooked only)
.endstruct

;=============================================================================
; Page Two usage
;=============================================================================

; $0200-$0205 are for IRQ and NMI redirection

; Real-time clock
; # of seconds since started (32-bit, since that's what c lib wants)
tod_seconds 	= $0206 ; tod seconds LSB
tod_seconds2	= $0207
tod_seconds3	= $0208
tod_seconds4	= $0209 ; tod seconds MSB
tod_seconds100  = $020a ; hundredths of seconds; effectively a count of clock interrupts

max3100_baud    = $020b ; an index to MAX3100_BAUD_TABLE

program_address_low=$20f		; pointers to begin/end of current loaded 
program_address_high=$210		
program_end_low   = $211		; Address of the byte following the last program byte.
program_end_high  = $212

; monitor variables
breakpoint_low    = $213
breakpoint_high	  = $214
breakpoint_value  = $215

; devtab management
CURRENT_DEVICE    = $0220	; devtab index of current device
DEVICE_CHANNEL    = $0221	; channel of current device
DEVICE_OFFSET     = $0222   ; offset used for navigating devtab structs
DEVICE_FILENAME   = $0223   ; filename for an open request
DEVICE_FILENAMEH  = $0224
DEVICE_FILEMODE   = $0225   ; Unix-style filemode for an open request.

TTY             = $0226		; sizeof TTY_BLOCK

buffer          = $0240	  	; Command line input buffer from $240 to $27f. 64 bytes.
argc		    = $0280   	; The input buffer will be parsed into a c-like
argv0L          = $0281   	; argv array, with the pointers pointing into
argv0H          = $0282   	; the buffer itself.  
argv1L          = $0283
argv1H          = $0284
argv2L          = $0285
argv2H          = $0286
argv3L          = $0287
argv3H          = $0288
argv4L          = $0289		; Final element of argv should be NULL.
argv4H          = $028a
argv_end        = $028b
program_ret     = $028b		; return code of called program
arg_is_quoted   = $028f     ; temp variable for argv parsing.
DEVTAB          = $0290   	; This is the devtab we actually use. 96 bytes, up to $02f0
NEXT_UNUSED     = $02F0		; One past end of DEVTAB

;=============================================================================
; Page Three usage
;=============================================================================

; serial port buffers
rbuffer 	= $0300		; read buffer. 128 bytes
wbuffer 	= $0380		; write buffer. 128 bytes

;=============================================================================
; Page Four usage
;=============================================================================

ttybuffer = $0400 		; TTY input buffer. Temporary. 128 bytes so we can reuse some serial port stuff?
scratchbuffer = $0480   ; Scratch buffer for OS commands. Used for handling filenames, etc.

;=============================================================================
; Page Zero usage
;=============================================================================

; My main constraint with zero page usage is that I want to maintain 
; compatibility with EHBasic, which uses it profigately. I'm currently using
; version 2.22 which uses $00-$13, $5b-$DF (or maybe $E1?) and $EF-$F0. 
; Yeesh!
; We're going to mainly use the space between $14 and $5a inclusive.
; And cc65 can use the block at $5b for its zero-page stuff.

; zero-page elements of TTY driver
wr_ttybuffer = $20		; ttybuffer write cursor
rd_ttybuffer = $21		; ttybuffer read cursor
cl_ttybuffer = $22		; ttybuffer start of current line

tmp1 = $23
tmp2 = $24
tmp3 = $25
tmp4 = $26

; zero-page elements of IO/serial driver
wr_rbuffer 		= $27		; Max3100 read buffer write index
rd_rbuffer 		= $28
wr_wbuffer 		= $29
rd_wbuffer 		= $2a		; Max3100 write buffer read index
spi_readbuffer	= $2b		; Used by spibyte
spi_writebuffer	= $2c		; Used by spibyte
statusbyte		= $2d		; Used by Max3100 code & other SPI drivers
max3100_rts_flag = $2e		; The RTS Flag value is $02 when it is enabled, 
							; meaning the remote side is allowed to send,
							; and 0 otherwise.
max3100_sending = $2f		; Used during Max3100_SendRecv

; IO/Serial constants:
SERIAL_RTS_ENABLE 	= $02
SERIAL_RTS_DISABLE 	= $00
SERIAL_BAUD_9600 	= %00001010
SERIAL_BAUD_19200	= %00001001
SERIAL_BAUD_38400	= %00001000

ptr1	= $30
ptr1h	= $31
ptr2	= $32
ptr2h	= $33

iptr1   = $34 ; a pointer or temp reserved for use in interrupt routines
iptr1h  = $35

; Also, the XModem routine uses some variables from
; $38 through $3f.

;=============================================================================
; Macros
;=============================================================================

.macro printstring string
        lda #<string
        ldx #>string
        jsr _print_string
.endmacro

.macro writedevice fd, string
		lda #fd
		jsr setdevice
		lda #<string
		ldx #>string
		jsr dev_writestr
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


;=============================================================================
; ioctl values
;=============================================================================
IO_INIT				= 0
IO_FLUSH			= 1
IO_AVAILABLE		= 2		; Returns # bytes available with blocking.

IO_SER_RATE			= 36	; kludge-can't overlap with IO_TTY_*

IO_TTY_ECHO_ON		= 32
IO_TTY_ECHO_OFF		= 33
IO_TTY_RAW_MODE		= 34
IO_TTY_COOKED_MODE	= 35


;=============================================================================
; P65 errno values. These are based off the cc65 errno values, but with
; bit 7 set so they're all negative values (except EOK).
;=============================================================================

P65_EOK      =   0
P65_ENOENT   =   $80 |  1
P65_ENOMEM   =   $80 |  2
P65_EACCES   =   $80 |  3
P65_ENODEV   =   $80 |  4
P65_EMFILE   =   $80 |  5
P65_EBUSY    =   $80 |  6
P65_EINVAL   =   $80 |  7
P65_ENOSPC   =   $80 |  8
P65_EEXIST   =   $80 |  9
P65_EAGAIN   =   $80 | 10
P65_EIO      =   $80 | 11
P65_EINTR    =   $80 | 12
P65_ENOSYS   =   $80 | 13
P65_ESPIPE   =   $80 | 14
P65_ERANGE   =   $80 | 15
P65_EBADF    =   $80 | 16
P65_ENOEXEC  =   $80 | 17
P65_EUNKNOWN =   $80 | 18

; OS-specific error codes
P65_ESYNTAX  =   $80 | 19
P65_ENOTDIR  =   $80 | 20
P65_EISDIR   =   $80 | 21
P65_EBADCMD  =   $80 | 22

