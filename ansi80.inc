vga80:
        LDA    #<wrch80
        STA     WRCVEC
        LDA    #>wrch80
        STA     WRCVEC+1
        LDA    #<rdch80
        STA     RDCVEC
        LDA    #>rdch80
        STA     RDCVEC+1

        LDA     #0
        STA     quelen
        LDA     #$80
        STA     ModeExtension
        LDA     #$BA
        STA     VgaCtrl
        LDA     #DefaultAttrib
        STA     attrib
vga80l1:
        LDA     #12
        JMP     $FFF4


;    Send ASCII Character to Screen subroutine
;    -----------------------------------------
;
;  - Prints non-control codes (#20 to #FF) at the current cursor position on
;    the screen.
;  - Executes the following control codes:
;
;    <NUL><ACK><BEL><BS><HT><LF><VT><FF><CR><SO><SI><NAK><ESC>
;      0    6    7   8   9   #A  #B  #C  #D  #E  #F  #15  #1B

        ; TEST FOR CONTROL CODES
LFCEA:          ldx     quelen          ; check if character must be stored in the queue (quelen > 0?)
                beq     notque          ; no, continue with printing
                cmp     #'['            ; test for bracket i.e. start of sequence
                bne     q1              ; no, continue 
                cpx     #1              ; is this the first character after the <ESC>
                beq     qstore          ; yes, store it also in the queue
                bne     qproc           ; else consider this as end-of-sequence
                
; TODO: stop queing if the second character of the sequence is not [
; At the moment it does not matter whether the [ is send or not. But it matters
; when executing the sequence.

                ; Now we are storing data into the queue. Store only digits and semi-colons
q1:             cmp     #';'            ; is it a semi-colon?
                beq     qstore          ; yes, store it
                cmp     #$30            ; compare with digit 0
                bmi     qproc           ; it's not a digit, start processing the queue
                cmp     #$3A            ; compare with digit 9
                bpl     qproc           ; it's not a digit, start processing the queue
qstore:         ldx     quelen          ; load queue pointer
                sta     queue,x         ; store character in queue
                inc     quelen          ; increment queue pointer
                bmi     qproc           ; limit of 127 characters reached, start processing the queue
                rts                     ; end of write routine (for now)

notque:         cmp     #$1B            ; is it escape
                bne     t06             ; no, continue with original print routine
                sta     queue           ; store in queue
                ldx     #1              ; initialize the queue
                stx     quelen          ; 
                rts                     ; end of print routine

; Start processing the queue
; On entrance of this routine:
; A - holds the last receive character of the sequence (the "command" letter)
; quelen holds the number of charachters in the queue WITHOUT the last character (as this is not in the queue)
; X and Y are undefined
qproc:          jsr     qdecode         ; read the parameters and store them as digits in the queue and Y has the number of bytes in the queue
                sty     quelen          ; save the queue length
                jsr     exequete        ; execute the code
                lda     #0              ; reset queue length
                sta     quelen
                rts                     ; end of routine

t06:            cmp     #$06            ; Is it <ACK> ?
                beq     LFD0B           ; ..yes, reset the 6847 VDG to alphanumeric
                                        ; mode and clear the NAK flag
                cmp     #$15            ; Is it <NAK> ?
                beq     LFD11           ; ..yes, set the NAK flag
                ldy     $E0             ; Get cursor postion - is the NAK flag bit 7 set ?
                bmi     LFD19           ; ..yes, printing not allowed - return
                cmp     #$1B            ; Is it <ESC> ?
                beq     LFD0B           ; ..yes, reset VDG to alphanumeric mode and clear NAK
                cmp     #$07            ; Is it <BEL> ?
                beq     LFD1A           ; ..yes, sound a bleep
                jsr     LFD44           ; Invert char at current cursor position
                ldx     #$0A            ; Point to the control code table at #FED5
                jsr     LFEC5           ; Test character for executable control code
                bne     LFD29           ; ..it's not an executable control code
                                        ; so print it if >#1F, otherwise return
                jmp     LFEB7           ; ..executable control code - get the code's
                                        ; execution address and jump to it


;    Handle <ESC> subroutine
;    -----------------------
;
;  - Resets the 6847 VDG to alphanumeric mode.
;  - Clears the NAK flag (bit 7 of #E0).


LFD0B:  jmp $fd0b

;    Handle <ACK> or <NAK> subroutine
;    --------------------------------
;
;  - Entry: Carry clear to perform <NAK>
;           Carry  set  to perform <ACK>
;  - Returns with Accumulator and Y registers preserved, and with X=2.
;
LFD11:  jmp     $fd11

LFD19:  rts

;    Handle <BEL> subroutine
;    -----------------------
;
;  - Returns with X=0, Y=128, and the sign flag set.

LFD1A:  jmp     $fd1a

;    Print an ASCII Character on the Screen subroutine
;    -------------------------------------------------
;
;  - Control characters (codes less than #20) are ignored.
;  - Increments current cursor position, incrementing the print line and/or
;    scrolling the screen as necessary.
;  - Entry: Accumulator contains ASCII code of character to be printed
;           Y register contains current cursor position ?#E0.
;  - Accumulator preserved.

LFD29:  cmp     #$20            ; Is the character a control code ?
        bcc     LFD44           ; ..yes, so don't print it

;        adc     #$1F           ; )
;        bmi     LFD33          ; )
;        eor     #$60           ; ) Convert to screen character

         cmp     #$40
         bcc     LFD33
         sbc     #$20
         and     #$5f

LFD33:  sta     ($DE),y         ; Store character at current print position

        ; set character attribute
        pha
        clc
        lda     $de             ; add 3200 to character positions
        pha
        adc     #<3200
        sta     $de
        lda     $df
        pha
        adc     #>3200
        sta     $df
        lda     attrib
        sta     ($de),y
        pla     
        sta     $df
        pla     
        sta     $de
        pla


LFD38:  iny                     ; Increment cursor position
        cpy     #NUMCOLS        ; Reached end of the current print line ?
        bcc     LFD42           ; ..no, update cursor position and invert
                                ; the cursor at this position
        jsr     LFDEC           ; ..yes, do <CR><LF> first

;    Reset Cursor to Start of Current Line Without Deletion subroutine
;    -----------------------------------------------------------------

LFD40:  ldy     #$00            ; Point to start of current line
LFD42:  sty     $E0             ; Update current cursor position register

;    Invert Character at Current Cursor Position subroutine
;    ------------------------------------------------------
;
;  - EORs the character at the current cursor position with the cursor mask
;    ?#E1.
;  - A, X, Y registers preserved.

cursor_on:
LFD44:
        pha                     ; Save character in accumulator
        lda ($DE),Y             ; Get character at current print position
        eor $E1                 ; Mask it
        sta ($DE),Y             ; ..and return it to the screen
        pla                     ; Restore character to accumulator
        rts

cursor_off:
        pha                     ; Save character in accumulator
        lda ($DE),Y             ; Get character at current print position
        and #$7F                ; Mask it
        sta ($DE),Y             ; ..and return it to the screen
        pla                     ; Restore character to accumulator
        rts

;    Handle <DEL> subroutine
;    -----------------------

LFD50:  jsr     LFE35           ; Move cursor back one position if possible, otherwise
                                ; invert character at current cursor position and return
        lda     #$20            ; Get <SPC>
        sta     ($DE),y         ; Blank character at previous cursor pos'n
        bpl     LFD42           ; Update cursor position and invert cursor

;    Handle <BS> subroutine
;    ----------------------
;
;  - Enter with Y containing the current cursor position ?#E1.

LFD5C:  jsr     LFE35           ; Move cursor back one position if possible, otherwise
                                ; invert character at current cursor position and return
        jmp     LFD42           ; Update cursor position and invert cursor

;    Handle <LF> subroutine
;    ----------------------

LFD62:  jsr     LFDEC           ; Do <LF>, scrolling if necessary

        lda     VgaCtrl        ; disable the cursor
        and     #$bf
        sta     VgaCtrl

LFD65:  ldy     $E0             ; Get origional cursor position, which has not changed
                                ; although the line start address may have
        bpl     LFD42           ; Update cursor position and invert cursor

;    Handle <FF> subroutine
;    ----------------------
;
;  - Resets the 8647 VDG to the alphanumeric mode and clears the screen.
;  - Sets the cursor to the top left position.

LFD69:  ldy     #$80            ;
        sty     $E1             ; Set the cursor mask to default
        ldy     #$00            ; Clear screen memory index
        sty     $B000           ; Set 6847 VDG to alphanumeric mode
        lda     #$80            ; set Godil to VGA80
        sta     ModeExtension
        lda     #$20            ; Get <SPC>
LFD74:  jsr     CLEARMORE
        iny                     ; Point to the next byte
        bne     LFD74           ; ..and clear both complete pages

;    Handle <RS> subroutine
;    ----------------------
;
;  - Sets cursor to top left position.

LFD7D:  jmp     $fd7d

;    Handle <VT> subroutine
;    ----------------------
;
;  - Enter with Y containing the current cursor position ?#E1.

LFD87:  jsr     LFE3A           ; Move the cursor position up a line
        jmp     LFD42           ; Update cursor position and invert cursor

;    Handle <SO> subroutine
;    ----------------------
;
;  - Turns page mode on, and sets the number of lines left to 16.

LFD8D:  clc                     ;
        lda     #NUMROWS        ; Get number of lines in page = 16
        sta     $E6             ; Indicate page mode by setting count

;    Handle <SI> subroutine
;    ----------------------
;
;  - Turns page mode off.
;  - Enter with Carry set.

LFD92:  jmp     $fd92

;    Handle Cursor Keys from Keyboard subroutine
;    -------------------------------------------
;
;  - Sends the cursor control code to screen and then fetches another key.

LFDA2:  tax

        bit     VgaCtrl        ; test hardware cursor
        bvs     cursor_enabled

        lda     $e0
        sta     CursorCol
        lda     #$ff
        sta     CursorRow
        lda     $de             ; use de/df as tmp workspace
        pha
        lda     $df
        pha
address_loop:
        inc     CursorRow
        lda     $de
        sec
        sbc     #80
        sta     $de
        lda     $df
        sbc     #0
        sta     $df
        bmi     address_loop
        pla                     ; restore de/df
        sta     $df
        pla
        sta     $de

        lda     VgaCtrl        ; enable the cursor
        ora     #$40
        sta     VgaCtrl

cursor_enabled:


        lda     #>(LFE9A-1)     ; stack ..and fetch another key
        pha
        lda     #<(LFE9A-1)
        pha

        txa
        and     #$05            ;
        rol     $B001           ;
        rol     a               ;

        cmp     #$08            ; cursor left
        beq     cursor_l
        cmp     #$09            ; cursor right
        beq     cursor_r
        cmp     #$0A            ; cursor down
        beq     cursor_d
        cmp     #$0B            ; cursor up
        beq     cursor_u
                                ; should never get here....
        rts

cursor_l:
        ldy     CursorCol
        dey
        bmi     cursor_l_wrap
        sty     CursorCol
        rts
cursor_l_wrap:
        ldy     #79
        sty     CursorCol

cursor_u:
        ldy     CursorRow
        dey
        bpl     cursor_u_nowrap
        ldy     #39
cursor_u_nowrap:
        sty     CursorRow
        rts

cursor_r:
        ldy     CursorCol
        iny
        cpy     #80
        bcs     cursor_r_wrap
        sty     CursorCol
        rts
cursor_r_wrap:
        ldy     #0
        sty     CursorCol

cursor_d:
        ldy     CursorRow
        iny
        cpy     #40
        bcc     cursor_d_nowrap
        ldy     #0
cursor_d_nowrap:
        sty     CursorRow
        rts

copy:
        lda     $de
        pha
        lda     $df
        pha
        lda     CursorCol
        sta     $de
        lda     #$80
        sta     $df
        ldy     CursorRow
copy_loop:
        dey
        bmi     copy_grab
        clc
        lda     $de
        adc     #80
        sta     $de
        bcc     copy_loop
        inc     $df
        bne     copy_loop
copy_grab:
        iny
        lda     ($de), Y
        ;                 ADC#$20
        ; Screen 00-1F -> 20-3F -> ASCII 40-5F
        ; Screen 20-3F -> 40-5F -> ASCII 20-3F
        ; Screen 40-5F -> 60-7F -> ASCII 60-7F
        clc
        adc     #$20
        cmp     #$60
        bcs     copy_done
        eor     #$60
copy_done:
        tax                    ; remember the ascii value
        jsr     cursor_r       ; copy also moves the cursor right
        pla
        sta     $df
        pla
        sta     $de
        txa                    ; get the ascii value back again
        jmp     $FDE9          ; Restore A,X,Y regs & status & return

;    Handle <LF>, Scrolling if Necessary subroutine
;    ----------------------------------------------
;
;  - If in page mode, decrements page counter, and at the end of the page
;    waits for a keypress before scrolling.

LFDEC:  lda     $DE             ; Get LSB start of line
        ldy     $DF             ; Get MSB start of line
        cpy     #>SCREENEND     ; In lower screen page ?
        bcc     LFE2C           ; ..no, do <LF> - scrolling not required
        cmp     #<SCREENEND     ; In last page..but is it the last line ?
        bcc     LFE2C           ; ..no, do <LF> - scrolling not required

        ; SCROLLING REQUIRED - CHECK IN PAGE MODE

        ldy     $E6             ; Get page mode flag
        bmi     LFE08           ; ..not in page mode - scroll the screen
        dey                     ;
        bne     LFE06           ;

        ;  IN PAGE MODE - GET KEYPRESS

LFDFF:  jsr     LFE71           ; Scan keyboard
        bcs     LFDFF           ; ..keep scanning until key pressed
        ldy     #NUMROWS        ;
LFE06:  sty     $E6             ; Reset page counter to 16 lines

;    Scroll the Screen subroutine
;    ----------------------------

LFE08:  ldy     #NUMCOLS        ; Shift screen up 32 characters = 1 line

;    Scroll Y lines of the Screen subroutine
;    ---------------------------------------
;
;  - For every #20 in Y a top line of the screen is not scrolled.

LFE0D:  lda     SCREEN,y        ; Get byte from upper text page
        sta     SCREEN-NUMCOLS,y                ; ..and store it a line higher
        lda     SCREEN+$C80,y                   ; scroll attribute
        sta     SCREEN+$C80-NUMCOLS,y
        iny                     ; Point to next screen byte
        bne     LFE0D           ; ..and shift up all the upper text page
        JSR     SCROLLMORE

;    Delete Current Line subroutine
;    ------------------------------
;
;  - CLears the 32 character line based at (#DE) to black (<SPACE>).

        ldy     #NUMCOLS-1      ; Set character pointer to end of line
        lda     #$20            ; Get <SPACE>
LFE26:  sta     ($DE),y         ; Clear the character to black
        dey                     ; Point to the next character
        bpl     LFE26           ; ..and clear the entire print line
        
        ldy     #NUMCOLS-1      ; Set character pointer to end of line
        lda     attrib          ; Get attribute
LFE26a: sta     $98B0,y         ; Set the current attribute
        dey                     ; Point to the next character
        bpl     LFE26a          ; ..and clear the entire print line        
        rts                     ;

;    Add One Line to the Cursor Position subroutine
;    ----------------------------------------------
;
;  - Enter with the accumulator containing the LSB current cursor
;    Delete Current Line subroutine
;    ------------------------------
;
;  - CLears the 32 character line based at (#DE) to black (<SPACE>).
;  address
;    #DE and Carry clear.

LFE2C:  adc     #NUMCOLS        ; Add 32 characters = 1 print line
        sta     $DE             ; ..and update LSB cursor  Add 32 characters = 1 print lineaddress
        bcc     LFE34           ;
        inc     $DF             ; Increment MSB cursor address if overflow
LFE34:  rts                     ;

;    Move the Cursor Back One Position subroutine
;    --------------------------------------------
;
;  - Decrements the current cursor position, dealing with line underflow.
;  - If the cursor is at the top left of the screen, the character at this
;    position is inverted before premature return.
;  - Used by the <BS> and <DEL> subroutines.
;  - Enter with Y register holding the current cursor position ?#31.

LFE35:  dey                     ; Point to the previous cursor position
        bpl     LFE51           ; ..still on current line, return

        ; DEAL WITH LINE UNDERFLOW

        ldy     #NUMCOLS-1      ; Set cursor position to last char on line
LFE3A:  lda     $DE             ; Get LSB current line address
        bne     LFE49           ; ..not at top of screen, so can move line
                                ; address up a line
        ldx     $DF             ; Get MSB current line address
        cpx     #>SCREEN        ; Is it upper page ?
        bne     LFE49           ; ..no, move line address up a line

        ; ALREADY AT TOP OF SCREEN - RETURN

        pla                     ; )
        pla                     ; ) Remove return address from stack
        jmp     LFD65           ; Invert char at current cursor position

        ; MOVE CURRENT START ADDRESS UP A LINE

LFE49:  sbc     #NUMCOLS        ; Move LSB current line back 32 characters
        sta     $DE             ; ..and update LSB line addres
        bcs     LFE51           ;
        dec     $DF             ; Decrement MSB line address if overflow
LFE51:  rts                     ;

;    Send Character to VIA and Screen subroutine
;    -------------------------------------------
;
;  - Preserves all registers.

wrch80: 
        ; jsr     LFEFB           ; Send character in accumulator to the VIA, disabled, see http://stardot.org.uk/forums/viewtopic.php?p=135913#p135912
        

;    Send Character to Screen subroutine
;    -----------------------------------
;
;  - Preserves all registers.

        php                     ; Save flags
        pha                     ; Save accumulator
        cld                     ;
        sty     $E5             ; Save Y register
        stx     $E4             ; Save X register
        jsr     LFCEA           ; Send character in accumulator to screen
        pla                     ; Restore accumulator
LFE60:  ldx     $E4             ; Restore X register
        ldy     $E5             ; Restore Y register
        plp                     ; Restore flags
        rts                     ;


;    OSRDCH Get Key subroutine
;    -------------------------
;
;  - Waits for a key to be pressed and returns with its ASCII value in the
;    accumulator.
;  - Executes control characters before return.
;  - If <LOCK> or cursor control keys is pressed, the code is executed
;    and another keypress fetched before return.
;  - Preserves X,Y registers and flags.

rdch80: php                     ; Save flags
        cld                     ;
        stx     $E4             ; Save X register
        sty     $E5             ; Save Y register

        ; WAIT FOR KEYBOARD TO BE RELEASED

LFE9A:  bit     $B002           ; Is <REPT> key pressed ?
        bvc     LFEA4           ; ..yes, no need to wait for keyboard to be released
        jsr     LFE71           ; Scan keyboard
        bcc     LFE9A           ; ..wait for key to be released

        ; GET KEYPRESS

LFEA4:  jsr     LFB8A           ; Wait 0.1 second for debounce
LFEA7:  jsr     LFE71           ; Scan keyboard
        bcs     LFEA7           ; ..keep scanning until key pressed
        jsr     LFE71           ; Scan keyboard again - still pressed ?
        bcs     LFEA7           ; ..no, noise ? - try again
        tya                     ; Acc = ASCII value of key - #20
        ldx     #$17            ; Pointer to control code table at #FEE2

        ; GET EXECUTION ADDRESS AND JUMP TO IT

        jsr     LFEC5           ; Test for control code or otherwise
LFEB7:  lda     tablelo, x      ; Get LSB execution  Test for control code or otherwiseaddress
        sta     $E2             ; ..into w/s
        lda     tablehi, x      ; Get MSB execution  ..into w/saddress
        sta     $E3             ; ..into w/s
        tya                     ; Acc = ASCII value of key - #20
        jmp     ($E2)           ; Jump to deal with char or control code

;    Decode Control Character subroutine
;    -----------------------------------
;
;  - Enter at #FEC5.
;  - Enter with X pointing to control code table:
;      X=#A  for the WRCHAR table at #FED5
;      X=#17 for the RDCHAR table at #FEE2.
;  - Returns with Carry set, and X pointing to matched code or last code.
;  - Returns with Z flag set if control code matched.

LFEC4:  dex                     ; Point to next control code in table
LFEC5:  cmp     LFECB, x        ; Is it this control code ?
        bcc     LFEC4           ; ..no, table value too large - try the next code
        rts                     ;

LKEY0:  php
        bit     $b001
        bmi     LKEY1
        lda     #63
LKEY1:  plp
        jmp     LFDDF



;    WRCHAR Control Code Data Lookup Table
;    -------------------------------------

LFECB:  .byte $00, $08, $09, $0A, $0B, $0C, $0D, $0E,$0F, $1E, $7F

;    RDCHAR Control Code Data Lookup Table
;    -------------------------------------

        .byte $00, $01, $05, $06, $08, $0E, $0F, $10, $11, $1C, $20, $21, $3B

;    WRCHAR Control Code Address Lookup Table
;    Note that this is just the LSB.
;    ----------------------------------------

tablelo:
        .byte <LFD44            ; invert char at cursor position
        .byte <LFD5C            ; handle <BS>
        .byte <LFD38            ; handle <HT>
        .byte <LFD62            ; handle <LF>
        .byte <LFD87            ; handle <VT>
        .byte <LFD69            ; handle <FF>
        .byte <LFD40            ; handle <CR>
        .byte <LFD8D            ; handle <SO>
        .byte <LFD92            ; handle <SI>
        .byte <LFD7D            ; handle <RS>
        .byte <LFD50            ; handle <DEL>

;    RDCHAR Control Code Address Lookup Table
;    Note that this is just the LSB.
;    ----------------------------------------

        .byte <LFDDF            ;
        .byte <LFDD2            ;
        .byte <LFD9A            ; handle LOCK
        .byte <LFDA2            ; handle cursor keys
        .byte <LFDE2            ;
        .byte <copy             ; handle COPY
        .byte <LFDC0            ; handle DEL
        .byte <LKEY0            ; handle key 0 (was LFDDF)
        .byte <LFDD8            ;
        .byte <LFDD6            ;
        .byte <LFDC8            ;
        .byte <LFDC6            ;
        .byte <LFDC2            ;

;    WRCHAR Control Code Address Lookup Table
;    Note that this is just the MSB.
;    ----------------------------------------

tablehi:
        .byte >LFD44            ; invert char at cursor position
        .byte >LFD5C            ; handle <BS>
        .byte >LFD38            ; handle <HT>
        .byte >LFD62            ; handle <LF>
        .byte >LFD87            ; handle <VT>
        .byte >LFD69            ; handle <FF>
        .byte >LFD40            ; handle <CR>
        .byte >LFD8D            ; handle <SO>
        .byte >LFD92            ; handle <SI>
        .byte >LFD7D            ; handle <RS>
        .byte >LFD50            ; handle <DEL>

;    RDCHAR Control Code Address Lookup Table
;    Note that this is just the MSB.
;    ----------------------------------------

        .byte >LFDDF            ;
        .byte >LFDD2            ;
        .byte >LFD9A            ; handle LOCK
        .byte >LFDA2            ; handle cursor keys
        .byte >LFDE2            ;
        .byte >copy             ; handle COPY
        .byte >LFDC0            ; handle DEL
        .byte >LKEY0            ; handle key 0 (was LFDDF)
        .byte >LFDD8            ;
        .byte >LFDD6            ;
        .byte >LFDC8            ;
        .byte >LFDC6            ;
        .byte >LFDC2            ;


CLEARMORE:
        sta     SCREEN+$000,y
        sta     SCREEN+$100,y
        sta     SCREEN+$200,y
        sta     SCREEN+$300,y
        sta     SCREEN+$400,y
        sta     SCREEN+$500,y
        sta     SCREEN+$600,y
        sta     SCREEN+$700,y
        sta     SCREEN+$800,y
        sta     SCREEN+$900,y
        sta     SCREEN+$a00,y
        sta     SCREEN+$b00,y
        cpy     #$80
        bpl     CLEARATTR
        sta     SCREEN+$c00,y

CLEARATTR:
        lda     attrib          ; set attribute 
        sta     SCREEN+$000+$C80,y
        sta     SCREEN+$100+$C80,y
        sta     SCREEN+$200+$C80,y
        sta     SCREEN+$300+$C80,y
        sta     SCREEN+$400+$C80,y
        sta     SCREEN+$500+$C80,y
        sta     SCREEN+$600+$C80,y
        sta     SCREEN+$700+$C80,y
        sta     SCREEN+$800+$C80,y
        sta     SCREEN+$900+$C80,y
        sta     SCREEN+$a00+$C80,y
        sta     SCREEN+$b00+$C80,y
        cpy     #$80
        bpl     CLEAREND
        sta     SCREEN+$c00+$C80,y
CLEAREND:
        lda     #$20
        rts


SCROLLMORE:

LFDF29:
        LDA     SCREEN+$100,Y
        STA     SCREEN+$100-NUMCOLS,Y
        LDA     SCREEN+$100+$C80,Y
        STA     SCREEN+$100+$C80-NUMCOLS,Y
        INY
        BNE     LFDF29
LFDF2A:
        LDA     SCREEN+$200,Y
        STA     SCREEN+$200-NUMCOLS,Y
        LDA     SCREEN+$200+$C80,Y
        STA     SCREEN+$200+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2A
LFDF2B:
        LDA     SCREEN+$300,Y
        STA     SCREEN+$300-NUMCOLS,Y
        LDA     SCREEN+$300+$C80,Y
        STA     SCREEN+$300+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2B
LFDF2C:
        LDA     SCREEN+$400,Y
        STA     SCREEN+$400-NUMCOLS,Y
        LDA     SCREEN+$400+$C80,Y
        STA     SCREEN+$400+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2C
LFDF2D:
        LDA     SCREEN+$500,Y
        STA     SCREEN+$500-NUMCOLS,Y
        LDA     SCREEN+$500+$C80,Y
        STA     SCREEN+$500+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2D
LFDF2E:
        LDA     SCREEN+$600,Y
        STA     SCREEN+$600-NUMCOLS,Y
        LDA     SCREEN+$600+$C80,Y
        STA     SCREEN+$600+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2E
LFDF2F:
        LDA     SCREEN+$700,Y
        STA     SCREEN+$700-NUMCOLS,Y
        LDA     SCREEN+$700+$C80,Y
        STA     SCREEN+$700+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2F
LFDF2G:
        LDA     SCREEN+$800,Y
        STA     SCREEN+$800-NUMCOLS,Y
        LDA     SCREEN+$800+$C80,Y
        STA     SCREEN+$800+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2G
LFDF2H:
        LDA     SCREEN+$900,Y
        STA     SCREEN+$900-NUMCOLS,Y
        LDA     SCREEN+$900+$C80,Y
        STA     SCREEN+$900+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2H
LFDF2I:
        LDA     SCREEN+$a00,Y
        STA     SCREEN+$a00-NUMCOLS,Y
        LDA     SCREEN+$a00+$C80,Y
        STA     SCREEN+$a00+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2I
LFDF2J:
        LDA     SCREEN+$b00,Y
        STA     SCREEN+$b00-NUMCOLS,Y
        LDA     SCREEN+$b00+$C80,Y
        STA     SCREEN+$b00+$C80-NUMCOLS,Y
        INY
        BNE     LFDF2J
LFDF2K:
        LDA     SCREEN+$c00,Y
        STA     SCREEN+$c00-NUMCOLS,Y
        LDA     SCREEN+$c00+$C80,Y
        STA     SCREEN+$c00+$C80-NUMCOLS,Y
        INY
        BPL     LFDF2K

        RTS

;               Execute the extended VDU codes

; set cursor position to x (decoque+1) , y (decoque)  { x and y do not refer to the registers in this comment!)
exevdu31:       ldy     curXpos         ; load current position (column) on the current line
                jsr     cursor_off      ; cursor off at old position
                lda     #$80            ; cursor position to y
                sta     $df
                lda     #$00
                sta     $de
                ldy     decoque         ; load the new line number
                bne     vdu31l1         ; jump if it's not 0
                iny                     ; Zero is most likely an omitted value and is not allowed, so change it to 1
vdu31l1:        dey                     ; decrement y for the loop
                beq     vdu31l2         ; jump if y=0 (upper row)
                clc
                lda     $de
                adc     #NUMCOLS
                sta     $de
                lda     $df
                adc     #0
                sta     $df
                jmp     vdu31l1
vdu31l2:        ldx     decoque+1       ; cursor position to x
                beq     vdu31l3         ; jump if it is 0, probably an omitted value
                dex                     ; decrement it (ansi starts line with pos 1, Atom starts from 0)
vdu31l3:        stx     curXpos
                ; jsr     lfd44         ; invert char at current cursor position
                rts

exevdu17:       ; set foreground colour, actual setting of attribute should be handled by print routine
                lda     queue+1         ; load the colour
                sta     attrib          ; store in memory
                rts                     ; end


exevdu16:       ; clear text window
                lda     $de             ; save workspace addresses
                pha
                lda     $df
                pha
                lda     $e0
                pha
                lda     $e1
                pha
                lda     #$00            ; calculate start address of topY
                sta     queue
                lda     #$80
                sta     queue+1
                ldy     topY            ; load counter
vdu16l1:        clc
                lda     queue
                adc     #NUMCOLS
                sta     queue
                lda     queue+1
                adc     #0
                sta     queue+1
                dey
                bne     vdu16l1
                ; now queue, queue+1 contains first address of line     
                ; just fill this area with spaces from left to right
                ; for the correct number of lines
                ldx     topY
vdu16l2:        lda     queue
                sta     $9e
                clc
                adc     #<3200
                sta     $a0
                lda     queue+1
                sta     $9f
                adc     #>3200
                sta     $a1
                ldy     leftX
vdu16l3:        lda     #32             ; clear character
                sta     ($9e),y
                lda     attrib          ; set background attrib
                sta     ($a0),y         
                iny
                cpy     rightX
                bcc     vdu16l3         ; branch if smaller
                beq     vdu16l3         ; branch if equal
                clc
                lda     queue           ; increment row
                adc     #NUMCOLS
                sta     queue
                lda     queue+1
                adc     #0
                sta     queue+1
                inx
                cpx     bottomY
                bcc     vdu16l2
                beq     vdu16l2
                pla
                sta     $e1
                pla
                sta     $e0
                pla
                sta     $df
                pla
                sta     $de
                rts


exevdu26:       ; reset text window
                lda     #0
                sta     leftX
                sta     topY
                lda     #79
                sta     rightX
                lda     #39
                sta     bottomY
                rts

exevdu28:       ; set textwindow (VDU 28,leftX,bottomY,rightX,topY)
                ldx     #4
vdu28l1:        lda     queue,x
                sta     topY-1,x
                dex
                bne     vdu28l1
                rts

exevdu22:       ldy     #0              ; reset y register
                lda     queue+1         ; load mode
                sta     $52             ; save graphics mode
                beq     LF6C2           ; clear 0
                cmp		#8				; mode 8 tweak (dumped by Z80 Tube client)
                bne		notmode8
                lda		#7				; if mode = 8 then mode = 7
notmode8:       cmp     #7              ; mode 7 (VGA80)
                bne     LF684
mode7:          jmp     LFD69           ; clear the screen and end
LF684:          cmp     #5              ; test for mode > 4
                bcc     LF68A
                lda     #4              ; force mode 4
LF68A:          ldx     #$80            ; reset x register
                stx     $54             ; set pointer to start of video memory
                sty     $53
                tax                     ; load pointer to end of video memory
                lda     $F6CE,x
                tax
                tya                     ; reset accu
LF6A0:          sta     ($53),y         ; reset video memory
                dey                     ; decrement index
                bne     LF6A0
                inc     $54             ; increment hi byte video pointer
                cpx     $54             ; test for end of memory
                bne     LF6A0
LF6AB:          ldy     $52             ; load video mode
                lda     $F6D8,y         ; set plot vector
                sta     $03FF
                lda     $F6D3,y
                sta     $03FE
                lda     $F6DD,y         ; set video controller
                sta     $B000
                lda     #$00            ; set Godil
                sta     ModeExtension
                rts
LF6C2:          lda     #$40            ; clear video memory for mode 0
LF6C4:          sta     $8000,y
                sta     $8100,y
                dey
                bne     LF6C4
                beq     LF6AB


exevdu18:       lda     decoque         ; load mode (I use it for palette)
                bne     palette1        ; jmp if non-zero (i.e. palette 1)
                lda     $B002           ; load current video controller setting
                and     #$F7            ; reset bit 4
                jmp     palette2        ; continue
palette1:       lda     $B002           ; same procedure to set bit 4
                ora     #$08
palette2:       sta     $B002           ; write to video controller setting
LDF05:          lda     queue+1         ; load colour number
                and     #$03            ; use only lower two bits
                tay                     ; transfer to Y reg
                lda     $DF4E,y         ; read colour byte pixel mask
                sta     $3FD            ; store in workspace
                lda     $B000           ; load video mode setting
                and     #$F0            ; mask lower bits
                cmp     #$70            ; test for mode (clear) 1
                bne     LDF25           ; jump if not
                lda     #$00            ; reset accu
                tay                     ; transfer to index
LDF1C:          sta     $8600,y         ; clear additional video memory
                sta     $8700,y
                dey                     ; decrement pointer
                bne     LDF1C           ; jump if not all cleared
LDF25:          lda     $B000           ; reload video mode setting
                and     #$DF            ; change mode to colour
                sta     $B000
                rol     a               ; determine current video mode
                rol     a
                rol     a
                and     #$03            ; only lower two bits are needed
                tay                     ; transfer to index
                lda     $DF42,y         ; read low byte of new plot vector
                sta     $3FE            ; store in workspace
                lda     $DF46,y         ; read high byte of new plot vector
                sta     $3FF            ; store in workspace
                rts                     ; that's it, end of routine

exevdu25:       
                lda     queue+1         ; copy coordinates
                sta     $5D             ; set in zeropage/workspace
                lda     queue+2         ; copy coordinates
                sta     $5C             ; set in zeropage/workspace
                lda     queue+3         ; copy coordinates
                sta     $5B             ; set in zeropage/workspace
                lda     queue+4         ; copy coordinates
                sta     $5A             ; set in zeropage/workspace
                lda     queue+5         ; set plot mode
                sta     $5E
                
                ldx     #3              ; load pointer
LF576:          lda     $03C1,x         ; load previous coordinate
                sta     $52,x           ; copy to zeropage/workspace
                dex
                bpl     LF576
                lda     $5E             ; load plot mode
                and     #4              ; test absolute/relative
                bne     LF597           ; jmp if absolute
                ldx     #2              ; calculate relative coordinates
LF586:          clc
                lda     $5A,x
                adc     $52,x
                sta     $5A,x
                lda     $5B,x
                adc     $53,x
                sta     $5B,x
                dex
                dex
                bpl     LF586
LF597:          ldx     #3              ; load new index
LF599:          lda     $5A,x           ; save new coordinates
                sta     $03C1,x
                dex
                bpl     LF599
                lda     $5E             ; load plot mode
                and     #3              ; test for move mode
                beq     LF5B2           ; if move then done
                sta     $5E             ; save the result of the test
                lda     queue+5         ; load the plot mode
                and     #8              ; test plot mode
                beq     LF5B5           ; jmp if draw mode
                jsr     $F678           ; plot the pixel
LF5B2:          rts                     ; end of routine
        

LF5B5:          ldx     #2              ; load x reg as index
LF5B7:          sec                     ; set carry for subtraction
                lda     $5A,x           ; load low byte of new coordinate
                sbc     $52,x           ; subtract low byte of previous coordinate
                ldy     $52,x           ; load low byte of previous coordinate
                sty     $5A,x           ; set as first plot coordinate
                sta     $52,x           ; set length of plot coordinate
                ldy     $53,x           
                lda     $5B,x           ; same procedure for high byte
                sbc     $53,x
                sty     $5B,x
                sta     $53,x
                sta     $56,x
                bpl     LF5DD           ; jump on positive length of plot
                lda     #0              ; reset accu
                sec                     ; set carry for subtraction
                sbc     $52,x           ; subtract negative value
                sta     $52,x           ; write positive value back
                lda     #0              ; same for high byte
                sbc     $53,x
                sta     $53,x
LF5DD:          dex                     ; decrement index
                dex
                bpl     LF5B7

LF5E1:          lda     $54             ; load low byte y coordinate
                cmp     $52             ; compare with low byte x
                lda     $55             ; load high byte y 
                sbc     $53             ; subtract from high byte x
                bcc     LF61C           ; jump if x is larger

LF5EB:          lda     #0              ; reset accu
                sbc     $54             ; subtract low byte y coordinate
                sta     $57             ; store negative value in pointer
                lda     #0              ; reset accu
                sbc     $55             ; subtract high byte y coordinate
                sec                     ; set carry for division
                ror     a               ; divide by 2
                sta     $59             ; set negative value in pointer
                ror     $57             ; divide low byte of pointer by 2
LF5FB:          jsr     $F678           ; plot a pixel
                lda     $5C             ; load low byte of plotted pixel y coordinate
                cmp     $03C3           ; compare with end value
                bne     LF60F           ; if not equal then continue
                lda     $5D             ; same for high byte
                cmp     $03C4
                bne     LF60F
LF60C:          jmp     LF5B2           ; end of routine
LF60F:          jsr     $F655           ; adjust test pointer and y coordinate
                lda     $59             ; load high byte of test pointer
                bmi     LF5FB           ; if negative, don't change x coordinate
                jsr     $F644           ; adjust test pointer and x coordinate
                jmp     LF5FB           ; plot the coordinate

LF61C:          lda     $53             ; divide x coordinate by 2
                lsr     a
                sta     $59             ; store in pointer
                lda     $52
                ror     a
                sta     $57
LF626:          jsr     $F678           ; plot pixel
                lda     $5A             ; load low byte plotted x coordinate
                cmp     $03C1           ; compare to end value
                bne     LF637
                lda     $5B             ; load high byte plotted x coordinate
                cmp     $03C2
                beq     LF60C           ; end of routine
LF637:          jsr     $F644           ; adjust test pointer x coordinate
                lda     $59             ; load high byte of testbyte
                bpl     LF626           ; if pointer still positive then plot next
                jsr     $F655           ; adjust test pointer y coordinate
                jmp     LF626

vga32:
                LDY     #3
vga32l1:        LDA     $FF9E,Y
                STA     WRCVEC,Y
                DEY
                BPL     vga32l1
                LDA     #$00
                STA     ModeExtension
                STA     VgaCtrl
                JMP     vga80l1

; Read the characters in the queue and convert them to hexadecimal codes; these will be stored in the queue. When this routine is
; called the queue starts with <ESC>[ followed by max 127 characters of the sequence.

; parameters when called:
; A - holds the last receive character of the sequence (the "command" letter)
; quelen holds the number of charachters in the queue WITHOUT the last character (as this is not in the queue) but including the leading <esc> and [
; X and Y are undefined

; On exit the queue has changed:
; first byte (queue) holds the "command" letter
; second byte (queue+1) is undefined (it's used as workspace)
; from the third byte (decoque) the parameters are stored as binary data
; the Y register holds the number of bytes in the decoded queue
 
qdecode:        sta     queue           ; this is the last character of the sequence. Store it as command
                lda     quelen          ; check if parameters in queue
                cmp     #2              ; there are only params if length > 2: first one is 0x1B, second one is 0x5B ([)
                bne     qdec0
                ldy     #0              ; set (dummy) pointer to the start of decoded data
                rts
qdec0:          lda     #0              ; location queue + 1 will be used as workspace for decoding
                sta     queue+1         ; clear this workspace
                ldx     #2              ; load (read) pointer to start of sequence parameters
                ldy     #0              ; load (write) pointer to start of decoded data
qdec1:          lda     queue,x         ; load character
                cmp     #';'            ; is it a semi-colon?
                bne     qdec3           ; no, continue
                ; from here we're decoding a numeric value string into binary values. However, if the escape sequence contains
                ; two semi-colons after each other, there is no numeric string. This will default to a value of 0 which is 
                ; correct according to the specs. 
                ; However, some of the "commands" need a default value of 1 so you have to take care of those exceptions.
qdec2:          lda     queue+1         ; load decoded data
                sta     decoque,y       ; store in queue (yes, we overwrite the original sequence but that's no problem)
                lda     #0              ; clear workspace
                sta     queue+1       
                iny                     ; increment write pointer
                inx                     ; increment read pointer
                cpx     quelen          ; last character read?
                bne     qdec1           ; no, process next character 
                rts                     ; end of decode, return
qdec3:          sec                     ; set carry for substraction
                sbc     #'0'            ; "convert" to binary value of ascii byte
                jsr     mul10           ; add this to the workspace (new binary value goes into queue+1)
                inx                     ; increment read pointer
                cpx     quelen          ; last character read?
                bne     qdec1           ; no, read the next one
                dex                     ; decrement the read pointer
                bne     qdec2           ; store it and end routine

; Multiply the value of the workspace byte by 10 and add the contents of A
mul10:          pha                     ; save A
                lda     queue+1         ; 
                asl                     ; multiply by 2
                sta     temp            ; temp store in TEMP
                asl                     ; again multiply by 2 (*4)
                asl                     ; again multiply by 2 (*8)
                clc
                adc     temp            ; as result, A = x*8 + x*2
                sta     queue+1         ; store
                pla                     ; restore A
                clc
                adc     queue+1         ; add it to calculated byte
                sta     queue+1         ; store
                rts                     ; finished, return

; Execute the command
; At calling this routine:
; queue holds the "command" letter
; decoque (and following) hold the binary decoded parameters
; Y and quelen contain the number of paramaters
; A and X are undefined

exequete:       lda     queue           ; load command byte
                ; I think the next one should be ldx #0 
                ; On second thought ... it seems not to be used, so remove it.
                ;                ldx     queue+1         ; reset index register to start of parameters
                cmp     #'H'            ; check for command cursor position
                bne     exe1
                jmp     cursor_pos
exe1:           cmp     #'f'            ; check for command cursor position
                bne     exe2
                jmp     cursor_pos
exe2:           cmp     #'A'            ; check for command cursor up
                bne     exe3
                jmp     cursor_up
exe3:           cmp     #'B'            ; check for command cursor down
                bne     exe4
                jmp     cursor_down
exe4:           cmp     #'C'            ; check for command cursor forward
                bne     exe5
                jmp     cursor_forw
exe5:           cmp     #'D'            ; check for command cursor backward
                bne     exe6
                jmp     cursor_back
exe6:           cmp     #'s'            ; check for command cursor position save
                bne     exe7
                jmp     cursor_save
exe7:           cmp     #'u'            ; check for command cursor position restore
                bne     exe8
                jmp     cursor_restore
exe8:           cmp     #'K'            ; check for command erase line
                bne     exe9
                jmp     erase_line
exe9:           cmp     #'J'            ; check for clear screen
                bne     exe10     
                jmp     clear_screen
exe10:          cmp     #'m'            ; check for graphics mode
                bne     exe11
                jmp     graphics_mode
exe11:
                rts                     ; 

; Move the cursor to the specified position
cursor_pos:     cpy     #0              ; test if any parameters in the queue
                bne     cursor_pos1     ; no, then jump
                iny                     ; load Y with 1 (shorter and way faster than LDY #1)
                sty     decoque         ; set new position to 1,1 (upper left corner)
                sty     decoque+1
cursor_pos1:    jmp     exevdu31        ; set cursor to new position

; Move the cursor up  
cursor_up:      jsr     cursor_1        ; check if any parameters given, if not then set parameter to 1
                ldy     curXpos
                jsr     cursor_off      ; turn cursor off
                ldy     decoque         ; load number of rows
cursor_up0:     lda     $de             ; check low byte upper row
                bne     cursor_up1      ; not upper row
                lda     $df             ; check high byte of upper row
                cmp     #$80             
                beq     cursor_up2      ; it's upper row, end routine
cursor_up1:     sec                     ; calculate address of previous row
                lda     $de
                sbc     #NUMCOLS
                sta     $de
                lda     $df
                sbc     #0
                sta     $df
                dey                     ; check if more lines to move
                bne     cursor_up0      ; yes, repeat loop
cursor_up2:     rts                     ; end of routine

; Move the cursor down
cursor_down:    jsr     cursor_1        ; check if any parameters given, if not then set parameter to 1
                ldy     curXpos
                jsr     cursor_off      ; turn cursor off
                ldy     decoque         ; load number of rows
cursor_down0:   lda     $de             ; check for low byte bottom row
                cmp     #$30            ; bottom row starts at $8C30
                bne     cursor_down1    ; not bottom row
                lda     $df
                cmp     #$8C
                beq     cursor_down2    ; it's bottom row, eind routine
cursor_down1:   clc                     ; clear carry for addition
                lda     $de             ; calculate address of next row
                adc     #NUMCOLS                
                sta     $de
                lda     $df
                adc     #0
                sta     $df
                dey                     ; check if more lines to move
                bne     cursor_down0    ; yes, repeat loop
cursor_down2:   rts                     ; end of routine


; Move the cursor backward
cursor_back:    jsr     cursor_1        ; check if any parameters given, if not then set parameter to 1
                lda     curXpos         ; load current X position               
                sec                     ; set carry for substraction
                sbc     decoque         ; substract number of lines
                bpl     cursor_back1    ; jump if not before left screen border
                lda     #0              ; otherwise load left column number 
cursor_back1:   ldy     curXpos
                jsr     cursor_off      ; turn cursor off
                sta     curXpos         ; set cursor to new position
                rts

; Move the cursor forward
cursor_forw:    jsr     cursor_1        ; check if any parameters given, if not then set parameter to 1
                lda     curXpos         ; load current X position               
                clc                     ; reset carry for addition
                adc     decoque         ; substract number of lines
                cmp     #80             ; test for right border of screen
                bmi     cursor_forw1    ; jump if not below screen border
                lda     #79             ; otherwise load right column number 
cursor_forw1:   jmp     cursor_back1    ; set cursor to new position

; Check if any parameters given, if none given then default to 1
cursor_1:       cpy     #0              ; load queue length
                bne     cursor_2        ; if not zero jump to end of routine
                lda     #1              ; load default value 1
                sta     decoque
cursor_2:       rts                     ; return to calling routine

; Save cursor position  
cursor_save:    lda     curXpos         ; copy cursor position
                sta     saveXpos
                lda     curYpos
                sta     saveYpos
                lda     curYpos+1
                sta     saveYpos+1
                rts                     ; end of routine

cursor_restore: lda     saveXpos        ; copy old cursor position
                sta     curXpos         ; store in queue
                lda     saveYpos        ; same for Y position
                sta     curYpos
                lda     saveYpos+1      ; same for Y position
                sta     curYpos+1
                rts

; Clear screen
; Parameters: value of decoque = n
; Clears part of the screen. If n is 0 (or missing), clear from cursor to end of screen. If n is 1,
; clear from cursor to beginning of the screen. If n is 2, clear entire screen (and moves cursor to 
; upper left on DOS ANSI.SYS). If n is 3, clear entire screen and delete all lines saved in the 
; scrollback buffer (this feature was added for xterm and is supported by other terminal applications). 
clear_screen:   lda     decoque         ; load n
                beq     clear_to_end    
                cmp     #1  
                beq     clear_to_begin
                lda     #<SCREEN        ; clear the entire screen, set start address of screen
                sta     CLRSTART
                lda     #>SCREEN
                sta     CLRSTART+1
clear_screen1:  lda     #<SCREENEND     ; set end address of screen
                sta     CLREND
                lda     #>SCREENEND
                sta     CLREND+1
                jmp     clearscrn       ; clear the screen

clear_to_end:   ldy     #0              ; load pointer
                jsr     clr_cur_pos     ; set cursor address in start address
                jmp     clear_screen1   ; continue like clear screen

clear_to_begin: ldy     #2              ; load pointer
                jsr     clr_cur_pos     ; set cursor address in end address
                lda     #<SCREEN        ; clear the entire screen, set start address of screen
                sta     CLRSTART
                lda     #>SCREEN
                sta     CLRSTART+1
                jmp     clearscrn       ; clear the screen

; Clear line 
; Parameters: value of decoque = n
; Erases part of the line. If n is 0 (or missing), clear from cursor to the end of the line. 
; If n is 1, clear from cursor to beginning of the line. If n is 2, clear entire line. Cursor 
; position does not change.
erase_line:     lda     decoque         ; load n
                beq     erase_to_end    
                cmp     #1
                beq     erase_to_begin
                lda     curYpos         ; erase entire line
                clc                     ; clear carry for addition
                sta     CLRSTART        ; set start address of line
                adc     #NUMCOLS        ; calculate end address
                sta     CLREND
                lda     curYpos+1      
                sta     CLRSTART+1
                adc     #0
                sta     CLREND+1
                jmp     clearscrn       ; clear the line

erase_to_end:   ldy     #0              ; load index
                jsr     clr_cur_pos     ; set start address for clearing
                lda     curYpos         ; set end address of line
                clc                     ; clear carry for addition
                adc     #NUMCOLS        ; calculate end address
                sta     CLREND
                lda     curYpos+1      
                adc     #0
                sta     CLREND+1
                jmp     clearscrn       ; clear the line from cursor to end

erase_to_begin: ldy     #2              ; load index
                jsr     clr_cur_pos     ; set end address for clearing
                lda     curYpos         ; set begin address of line
                sta     CLRSTART
                lda     curYpos+1
                sta     CLRSTART+1
                jmp     clearscrn       ; clear the line from begin to cursor
             

graphics_mode:  ldy     #0              ; init pointer
gm_loop:        iny                     ; increment pointer
                lda     queue+1,y       ; load attribute data
                beq     gm_attr_off     ; #0 = all attributes off
                cmp     #'m'            ; Some programs (like man command) use [m for clearing all attributes
                beq     gm_attr_off
                cmp     #4              ; #4 = underline on
                beq     gm_underline    
                cmp     #7              ; #7 = reverse video
                beq     gm_reverse
                cmp     #8              ; #8 = consealed on
                beq     gm_consealed
                cmp     #30             ; #30 - #37 is forground colour
                bmi     gm_next         ; if < 30 then ignore code
                cmp     #38             
                bpl     gm_bgcol        ; if >= 38 then test for background colour
                tax                     ; move colour code to x-index
                lda     attrib          ; load current attribute
                and     #$F8            ; clear fg colour bits
                ora     colourtabfg-30,x; set new fg colour bits
                sta     attrib
                jmp     gm_next         ; continue
gm_bgcol:       cmp     #40             ; #40 - #47 is background colour
                bmi     gm_next         ; if < 40 then ignore code
                cmp     #48             ; if >= 48 then also ignore code
                bpl     gm_next
                tax                     ; move colour code to x-index
                lda     attrib          ; load current attribute
                and     #$8F            ; clear fg colour bits
                ora     colourtabbg-40,x; set new fg colour bits
                sta     attrib
gm_next:        cpy     quelen          ; any mode parameters left
                bne     gm_loop         ; yes, process the next parameter
gm_end:         rts                     ; else return

gm_attr_off:    lda     #DefaultAttrib  ; restore attributes to default
                sta     attrib          ; set in workspace
                bne     gm_next         ; check for next parameter

gm_underline:   lda     attrib          ; load current attribute
                ora     #8              ; set underline bit
                sta     attrib          ; set in workspace
                bne     gm_next         ; check for next parameter

gm_reverse:     lda     attrib          ; load current attrib
                and     #$07            ; extract current foreground colour
                asl                     ; move to upper nibble
                asl
                asl
                asl
                sta     temp            ; store in workspace
gm_reverse1:    lda     attrib          ; reload current attrib
                and     #$70            ; extract current background colour
                clc                     ; clear carry for moving
                lsr                     ; move to lower nibble
                lsr
                lsr
                lsr
                ora     temp            ; merge with temp data
                sta     temp            
                lda     attrib          ; reload current attrib
                and     #$88            ; clear all colour bits
                ora     temp            ; merge with new colour data
                sta     attrib          ; write new attribute
                jmp     gm_next         ; process next parameter

gm_consealed:   lda     attrib          ; load current attribute
                and     #$F8            ; clear fg colour bits
                sta     temp            ; store in workspace
                jmp     gm_reverse1     ; 

; clears (a part of) the screen. Parameters:
; - CLRSTART    start address of area to clear
; - CLREND      end address of area to clear
clearscrn:      ldy     #0              ; reset pointer
clearscrn1:     lda     #' '            ; load value for space
                sta     (CLRSTART),y    ; write to video memory
                inc     CLRSTART        ; increment pointer
                bne     clearscrn2 
                inc     CLRSTART+1
clearscrn2:     lda     CLRSTART+1      ; test for end of area to clear
                cmp     CLREND+1
                bne     clearscrn1      ; not equal, continue to next address
                lda     CLRSTART  
                cmp     CLREND
                bne     clearscrn1      ; not equal, continue to next address
                rts

clr_cur_pos:    lda     curYpos         ; load low byte Y position of cursor address
                clc                     ; clear carry for addition
                adc     curXpos         ; add X position of cursor address
                sta     ZP,y            ; store in workspace
                lda     curYpos+1       ; load high byte Y position of cursor address
                adc     #0              ; add carry
                sta     ZP+1,y          ; store in workspace
                rts                     ; return

colourtabfg:    .byte   $00             ; black
                .byte   $04             ; red
                .byte   $02             ; green
                .byte   $06             ; yellow
                .byte   $01             ; blue
                .byte   $05             ; magenta
                .byte   $03             ; cyan
                .byte   $07             ; white
                .word   $0000           ; ignored, also black
colourtabbg:    .byte   $00             ; black
                .byte   $40             ; red
                .byte   $20             ; green
                .byte   $60             ; yellow
                .byte   $10             ; blue
                .byte   $50             ; magenta
                .byte   $30             ; cyan
                .byte   $70             ; white
                .word   $0000           ; ignored, also black
