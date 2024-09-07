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

.import _commandline, _print_string, _print_hex, _read_char, _print_char
.import setdevice, dev_open, dev_close, dev_putc, dev_getc
.import set_filename, set_filemode, openfile
.import dev_ioctl, dev_seek, dev_read, dev_get_status
.import mkdir, rmdir, rm, cp, mv, stat
.import RESET
;.export PutChar, GetChar, SET_FILENAME, SET_FILEMODE, DEV_OPEN, DEV_CLOSE, DEV_PUTC, DEV_GETC
;.export DEV_SEEK, DEV_GET_STATUS
;.export FS_MKDIR, FS_RMDIR, FS_RM, FS_CP, FS_MV


.segment "kernal_table"
FS_STAT:        jmp stat                ; FFB5
FS_MKDIR:       jmp mkdir               ; FFB8
FS_RMDIR:       jmp rmdir               ; FFBB
FS_RM:          jmp rm                  ; FFBE
FS_CP:          jmp cp                  ; FFC1
FS_MV:          jmp mv                  ; FFC4

DEV_IOCTL:      jmp dev_ioctl           ; FFC7
DEV_SEEK:       jmp dev_seek            ; FFCA
DEV_READ:       jmp dev_read            ; FFCD
DEV_GET_STATUS: jmp dev_get_status      ; FFD0
FILE_OPEN:      jmp openfile            ; FFD3
SET_FILENAME:   jmp set_filename        ; FFD6
SET_FILEMODE:   jmp set_filemode        ; FFD9
DEV_OPEN:       jmp dev_open            ; FFDC open current device
DEV_CLOSE:      jmp dev_close           ; FFDF close current device
CommandLine:    jmp _commandline        ; FFE2
Print_String:   jmp _print_string       ; FFE5
Print_Char:     jmp _print_char         ; FFE8 serial-specific routine
Read_Char:      jmp _read_char          ; FFEB serial-specific routine
PutHexit:       jmp _print_hex          ; FFEE
DEV_PUTC:       jmp dev_putc            ; FFF1 writes character to current device
DEV_GETC:       jmp dev_getc            ; FFF4 reads character from current device
SETDEVICE:      jmp setdevice           ; FFF7 sets current device - see devtab.asm

; interrupt vectors
.word $0200           	                ; NMI at $FFFA
.word RESET				                ; RESET at $FFFC
.word $0203           	                ; IRQ at $FFFE

