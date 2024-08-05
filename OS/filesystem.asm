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

;; filesystem.asm - file & directory operations

.include "os3.inc"
.import set_filename, setdevice, dev_writestr, dev_getc, dev_putc
.import mkdir_command, rmdir_command, rm_command, cp_command
.export mkdir, rmdir, rm, cp;, mv, cp


; Create a new directory 
; AX points to a string with a pathname relative to current directory.
; Returns: P65_EOK or error code in A
; Modifies AXY, ptr1
.proc mkdir
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<mkdir_command
        ldx #>mkdir_command
        jmp dos_singlearg
.endproc

.proc rmdir
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<rmdir_command
        ldx #>rmdir_command
        jmp dos_singlearg
.endproc

.proc rm
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<rm_command
        ldx #>rm_command
        jmp dos_singlearg
.endproc

; helper for mkdir etc. expects operator string in AX,
; argument in DEVICE_FILENAME, and current device is SD command.
; Modifiees AXY, ptr1
.proc dos_singlearg
        jsr dev_writestr
        lda DEVICE_FILENAME
        ldx DEVICE_FILENAME+1
        jsr dev_writestr
   		lda #0
		jsr dev_putc		; and a marker for end of array-of-strings
        jsr dev_getc        ; read response code
        rts
.endproc


.proc cp
        jsr set_filename
        lda #1
        jsr setdevice      ; send to SD card command channel.
        lda #<cp_command
        ldx #>cp_command
        jmp dos_doublearg
.endproc


; helper for cp & mv. expects operator string in AX,
; src argument in DEVICE_FILENAME, dest argument in ptr2
; and current device is SD command channel.
; Modifiees AXY, ptr1, ptr2
.proc dos_doublearg
        jsr dev_writestr
        lda DEVICE_FILENAME
        ldx DEVICE_FILENAME+1
        jsr dev_writestr    ; Write source name
        lda ptr2
        ldx ptr2h
        jsr dev_writestr    ; Write dest name
   		lda #0              ; and a marker for end of array-of-strings
		jsr dev_putc		
        jsr dev_getc        ; read response code
        rts
.endproc
