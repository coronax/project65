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

.import _commandline, _outputstring, putc, getc, print_hex 
.import readchar, sendchar, _SETDEVICE, open, close
.import set_filename, set_filemode, openfile
.import RESET
.export PutChar, GetChar, SET_FILENAME, SET_FILEMODE, DEV_OPEN, DEV_CLOSE, DEV_PUTC, DEV_GETC

.segment "kernal_table"
FILE_OPEN:				;FFD1
        jmp openfile
SET_FILENAME:                           ;FFD4
        jmp set_filename
SET_FILEMODE:                           ;FFD7
        jmp set_filemode
DEV_OPEN:                               ;FFDA
        jmp open                        ; open current device
DEV_CLOSE:                              ;FFDD
        jmp close                       ; close current device
CommandLine:                            ;FFE0
        jmp _commandline
OutputString:                           ;FFE3
        jmp _outputstring
PutChar:                                ;FFE6
        jmp sendchar                    ; serial-specific routine
GetChar:                                ;FFE9
        jmp readchar                    ; serial-specific routine
PutHexit:                               ;FFEC
        jmp print_hex
DEV_PUTC:				;FFEF
		jmp putc                ; writes character to current device
DEV_GETC:				;FFF2
		jmp getc                ; reads character from current device
SETDEVICE:				;FFF5
		jmp _SETDEVICE          ; sets current device - see devtab.asm

; pad out to $FFFA
.byte $00, $00 

; interrupt vectors
.word $0200           	                ; NMI at $FFFA
.word RESET				; RESET at $FFFC
.word $0203           	                ; IRQ at $FFFE

