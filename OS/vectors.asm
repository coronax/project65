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
FS_STAT:        jmp stat                ; FF9F
FS_MKDIR:       jmp mkdir               ; FFA2
FS_RMDIR:       jmp rmdir               ; FFA5
FS_RM:          jmp rm                  ; FFA8
FS_CP:          jmp cp                  ; FFAB
FS_MV:          jmp mv                  ; FFAE

DEV_IOCTL:      jmp dev_ioctl           ; FFB1
DEV_SEEK:       jmp dev_seek            ; FFB4
DEV_READ:       jmp dev_read            ; FFB7
DEV_GET_STATUS: jmp dev_get_status      ; FFBA
FILE_OPEN:      jmp openfile            ; FFBD
SET_FILENAME:   jmp set_filename        ; FFC0
SET_FILEMODE:   jmp set_filemode        ; FFC3
DEV_OPEN:       jmp dev_open            ; FFC6 open current device
DEV_CLOSE:      jmp dev_close           ; FFC9 close current device
CommandLine:    jmp _commandline        ; FFCC
Print_String:   jmp _print_string       ; FFCF
Print_Char:     jmp _print_char         ; FFD2 serial-specific routine
Read_Char:      jmp _read_char          ; FFD5 serial-specific routine
PutHexit:       jmp _print_hex          ; FFD8
DEV_PUTC:       jmp dev_putc            ; FFDB writes character to current device
DEV_GETC:       jmp dev_getc            ; FFDE reads character from current device
SETDEVICE:      jmp setdevice           ; FFE1 sets current device - see devtab.asm

; interrupt vectors
.word RESET                             ; 65816 COP       at $FFE4 
.word $0203                             ; 65816 BREAK
.word RESET                             ; 65816 ABORT
.word $0200                             ; 65816 NMI
.word $0000                             ; 65816 UNUSED
.word $0203                             ; 65816 IRQ
.word $0000                             ; PADDING
.word $0000                             ; PADDING
.word RESET                             ;  6502 COP at $FFF4
.word $0203                             ;  6502 BREAK (UNUSED)
.word RESET                             ;  6502 ABORT at $FFF8
.word $0200           	                ;  6502 NMI at $FFFA
.word RESET				                ;  6502 RESET at $FFFC
.word $0203           	                ;  6502 IRQ at $FFFE

