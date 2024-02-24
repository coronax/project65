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

; Because Insitu can rewrite the system ROM, it cannot itself use any ROM
; routines. The first thing it does is disable interrupts, and following 
; that it uses these entirely polling-based IO routines, which were 
; adapted from an early version of the P:65 OS, along with the Ricor/Archer
; XModem routine.

.export init_io, _outputstring, _sendchar, readchar, _print_hex , _XModem, _getch

CR	=        $0d                ; carriage return
LF      =        $0a                ; line feed
ESC     =        $1b                ; ESC to exit

;buffer=$1000
chartosend=$207         ; store send char for _sendchar
readbuffer=$209         ; temp read buffer for rwbit
writebuffer=$20a        ; temp write buffer for rwbit
statusbyte=$20b         ; store status byte for readchar
program_address_low=$20c    ; store the start of the XModem destination buffer here.
program_address_high=$20d


; zero-page scratch space
ptr1	:= $fb
ptr1h	:= $fc
ptr2	:= $fd
ptr2h	:= $fe


.macro printstring string
        lda #<string
        ldx #>string
        jsr _outputstring
.endmacro

VIA_ADDRESS 	= $C000
VIA_DDRA	= VIA_ADDRESS + $3
VIA_DDRB	= VIA_ADDRESS + $2
VIA_DATAA	= VIA_ADDRESS + $1
VIA_DATAB	= VIA_ADDRESS + $0
VIA_ACR		= VIA_ADDRESS + $B          ; auxiliary control register
VIA_SHIFT	= VIA_ADDRESS + $A


.code
;.byte "io.asm"

; The IO code uses Port B for bitbanged SPI I/O.
;   PB0 = SPI Clock
;   PB1 = chip select for Max3100
;   PB2 = MOSI
;   PB3 = MISO (old)
;   PB4 = SD card chip select
;	PB5 = interrupt from Max3100 (experimental)
;   PB7 = MISO (new)

init_io:
	; disable interrupts - insitu is all-polling for now
	sei
		
        ; set up parallel port
        lda #$77		; reading on pins 3 (old miso) and 7 (new miso)
        sta VIA_DDRB
	lda #$00
	sta VIA_DDRA
        lda #$18		; ACR - disable latching & setup shift register
        sta VIA_ACR		; shift register shift out with phi2 clock
        ldx #0

        ; a restart signal for the logic analyzer
        lda #$10
        sta VIA_DATAB
        nop
        nop
        nop
        lda #$02		; max3100 ss high
        sta VIA_DATAB

        jsr initmax3100
	rts



; initialize max3100 via SPI
initmax3100:
        	; toggle slave select - and include a full toggle just
                ; in case the slave select wasn't set correctly to
                ; begin with.
                lda 	VIA_DATAB
                ora	#2
                sta	VIA_DATAB
                eor 	#2
                sta 	VIA_DATAB

                ; send first 8 bits
                lda 	#%11000000
                jsr	spibyte

        	; send second 8 bits
        	lda 	#%00001010 ; 9600 baud
        	jsr 	spibyte

                lda	VIA_DATAB
                ora 	#2
                sta 	VIA_DATAB	; turn off slave select			

                rts


; Sends the character in accumulator out the spi port.
; If the max3100 reports that the send buffer is full,
; we retry until we're successful.
_sendchar:
		sta 	chartosend
retrysend:
		; set slave select
		lda 	VIA_DATAB
		eor		#2
		sta 	VIA_DATAB

		; send write command
		lda 	#$82	; enable RTS always
		jsr 	spibyte

		; save the status byte we just read
		sta 	statusbyte

		; send 8 bits of char data
		lda 	chartosend
		jsr 	spibyte
			
		lda		VIA_DATAB
		ora 	#2
		sta 	VIA_DATAB	; turn off slave select			

		; if bit 6 of the first byte we read (command)
		; is 0, the buffer was full and the send didn't
		; go thru.
		lda 	statusbyte
		and 	#$40
		beq 	retrysend

		; for ehbasic, we need to get the original char
		; back in A.  Conveniently, we've rotated all the
		; way around.
		lda 	chartosend
		rts


; Blocking read character
_getch:
		clc
get:     	jsr readchar
		bcc get
		rts


; read character
readchar:
		; toggle slave select
		lda 	VIA_DATAB
		eor		#2
		sta 	VIA_DATAB

		; send 8 bit read command; read status
		; to send a bit, we rol to send the
		; high bit of command to the carry flag,
		; and then call sendcarrybit
		lda 	#$00
		jsr 	spibyte
		sta 	statusbyte

		; send $00, read 8 bits of char data
		lda 	#0
		jsr 	spibyte

		lda		VIA_DATAB
		ora 	#2
		sta 	VIA_DATAB	; turn off slave select			

		; high bit of statusbyte is 1 iff we read
		; a real character. rol to put that bit in
		; carry, and load whatever we read into A
		rol 	statusbyte
		lda 	readbuffer  
		rts

spibyte:
		sta 	writebuffer
		jsr 	rwbit
		jsr 	rwbit
		jsr 	rwbit
		jsr 	rwbit
		jsr 	rwbit
		jsr 	rwbit
		jsr 	rwbit
		jsr 	rwbit
		lda 	readbuffer
		rts


; rwbit writes one bit of writebuffer and reads
; one bit into readbuffer.  Both buffers are
; left shifted by one place.
rwbit:
		rol 	writebuffer
		bcs 	nonzero
		lda	VIA_DATAB
		and	#%11111011	; set MOSI low
nzret:   	sta 	VIA_DATAB       ; set output bit
		inc 	VIA_DATAB       ; set clock high    
		; receive bit
		asl 	readbuffer
		lda 	VIA_DATAB       ; read input bit
		and 	#$80
		beq 	reczero
		inc 	readbuffer      ; add a 1 bit 
reczero:
		dec 	VIA_DATAB       ; set clock low
		rts
nonzero: 
		lda 	VIA_DATAB
		ora	#4		; set MOSI high
		jmp 	nzret

			

; Prints a zero-terminated string of up to 255 characters	
; Uses A, Y, and ptr1		
_outputstring:
                sta     ptr1
                stx     ptr1h
                ldy     #0
outloop:        
                lda    (ptr1),y
                beq     doneoutputstring
                jsr     _sendchar
                iny
                bne     outloop ; bne so we don't loop forever on long strings
doneoutputstring:
                rts

		
		
; prints the 2-character hex representation
; of the value in A
_print_hex:
		pha
	   ; sta temp
		ror
		ror
		ror
		ror
		and #$0F
		tax
		lda hexits,x
		jsr _sendchar
		pla
	   ; lda temp
		and #$0F
		tax
		lda hexits,x
		jmp _sendchar ; cheap rts

hexits:
		.byte "0123456789ABCDEF"
			
			

; XMODEM/CRC Receiver for the 65C02
;
; By Daryl Rictor & Ross Archer  Aug 2002
;
; 21st century code for 20th century CPUs (tm?)
; 
; A simple file transfer program to allow upload from a console device
; to the SBC utilizing the x-modem/CRC transfer protocol.  Requires just
; under 1k of either RAM or ROM, 132 bytes of RAM for the receive buffer,
; and 8 bytes of zero page RAM for variable storage.
;
;**************************************************************************
; This implementation of XMODEM/CRC does NOT conform strictly to the 
; XMODEM protocol standard in that it (1) does not accurately time character
; reception or (2) fall back to the Checksum mode.

; (1) For timing, it uses a crude timing loop to provide approximate
; delays.  These have been calibrated against a 1MHz CPU clock.  I have
; found that CPU clock speed of up to 5MHz also work but may not in
; every case.  Windows HyperTerminal worked quite well at both speeds!
;
; (2) Most modern terminal programs support XMODEM/CRC which can detect a
; wider range of transmission errors so the fallback to the simple checksum
; calculation was not implemented to save space.
;**************************************************************************
;
; Files uploaded via XMODEM-CRC must be
; in .o64 format -- the first two bytes are the load address in
; little-endian format:  
;  FIRST BLOCK
;     offset(0) = lo(load start address),
;     offset(1) = hi(load start address)
;     offset(2) = data byte (0)
;     offset(n) = data byte (n-2)
;
; Subsequent blocks
;     offset(n) = data byte (n)
;
; The TASS assembler and most Commodore 64-based tools generate this
; data format automatically and you can transfer their .obj/.o64 output
; file directly.  
;   
; The only time you need to do anything special is if you have 
; a raw memory image file (say you want to load a data
; table into memory). For XMODEM you'll have to 
; "insert" the start address bytes to the front of the file.
; Otherwise, XMODEM would have no idea where to start putting
; the data.


;-------------------------- The Code ----------------------------
;
; zero page variables (adjust these to suit your needs)
;
;
crc         = $38                ; CRC lo byte  (two byte variable)
crch        = $39                ; CRC hi byte  

ptr         = $3a                ; data pointer (two byte variable)
ptrh        = $3b                ;   "    "

blkno       = $3c                ; block number 
retry       = $3d                ; retry counter 
retry2		= $3e                ; 2nd counter
bflag       = $3f                ; block flag 
;
;
; non-zero page variables and buffers
;
;
Rbuff       = $0400              ; temp 132 byte receive buffer 
                                 ;(place anywhere, page aligned)
;
;
;  tables and constants
;
;
; The crclo & crchi labels are used to point to a lookup table to calculate
; the CRC for the 128 byte data blocks.  There are two implementations of these
; tables.  One is to use the tables included (defined towards the end of this
; file) and the other is to build them at run-time.  If building at run-time,
; then these two labels will need to be un-commented and declared in RAM.
;
;crclo                =        $7D00              ; Two 256-byte tables for quick lookup
;crchi                =         $7E00              ; (should be page-aligned for speed)
;
;
;
; XMODEM Control Character Constants
SOH		=        $01                ; start block
EOT     =        $04                ; end of text marker
ACK     =        $06                ; good block acknowledged
NAK     =        $15                ; bad block acknowledged
CAN     =        $18                ; cancel (not standard, not supported)
;CR      =        $0d                ; carriage return
;LF      =        $0a                ; line feed
;ESC     =        $1b                ; ESC to exit

;
;^^^^^^^^^^^^^^^^^^^^^^ Start of Program ^^^^^^^^^^^^^^^^^^^^^^
;
; Xmodem/CRC upload routine
; By Daryl Rictor, July 31, 2002
;
; v0.3  tested good minus CRC
; v0.4  CRC fixed!!! init to $0000 rather than $FFFF as stated   
; v0.5  added CRC tables vs. generation at run time
; v 1.0 recode for use with SBC2
; v 1.1 added block 1 masking (block 257 would be corrupted)

.code


_XModem:     jsr     PrintMsg        ; send prompt and info
            lda     #$01
            sta     blkno           ; set block # to 1
            sta     bflag           ; set flag to get address from block 1
StartCrc:   lda     #'C'            ; "C" start with CRC mode
            jsr     Put_Chr         ; send it
            lda     #$FF        
            sta     retry2          ; set loop counter for ~3 sec delay
            lda     #$00
            sta     crc
            sta     crch            ; init CRC value        
            jsr     GetByte         ; wait for input
            bcs     GotByte         ; byte received, process it
            bcc     StartCrc        ; resend "C"

StartBlk:   lda  	#$FF            ; 
            sta     retry2          ; set loop counter for ~3 sec delay
            lda     #$00            ;
            sta     crc             ;
            sta     crch            ; init CRC value        
            jsr     GetByte         ; get first byte of block
            bcc     StartBlk        ; timed out, keep waiting...
GotByte:    cmp     #ESC            ; quitting?
            bne     GotByte1        ; no
; cj abort xmodem
			lda 	#$FE			; Error code in "A" of desired
			rts
GotByte1:   cmp     #SOH            ; start of block?
            beq     BegBlk          ; yes
            cmp     #EOT            ;
            bne     BadCrc          ; Not SOH or EOT, so flush buffer & send NAK        
            jmp     Done            ; EOT - all done!
BegBlk:     ldx     #$00
GetBlk:     lda     #$ff            ; 3 sec window to receive characters
            sta     retry2          ;
GetBlk1:    jsr     GetByte         ; get next character
            bcc     BadCrc          ; chr rcv error, flush and send NAK
GetBlk2:    sta     Rbuff,x         ; good char, save it in the rcv buffer
            inx                     ; inc buffer pointer        
            cpx     #$84            ; <01> <FE> <128 bytes> <CRCH> <CRCL>
            bne     GetBlk          ; get 132 characters
            ldx     #$00            ;
            lda     Rbuff,x         ; get block # from buffer
            cmp     blkno           ; compare to expected block #        
            beq     GoodBlk1        ; matched!
            jsr     Print_Err       ; Unexpected block number - abort        
            jsr     Flush           ; mismatched - flush buffer and then do BRK
;           lda     #$FD            ; put error code in "A" if desired
;           brk                     ; unexpected block # - fatal error - BRK or RTS
; cj xmodem_error
			lda 	#$FD
			rts
GoodBlk1:   eor     #$ff            ; 1's comp of block #
            inx                     ;
            cmp     Rbuff,x         ; compare with expected 1's comp of block #
            beq     GoodBlk2        ; matched!
            jsr     Print_Err       ; Unexpected block number - abort        
            jsr     Flush           ; mismatched - flush buffer and then do BRK
;                lda        #$FC    ; put error code in "A" if desired
;                brk                ; bad 1's comp of block#        
; cj xmodem error
			lda 	#$FC
			rts
GoodBlk2:   ldy     #$02            ; 
CalcCrc:    lda     Rbuff,y         ; calculate the CRC for the 128 bytes of data        
            jsr     UpdCrc          ; could inline sub here for speed
            iny                     ;
            cpy     #$82            ; 128 bytes
            bne     CalcCrc         ;
            lda     Rbuff,y         ; get hi CRC from buffer
            cmp     crch            ; compare to calculated hi CRC
            bne     BadCrc          ; bad crc, send NAK
            iny                     ;
            lda     Rbuff,y         ; get lo CRC from buffer
            cmp     crc             ; compare to calculated lo CRC
            beq     GoodCrc         ; good CRC
BadCrc:     jsr     Flush           ; flush the input port
            lda     #NAK            ;
            jsr     Put_Chr         ; send NAK to resend block
            jmp     StartBlk        ; start over, get the block again                        
GoodCrc:    ldx     #$02            ;
            lda     blkno           ; get the block number
            cmp     #$01            ; 1st block?
            bne     CopyBlk         ; no, copy all 128 bytes
            lda     bflag           ; is it really block 1, not block 257, 513 etc.
            beq     CopyBlk         ; no, copy all 128 bytes
			; CJ - if we have a start address, use that. otherwise
			; use the first two bytes of data as the start address.
			; how we use this determines how the input data must be set up.
			; So, uh, be advised.
			lda		program_address_low	; do we have an initial address?
			beq		GetStartPtr		; no, so read from first bytes of data
			lda		program_address_high ; do we have an initial address?
			beq		GetStartPtr		; no, so read from first bytes of data
			sta 	ptr+1
			lda 	program_address_low
			sta		ptr
			dec 	bflag			; set the flag so we won't get another address
			jmp 	CopyBlk			; copy all 128 bytes
			
GetStartPtr:lda     Rbuff,x         ; get target address from 1st 2 bytes of blk 1
            sta     ptr             ; save lo address
            sta     program_address_low
            inx                     ;
            lda     Rbuff,x         ; get hi address
            sta     ptr+1           ; save it
            sta     program_address_high
            inx                     ; point to first byte of data
            dec     bflag           ; set the flag so we won't get another address                
CopyBlk:    ldy     #$00            ; set offset to zero
CopyBlk3:   lda     Rbuff,x         ; get data byte from buffer
            sta     (ptr),y         ; save to target
            inc     ptr             ; point to next address
            bne     CopyBlk4        ; did it step over page boundary?
            inc     ptr+1           ; adjust high address for page crossing
CopyBlk4:   inx                     ; point to next data byte
            cpx     #$82            ; is it the last byte
            bne     CopyBlk3        ; no, get the next one
IncBlk:     inc     blkno           ; done.  Inc the block #
            lda     #ACK            ; send ACK
            jsr     Put_Chr         ;
            jmp     StartBlk        ; get next block
Done:       lda     #ACK            ; last block, send ACK and exit.
            jsr     Put_Chr         ;
            jsr     Flush           ; get leftover characters, if any
            jsr     Print_Good      ;
            rts                     ;
;
;^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
;
; subroutines
;
;                                      ;
GetByte:    lda        #$00            ; wait for chr input and cycle timing loop
            sta        retry           ; set low value of timing loop
StartCrcLp: jsr        Get_Chr         ; get chr from serial port, don't wait 
            bcs        GetByte1        ; got one, so exit
            dec        retry           ; no character received, so dec counter
            bne        StartCrcLp      ;
            dec        retry2          ; dec hi byte of counter
            bne        StartCrcLp      ; look for character again
            clc                        ; if loop times out, CLC, else SEC and return
GetByte1:   rts                        ; with character in "A"
;
Flush:      lda        #$70                ; flush receive buffer
            sta        retry2                ; flush until empty for ~1 sec.
Flush1:     jsr        GetByte                ; read the port
            bcs        Flush                ; if chr recvd, wait for another
            rts                        ; else done
;
PrintMsg:   ldx        #$00                ; PRINT starting message
PrtMsg1:    lda        Msg,x                
            beq        PrtMsg2                        
            jsr        Put_Chr
            inx
            bne        PrtMsg1
PrtMsg2:    rts
Msg:        .byte        "Begin XMODEM/CRC transfer.  Press <Esc> to abort..."
            .byte          CR, LF
            .byte   0
;
Print_Err:  ldx        #$00                ; PRINT Error message
PrtErr1:    lda           ErrMsg,x
            beq        PrtErr2
            jsr        Put_Chr
            inx
            bne        PrtErr1
PrtErr2:    rts
ErrMsg:     .byte         "Upload Error!"
            .byte          CR, LF
            .byte   0
;
Print_Good: ldx        #$00                ; PRINT Good Transfer message
Prtgood1:   lda        GoodMsg,x
            beq        Prtgood2
            jsr        Put_Chr
            inx
            bne        Prtgood1
Prtgood2:   rts
GoodMsg:    .byte         "Upload Successful!"
            .byte          CR, LF
            .byte   0
;
;
;======================================================================
;  I/O Device Specific Routines
;
;  Two routines are used to communicate with the I/O device.
;
; "Get_Chr" routine will scan the input port for a character.  It will
; return without waiting with the Carry flag CLEAR if no character is
; present or return with the Carry flag SET and the character in the "A"
; register if one was present.
;
; "Put_Chr" routine will write one byte to the output port.  Its alright
; if this routine waits for the port to be ready.  its assumed that the 
; character was send upon return from this routine.
;
; input chr from ACIA (no waiting)
;
Get_Chr = readchar
;
; output to OutPut Port
;
Put_Chr = _sendchar

;=========================================================================
;
;
;  CRC subroutines 
;
;										
UpdCrc:     eor         crc+1       ; Quick CRC computation with lookup tables
            tax                     ; updates the two bytes at crc & crc+1
            lda         crc         ; with the byte send in the "A" register
            eor         CRCHI,X
            sta         crc+1
            lda         CRCLO,X
            sta         crc
            rts

; The following tables are used to calculate the CRC for the 128 bytes
; in the xmodem data blocks.  You can use these tables if you plan to 
; store this program in ROM.  If you choose to build them at run-time, 
; then just delete them and define the two labels: crclo & crchi.
;
; low byte CRC lookup table (should be page aligned)
.align 256
CRCLO:
.byte $00,$21,$42,$63,$84,$A5,$C6,$E7,$08,$29,$4A,$6B,$8C,$AD,$CE,$EF
.byte $31,$10,$73,$52,$B5,$94,$F7,$D6,$39,$18,$7B,$5A,$BD,$9C,$FF,$DE
.byte $62,$43,$20,$01,$E6,$C7,$A4,$85,$6A,$4B,$28,$09,$EE,$CF,$AC,$8D
.byte $53,$72,$11,$30,$D7,$F6,$95,$B4,$5B,$7A,$19,$38,$DF,$FE,$9D,$BC
.byte $C4,$E5,$86,$A7,$40,$61,$02,$23,$CC,$ED,$8E,$AF,$48,$69,$0A,$2B
.byte $F5,$D4,$B7,$96,$71,$50,$33,$12,$FD,$DC,$BF,$9E,$79,$58,$3B,$1A
.byte $A6,$87,$E4,$C5,$22,$03,$60,$41,$AE,$8F,$EC,$CD,$2A,$0B,$68,$49
.byte $97,$B6,$D5,$F4,$13,$32,$51,$70,$9F,$BE,$DD,$FC,$1B,$3A,$59,$78
.byte $88,$A9,$CA,$EB,$0C,$2D,$4E,$6F,$80,$A1,$C2,$E3,$04,$25,$46,$67
.byte $B9,$98,$FB,$DA,$3D,$1C,$7F,$5E,$B1,$90,$F3,$D2,$35,$14,$77,$56
.byte $EA,$CB,$A8,$89,$6E,$4F,$2C,$0D,$E2,$C3,$A0,$81,$66,$47,$24,$05
.byte $DB,$FA,$99,$B8,$5F,$7E,$1D,$3C,$D3,$F2,$91,$B0,$57,$76,$15,$34
.byte $4C,$6D,$0E,$2F,$C8,$E9,$8A,$AB,$44,$65,$06,$27,$C0,$E1,$82,$A3
.byte $7D,$5C,$3F,$1E,$F9,$D8,$BB,$9A,$75,$54,$37,$16,$F1,$D0,$B3,$92
.byte $2E,$0F,$6C,$4D,$AA,$8B,$E8,$C9,$26,$07,$64,$45,$A2,$83,$E0,$C1
.byte $1F,$3E,$5D,$7C,$9B,$BA,$D9,$F8,$17,$36,$55,$74,$93,$B2,$D1,$F0 

; hi byte CRC lookup table (should be page aligned)
CRCHI:
.byte $00,$10,$20,$30,$40,$50,$60,$70,$81,$91,$A1,$B1,$C1,$D1,$E1,$F1
.byte $12,$02,$32,$22,$52,$42,$72,$62,$93,$83,$B3,$A3,$D3,$C3,$F3,$E3
.byte $24,$34,$04,$14,$64,$74,$44,$54,$A5,$B5,$85,$95,$E5,$F5,$C5,$D5
.byte $36,$26,$16,$06,$76,$66,$56,$46,$B7,$A7,$97,$87,$F7,$E7,$D7,$C7
.byte $48,$58,$68,$78,$08,$18,$28,$38,$C9,$D9,$E9,$F9,$89,$99,$A9,$B9
.byte $5A,$4A,$7A,$6A,$1A,$0A,$3A,$2A,$DB,$CB,$FB,$EB,$9B,$8B,$BB,$AB
.byte $6C,$7C,$4C,$5C,$2C,$3C,$0C,$1C,$ED,$FD,$CD,$DD,$AD,$BD,$8D,$9D
.byte $7E,$6E,$5E,$4E,$3E,$2E,$1E,$0E,$FF,$EF,$DF,$CF,$BF,$AF,$9F,$8F
.byte $91,$81,$B1,$A1,$D1,$C1,$F1,$E1,$10,$00,$30,$20,$50,$40,$70,$60
.byte $83,$93,$A3,$B3,$C3,$D3,$E3,$F3,$02,$12,$22,$32,$42,$52,$62,$72
.byte $B5,$A5,$95,$85,$F5,$E5,$D5,$C5,$34,$24,$14,$04,$74,$64,$54,$44
.byte $A7,$B7,$87,$97,$E7,$F7,$C7,$D7,$26,$36,$06,$16,$66,$76,$46,$56
.byte $D9,$C9,$F9,$E9,$99,$89,$B9,$A9,$58,$48,$78,$68,$18,$08,$38,$28
.byte $CB,$DB,$EB,$FB,$8B,$9B,$AB,$BB,$4A,$5A,$6A,$7A,$0A,$1A,$2A,$3A
.byte $FD,$ED,$DD,$CD,$BD,$AD,$9D,$8D,$7C,$6C,$5C,$4C,$3C,$2C,$1C,$0C
.byte $EF,$FF,$CF,$DF,$AF,$BF,$8F,$9F,$6E,$7E,$4E,$5E,$2E,$3E,$0E,$1E 
.align 1
;
;
; End of File
;


