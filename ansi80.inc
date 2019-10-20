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
                jsr     cursor_off      ; Turn cursor off
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
        and     #$ff            ; Is the character an extended ascii code?
        bmi     LFD44           ; ..yes, so don't print it

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
co:     sta ($DE),Y             ; ..and return it to the screen
        pla                     ; Restore character to accumulator
        rts

cursor_on:
        pha                     ; Save character in accumulator
        ldy curXpos             ; load cursor position on line
        lda ($DE),Y             ; Get character at current print position
        ora #$80                ; Mask it
        bne co                  ; jump always
        


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
                jsr     cursor_on       ; turn cursor on
                rts



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
exe11:          cmp     #'S'            ; check for scroll up
                bne     exe12
                jmp     scroll_up
exe12:          cmp     #'T'            ; check for scroll down
                bne     exe13
                jmp     scroll_dn
exe13:          cmp     #'p'            ; check for plot command (private extention)
                bne     exe14
                jmp     exevdu25
exe14:          cmp     #'v'            ; check for vdu control command (private extention)
                bne     exe15
                jmp     vdu_cmd
exe15:
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
cursor_up2:     jsr     cursor_on       ; turn cursor on
                rts                     ; end of routine

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
cursor_down2:   jsr     cursor_on       ; turn cursor on
                rts                     ; end of routine


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
                jsr     cursor_on       ; turn cursor on
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

cursor_restore: ldy     curXpos         ; turn cursor off at current position
                jsr     cursor_off
                lda     saveXpos        ; copy old cursor position
                sta     curXpos         ; store in queue
                lda     saveYpos        ; same for Y position
                sta     curYpos
                lda     saveYpos+1      ; same for Y position
                sta     curYpos+1
                jmp     cursor_on       ; turn cursor on at old position and end routine

; Clear screen
; Parameters: value of decoque = n
; Clears part of the screen. If n is 0 (or missing), clear from cursor to end of screen. If n is 1,
; clear from cursor to beginning of the screen. If n is 2, clear entire screen (and moves cursor to 
; upper left on DOS ANSI.SYS). If n is 3, clear entire screen and delete all lines saved in the 
; scrollback buffer (this feature was added for xterm and is supported by other terminal applications). 
clear_screen:   lda     quelen          ; test if parameter is omitted
                beq     clear_to_end    ; yes it is omitted, so clear from cursor to the end
                lda     decoque         ; load n
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
erase_line:     jsr     cursor_off      ; remove cursor from screen
                lda     quelen          ; are there any parameters?
                beq     erase_to_end    ; no, start erasing from cursor to the end of the line
                lda     decoque         ; load n
                beq     erase_to_end    ; if n = 0 then start erasing from cursor to the end of the line
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
                lda     quelen          ; are there any parameters?
                beq     gm_attr_off     ; no, treat it like a reset command
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
; Return: A, Y undefined; X unchanged
clearscrn:      LDA     #$80            ; Switch back to text mode
                STA     ModeExtension
                ldy     #0              ; reset pointer
                lda     CLREND          ; if the cursor is at the left upper corner then there is nothing to clean
                bne     clearscrn0      ; low byte is not $00, so jump to next test
                lda     CLREND+1
                cmp     #$80
                beq     clearscrn3      ; it is $8000, skip routine
clearscrn0:     lda     CLRSTART        ; if the cursor is at the right bottom corner then there is nothing to clean
                cmp     #<(SCREENATTR-1)            
                bne     clearscrn0a     ; low byte is not $ff, so start erasing
                lda     CLRSTART+1
                cmp     #>(SCREENATTR-1)
                beq     clearscrn3      ; it is right bottom, skip routine
clearscrn0a:    clc                     ; set start of attributes (= CLRSTART + ROWS*COLS)
                lda     CLRSTART        
                adc     #<(NUMROWS*NUMCOLS)
                sta     ATTRORIG
                lda     CLRSTART+1
                adc     #>(NUMROWS*NUMCOLS)
                sta     ATTRORIG+1
clearscrn1:     lda     #' '            ; load value for space
                sta     (CLRSTART),y    ; write to video memory
                lda     attrib          ; load attribute value
                sta     (ATTRORIG),y    ; write to attribute memory
                inc     CLRSTART        ; increment data pointer
                bne     clearscrn1a 
                inc     CLRSTART+1
clearscrn1a:    inc     ATTRORIG        ; increment attribute pointer
                bne     clearscrn2
                inc     ATTRORIG+1
clearscrn2:     lda     CLRSTART+1      ; test for end of area to clear
                cmp     CLREND+1
                bne     clearscrn1      ; not equal, continue to next address
                lda     CLRSTART  
                cmp     CLREND
                bne     clearscrn1      ; not equal, continue to next address
clearscrn3:     rts

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
                .byte   $00             ; ignored, also black
                .byte   DefaultAttrib   ; default colour
colourtabbg:    .byte   $00             ; black
                .byte   $40             ; red
                .byte   $20             ; green
                .byte   $60             ; yellow
                .byte   $10             ; blue
                .byte   $50             ; magenta
                .byte   $30             ; cyan
                .byte   $70             ; white
                .byte   $00             ; ignored, also black
                .byte   (DefaultAttrib/16)*16; default background colour


; Scroll routines
; Number of lines to scroll in decoque. On exit all registers are undefined.

scroll_up:      jsr     cursor_1        ; check if parameters given, if not then default to 1
                ldx     decoque         ; load number of lines to scroll
                jsr     cursor_off      ; turn cursor off
scroll_up0:     lda     #<(SCREEN+NUMCOLS); set orig address of copy in workspace
                sta     SCRNORIG        
                lda     #>(SCREEN+NUMCOLS)
                sta     SCRNORIG+1
                lda     #<SCREEN        ; set dest address of copy in workspace
                sta     SCRNDEST
                lda     #>SCREEN
                sta     SCRNDEST+1
                lda     #<(SCREENEND+NUMCOLS*2);  set orig address of attributes in workspace
                sta     ATTRORIG
                lda     #>(SCREENEND+NUMCOLS*2)
                sta     ATTRORIG+1
                lda     #<(SCREENEND+NUMCOLS)   ; set dest address of attributes in workspace
                sta     ATTRDEST
                lda     #>(SCREENEND+NUMCOLS)
                sta     ATTRDEST+1
                ldy     #0              ; clear index register
scroll_up1:     lda     (SCRNORIG),y    ; load byte
                sta     (SCRNDEST),y    ; store byte
                lda     (ATTRORIG),y    ; load attribute
                sta     (ATTRDEST),y    ; store attribute
                inc     SCRNORIG        ; increment all pointers (orig + dest)
                bne     *+4
                inc     SCRNORIG+1   
                inc     SCRNDEST
                bne     *+4
                inc     SCRNDEST+1
                inc     ATTRORIG
                bne     *+4
                inc     ATTRORIG+1
                inc     ATTRDEST
                bne     *+4
                inc     ATTRDEST+1
                lda     SCRNORIG+1      ; check if last address for copy is reached
                cmp     #>(SCREENEND+NUMCOLS)     ; compare source pointer to end address
                bne     scroll_up1      ; not equal, jump for next byte
                lda     SCRNORIG
                cmp     #<(SCREENEND+NUMCOLS)
                bne     scroll_up1
scroll_up2:     lda     #<SCREENEND     ; clear the bottom line
                sta     CLRSTART        ; set first address (begin of line)
                lda     #>SCREENEND
                sta     CLRSTART+1
                lda     #<(SCREENEND+NUMCOLS)     ; set last address (end of line)
                sta     CLREND
                lda     #>(SCREENEND+NUMCOLS)
                sta     CLREND+1
                jsr     clearscrn       ; clear the line
                dex                     ; decrement loop counter
                bne     scroll_up0      ; repeat loop if not ended yet
                rts                     ; return 

scroll_dn:      jsr     cursor_1        ; check if parameters given, if not then default to 1
                ldx     decoque         ; load number of lines to scroll
                jsr     cursor_off      ; turn cursor off
scroll_dn0:     lda     #<(SCREEN+NUMCOLS*(NUMROWS-1)-1); set orig address of copy in workspace
                sta     SCRNORIG        
                lda     #>(SCREEN+NUMCOLS*(NUMROWS-1)-1)
                sta     SCRNORIG+1
                lda     #<(SCREEN+NUMCOLS*NUMROWS-1)        ; set dest address of copy in workspace
                sta     SCRNDEST
                lda     #>(SCREEN+NUMCOLS*(NUMROWS)-1)
                sta     SCRNDEST+1
                lda     #<(SCREENEND+NUMCOLS*2+NUMCOLS*(NUMROWS-1)-1);  set orig address of attributes in workspace
                sta     ATTRORIG
                lda     #>(SCREENEND+NUMCOLS*2+NUMCOLS*(NUMROWS-1)-1)
                sta     ATTRORIG+1
                lda     #<(SCREENEND+NUMCOLS*2+(NUMCOLS*NUMROWS-1))   ; set dest address of attributes in workspace
                sta     ATTRDEST
                lda     #>(SCREENEND+NUMCOLS*2+(NUMCOLS*NUMROWS-1))
                sta     ATTRDEST+1
                ldy     #0              ; clear index register
scroll_dn1:     lda     (SCRNORIG),y    ; load byte
                sta     (SCRNDEST),y    ; store byte
                lda     (ATTRORIG),y    ; load attribute
                sta     (ATTRDEST),y    ; store attribute
                dec     SCRNORIG        ; increment all pointers (orig + dest)
                lda     SCRNORIG        ; check on overflow
                cmp     #$FF
                bne     *+4
                dec     SCRNORIG+1   
                dec     SCRNDEST
                lda     SCRNDEST
                cmp     #$FF
                bne     *+4
                dec     SCRNDEST+1
                dec     ATTRORIG
                lda     ATTRORIG
                cmp     #$FF
                bne     *+4
                dec     ATTRORIG+1
                dec     ATTRDEST
                lda     ATTRDEST
                cmp     #$FF
                bne     *+4
                dec     ATTRDEST+1
                lda     SCRNORIG+1      ; check if last address for copy is reached
                bmi     scroll_dn1      ; if it's still negative then address >= 8000 which is first address of video memory
scroll_dn2:     lda     #<SCREEN        ; clear the top line
                sta     CLRSTART        ; set first address (begin of line)
                lda     #>SCREEN
                sta     CLRSTART+1
                lda     #<(SCREEN+NUMCOLS)     ; set last address (end of line)
                sta     CLREND
                lda     #>(SCREEN+NUMCOLS)
                sta     CLREND+1
                jsr     clearscrn       ; clear the line
                dex                     ; decrement loop counter
                bne     scroll_dn0      ; repeat loop if not ended yet
                rts                     ; return 


                
