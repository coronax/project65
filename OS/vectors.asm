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

.import _commandline, _outputstring, dev_putc, dev_getc, print_hex 
.import readchar, sendchar, setdevice, dev_open, dev_close
.import set_filename, set_filemode, openfile
.import dev_ioctl, dev_seek, dev_tell, dev_get_status
.import RESET
.export PutChar, GetChar, SET_FILENAME, SET_FILEMODE, DEV_OPEN, DEV_CLOSE, DEV_PUTC, DEV_GETC
.export DEV_SEEK, DEV_TELL, DEV_GET_STATUS

.segment "kernal_table"
DEV_IOCTL:      jmp dev_ioctl           ; FFC5
DEV_SEEK:       jmp dev_seek            ; FFC8
DEV_TELL:       jmp dev_tell            ; FFCB
DEV_GET_STATUS: jmp dev_get_status      ; FFCE
FILE_OPEN:      jmp openfile            ; FFD1
SET_FILENAME:   jmp set_filename        ; FFD4
SET_FILEMODE:   jmp set_filemode        ; FFD7
DEV_OPEN:       jmp dev_open            ; FFDA open current device
DEV_CLOSE:      jmp dev_close           ; FFDD close current device
CommandLine:    jmp _commandline        ; FFE0
OutputString:   jmp _outputstring       ; FFE3
PutChar:        jmp sendchar            ; FFE6 serial-specific routine
GetChar:        jmp readchar            ; FFE9 serial-specific routine
PutHexit:       jmp print_hex           ; FFEC
DEV_PUTC:       jmp dev_putc            ; FFEF writes character to current device
DEV_GETC:       jmp dev_getc            ; FFF2 reads character from current device
SETDEVICE:      jmp setdevice           ; FFF5 sets current device - see devtab.asm

; pad out to $FFFA
.byte $00, $00 

; interrupt vectors
.word $0200           	                ; NMI at $FFFA
.word RESET				; RESET at $FFFC
.word $0203           	                ; IRQ at $FFFE

