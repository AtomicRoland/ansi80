; ANSI80 keyboard scan routines 

scan80:             LDA     #<SCAN          ; SET ALTERNATIVE KEYBOARD READ ROUTINE
                    STA     RDCVEC
                    LDA     #>SCAN
                    STA     RDCVEC+1
                    LDA     #$00            ; RESET LOCK KEY STATUS (caps lock off)
                    STA     LOCKSTAT        
                    STA     LEDCTRL         ; INITIALIZE LED CONTROL REGISTER
                    STA     LEDDATA         ; ALL LEDS OFF
                    STA     KEYRDPTR        ; RESET KEYBOARD INPUT BUFFER (pointers)
                    STA     KEYWRPTR
                    RTS                     ; END OF INIT ROUTINE

; This routine checks if a key is pressed; if a key is pressed it will be send via the serial interface. Received
; bytes will be handled by the interrupt service routine.
SCAN:               JSR     GETKEY          ; SCAN THE KEYBOARD
                    CMP     #0              ; TEST IF KEY PRESSED
                    BEQ     SCAN1           ; JUMP IF NO KEY IS PRESSED
                    JMP     STOREKEY        ; STORE KEY STROKE(S) IN INPUT BUFFER
SCAN1:              RTS                     ; END ROUTINE

; Check and read a key
; This routine scans the keyboard and returns the ascii value of that key or returns $00 when no key is pressed.

GETKEY:             JSR     KEYSCAN         ; TEST IF A KEY IS PRESSED
                    BCS     ENDGETKEY       ; IF NO KEY IS PRESSED THEN END
                    LDX     #12             ; WAIT A SHORT WHILE
                    JSR     WAIT      
                    JMP     KEY2ASCII       ; GET THE ASCII VALUE OF THE KEY AND RETURN TO MAIN LOOP
ENDGETKEY:          LDA     #$00            ; LOAD ZERO CHARACTER TO INDICATE NO KEY PRESSED
                    RTS                     ; RETURN TO MAIN LOOP
                    

STOREKEY:           LDY     KEYWRPTR        ; LOAD KEYBOARD WRITE POINTER
                    STA     INPUTBUF,Y      ; STORE CHARACTER
                    INY                     ; INCREMENT POINTER
                    BPL     STOREKEY1       ; JUMP IF NOT END OF BUFFER REACHED
                    LDY     #0              ; RESET POINTER
STOREKEY1:          STY     KEYWRPTR        ; SAVE POINTER
                    JSR     KEYLEDON        ; TURN INPUT BUFFER DATA LED ON
                    RTS                     ; RETURN FROM SUBROUTINE

CAPSLEDON:          LDA     LEDDATA         ; LOAD CURRENT DATA
                    ORA     #$80            ; SET BIT
SETLED:             STA     LEDDATA         ; WRITE NEW DATA
                    RTS

CAPSLEDOFF:         LDA     LEDDATA         ; LOAD CURRENT DATA
                    AND     #$7F            ; CLEAR BIT
                    JMP     SETLED          ; SET LED AND RETURN

KEYLEDON:           LDA     LEDDATA         ; LOAD CURRENT DATA
                    ORA     #$08            ; SET BIT
                    JMP     SETLED          ; SET LED AND RETURN

KEYLEDOFF:          LDA     LEDDATA         ; LOAD CURRENT DATA
                    AND     #$F7            ; CLEAR BIT
                    JMP     SETLED          ; SET LED AND RETURN

; Convert the scancode to ascii code
; The scancode is in the Y register. On return the ascii value will be in A.

KEY2ASCII:          CPY     #5              ; CHECK FOR CAPS LOCK KEY
                    BEQ     LOCKEY          ; JMP IF CAPS LOCK
                    LDA     #$00            ; CLEAR ACCU
                    CPY     #$21            ; TEST FOR 'A' KEY
                    BMI     KEY0            ; JUMP IF SCAN CODE BEFORE 'A' KEY
                    CPY     #$3B            ; TEST FOR 'Z' KEY
                    BPL     KEY0            ; JUMP IF SCAN CODE AFTER 'Z' KEY
                    LDA     LOCKSTAT        ; LOAD LOCK STATUS
KEY0:               EOR     PORTB           ; CHECK FOR SHIFT KEY (if caps lock on than bit 7 will be inverted)
                    BMI     KEY1            ; IF NO SHIFT, GO CHECK FOR CTRL
                    LDA     #<KEY_SHIFT     ; SET ADDRESS OF SHIFT TABLE IN ZERO PAGE
                    STA     ZP
                    LDA     #>KEY_SHIFT     
                    STA     ZP+1      
                    JMP     KEY3            ; JMP TO CONVERSION ROUTINE
KEY1:               AND     #$40            ; CHECK FOR CTRL KEY
                    BNE     KEY2            ; NO CTRL, GOTO DEFAULT TABLE
                    LDA     #<KEY_CTRL      ; SET ADDRESS OF CTRL TABLE IN ZERO PAGE
                    STA     ZP
                    LDA     #>KEY_CTRL
                    STA     ZP+1
                    JMP     KEY3            ; JMP TO CONVERSION ROUTINE
KEY2:               LDA     #<KEY_TABLE     ; SET ADDRESS OF DEFAULT TABLE IN ZERO PAGE
                    STA     ZP  
                    LDA     #>KEY_TABLE
                    STA     ZP+1
KEY3:               LDA     (ZP),Y          ; LOAD THE CORRESPONDING ASCII VALUE
                    RTS                     ; RETURN (TO MAIN LOOP)

; CAPS LOCK KEY TOGGLES JUST THE FLAG IN ZERO PAGE AND THE LED.
LOCKEY:             LDA     LOCKSTAT        ; LOAD LOCK STATUS ($00 = LOCK OFF, $40 = LOCK ON)
                    EOR     #$80            ; TOGGLE LOCK STATUS
                    STA     LOCKSTAT        ; WRITE NEW LOCK STATUS
                    BNE     LOCKEY_ON       ; SWITCH CAPS LOCK LED ON ...
                    JSR     CAPSLEDOFF      ; ... OR OFF
                    LDA     #0              ; CLEAR ACCU TO PREVENT STORING DATA INTO BUFFER
                    RTS                     ; RETURN TO MAIN LOOP
LOCKEY_ON:          JSR     CAPSLEDON
                    LDA     #0              ; CLEAR ACCU TO PREVENT STORING DATA INTO BUFFER
                    RTS                     ; RETURN TO MAIN LOOP

KEY_TABLE:          
                    .byte   ' '             ; SCAN CODE $00, SPACE BAR
                    .byte   '['             ; SCAN CODE $01, [
                    .byte   '\'             ; SCAN CODE $02, \
                    .byte   ']'             ; SCAN CODE $03, ]
                    .byte   '^'             ; SCAN CODE $04, ^
                    .byte   $00             ; SCAN CODE $05, LOCK
                    .byte   $09             ; SCAN CODE $06, ARROW HOR
                    .byte   $0B             ; SCAN CODE $07, ARROW VERT
                    .byte   $00             ; SCAN CODE $08, 
                    .byte   $00             ; SCAN CODE $09, 
                    .byte   $00             ; SCAN CODE $0A, 
                    .byte   $00             ; SCAN CODE $0B, 
                    .byte   $00             ; SCAN CODE $0C, 
                    .byte   $0D             ; SCAN CODE $0D, RETURN
                    .byte   $00             ; SCAN CODE $0E, COPY
                    .byte   $7F             ; SCAN CODE $0F, DELETE

                    .byte   '0'             ; SCAN CODE $10, 0
                    .byte   '1'             ; SCAN CODE $11, 1!
                    .byte   '2'             ; SCAN CODE $12, 2"
                    .byte   '3'             ; SCAN CODE $13, 3#
                    .byte   '4'             ; SCAN CODE $14, 4$
                    .byte   '5'             ; SCAN CODE $15, 5%
                    .byte   '6'             ; SCAN CODE $16, 6&
                    .byte   '7'             ; SCAN CODE $17, 7'
                    .byte   '8'             ; SCAN CODE $18, 8(
                    .byte   '9'             ; SCAN CODE $19, 9)
                    .byte   ':'             ; SCAN CODE $1A, :*
                    .byte   ';'             ; SCAN CODE $1B, ;+
                    .byte   ','             ; SCAN CODE $1C, ,<
                    .byte   '-'             ; SCAN CODE $1D, -=
                    .byte   '.'             ; SCAN CODE $1E, .>
                    .byte   '/'             ; SCAN CODE $1F, /?

                    .byte   '@'             ; SCAN CODE $20, @
                    .byte   'a'             ; SCAN CODE $21, A
                    .byte   'b'             ; SCAN CODE $22, B
                    .byte   'c'             ; SCAN CODE $23, C
                    .byte   'd'             ; SCAN CODE $24, D
                    .byte   'e'             ; SCAN CODE $25, E
                    .byte   'f'             ; SCAN CODE $26, F
                    .byte   'g'             ; SCAN CODE $27, G
                    .byte   'h'             ; SCAN CODE $28, H
                    .byte   'i'             ; SCAN CODE $29, I
                    .byte   'j'             ; SCAN CODE $2A, J
                    .byte   'k'             ; SCAN CODE $2B, K
                    .byte   'l'             ; SCAN CODE $2C, L
                    .byte   'm'             ; SCAN CODE $2D, M
                    .byte   'n'             ; SCAN CODE $2E, N
                    .byte   'o'             ; SCAN CODE $2F, O

                    .byte   'p'             ; SCAN CODE $30, P
                    .byte   'q'             ; SCAN CODE $31, Q
                    .byte   'r'             ; SCAN CODE $32, R
                    .byte   's'             ; SCAN CODE $33, S
                    .byte   't'             ; SCAN CODE $34, T
                    .byte   'u'             ; SCAN CODE $35, U
                    .byte   'v'             ; SCAN CODE $36, V
                    .byte   'w'             ; SCAN CODE $37, W
                    .byte   'x'             ; SCAN CODE $38, X
                    .byte   'y'             ; SCAN CODE $39, Y
                    .byte   'z'             ; SCAN CODE $3A, Z
                    .byte   $1B             ; SCAN CODE $3B, ESC
                    .byte   $00             ; SCAN CODE $3C, 
                    .byte   $00             ; SCAN CODE $3D, 
                    .byte   $00             ; SCAN CODE $3E, 
                    .byte   $00             ; SCAN CODE $3F, 

KEY_SHIFT: 
                    .byte   ' '             ; SCAN CODE $00, SPACE BAR
                    .byte   '{'             ; SCAN CODE $01, [
                    .byte   '|'             ; SCAN CODE $02, \
                    .byte   '}'             ; SCAN CODE $03, ]
                    .byte   '_'             ; SCAN CODE $04, ^
                    .byte   $00             ; SCAN CODE $05, LOCK
                    .byte   $08             ; SCAN CODE $06, ARROW HOR
                    .byte   $0A             ; SCAN CODE $07, ARROW VERT
                    .byte   $00             ; SCAN CODE $08, 
                    .byte   $00             ; SCAN CODE $09, 
                    .byte   $00             ; SCAN CODE $0A, 
                    .byte   $00             ; SCAN CODE $0B, 
                    .byte   $00             ; SCAN CODE $0C, 
                    .byte   $0D             ; SCAN CODE $0D, RETURN
                    .byte   $00             ; SCAN CODE $0E, COPY
                    .byte   $7F             ; SCAN CODE $0F, DELETE

                    .byte   '0'             ; SCAN CODE $10, 0
                    .byte   '!'             ; SCAN CODE $11, 1!
                    .byte   '"'             ; SCAN CODE $12, 2"
                    .byte   '#'             ; SCAN CODE $13, 3#
                    .byte   '$'             ; SCAN CODE $14, 4$
                    .byte   '%'             ; SCAN CODE $15, 5%
                    .byte   '&'             ; SCAN CODE $16, 6&
                    .byte   "'"             ; SCAN CODE $17, 7'
                    .byte   '('             ; SCAN CODE $18, 8(
                    .byte   ')'             ; SCAN CODE $19, 9)
                    .byte   '*'             ; SCAN CODE $1A, :*
                    .byte   '+'             ; SCAN CODE $1B, ;+
                    .byte   '<'             ; SCAN CODE $1C, ,<
                    .byte   '='             ; SCAN CODE $1D, -=
                    .byte   '>'             ; SCAN CODE $1E, .>
                    .byte   '?'             ; SCAN CODE $1F, /?

                    .byte   '@'             ; SCAN CODE $20, @
                    .byte   'A'             ; SCAN CODE $21, A
                    .byte   'B'             ; SCAN CODE $22, B
                    .byte   'C'             ; SCAN CODE $23, C
                    .byte   'D'             ; SCAN CODE $24, D
                    .byte   'E'             ; SCAN CODE $25, E
                    .byte   'F'             ; SCAN CODE $26, F
                    .byte   'G'             ; SCAN CODE $27, G
                    .byte   'H'             ; SCAN CODE $28, H
                    .byte   'I'             ; SCAN CODE $29, I
                    .byte   'J'             ; SCAN CODE $2A, J
                    .byte   'K'             ; SCAN CODE $2B, K
                    .byte   'L'             ; SCAN CODE $2C, L
                    .byte   'M'             ; SCAN CODE $2D, M
                    .byte   'N'             ; SCAN CODE $2E, N
                    .byte   'O'             ; SCAN CODE $2F, O

                    .byte   'P'             ; SCAN CODE $30, P
                    .byte   'Q'             ; SCAN CODE $31, Q
                    .byte   'R'             ; SCAN CODE $32, R
                    .byte   'S'             ; SCAN CODE $33, S
                    .byte   'T'             ; SCAN CODE $34, T
                    .byte   'U'             ; SCAN CODE $35, U
                    .byte   'V'             ; SCAN CODE $36, V
                    .byte   'W'             ; SCAN CODE $37, W
                    .byte   'X'             ; SCAN CODE $38, X
                    .byte   'Y'             ; SCAN CODE $39, Y
                    .byte   'Z'             ; SCAN CODE $3A, Z
                    .byte   $1B             ; SCAN CODE $3B, ESC
                    .byte   $00             ; SCAN CODE $3C, 
                    .byte   $00             ; SCAN CODE $3D, 
                    .byte   $00             ; SCAN CODE $3E, 
                    .byte   $00             ; SCAN CODE $3F, 

KEY_CTRL:    
                    .byte   $00             ; SCAN CODE $00, SPACE BAR
                    .byte   $00             ; SCAN CODE $01, [
                    .byte   $00             ; SCAN CODE $02, \
                    .byte   $00             ; SCAN CODE $03, ]
                    .byte   '~'             ; SCAN CODE $04, ^
                    .byte   $00             ; SCAN CODE $05, LOCK
                    .byte   $00             ; SCAN CODE $06, ARROW HOR
                    .byte   $00             ; SCAN CODE $07, ARROW VERT
                    .byte   $00             ; SCAN CODE $08, 
                    .byte   $00             ; SCAN CODE $09, 
                    .byte   $00             ; SCAN CODE $0A, 
                    .byte   $00             ; SCAN CODE $0B, 
                    .byte   $00             ; SCAN CODE $0C, 
                    .byte   $00             ; SCAN CODE $0D, RETURN
                    .byte   $00             ; SCAN CODE $0E, COPY
                    .byte   $00             ; SCAN CODE $0F, DELETE

                    .byte   $00             ; SCAN CODE $10, 0
                    .byte   $00             ; SCAN CODE $11, 1!
                    .byte   $00             ; SCAN CODE $12, 2"
                    .byte   $00             ; SCAN CODE $13, 3#
                    .byte   $00             ; SCAN CODE $14, 4$
                    .byte   $00             ; SCAN CODE $15, 5%
                    .byte   $00             ; SCAN CODE $16, 6&
                    .byte   $00             ; SCAN CODE $17, 7'
                    .byte   $00             ; SCAN CODE $18, 8(
                    .byte   $00             ; SCAN CODE $19, 9)
                    .byte   $00             ; SCAN CODE $1A, :*
                    .byte   $1C             ; SCAN CODE $1B, ;+
                    .byte   $1D             ; SCAN CODE $1C, ,<
                    .byte   '_'             ; SCAN CODE $1D, -=
                    .byte   $1E             ; SCAN CODE $1E, .>
                    .byte   $1F             ; SCAN CODE $1F, /?

                    .byte   $00             ; SCAN CODE $20, @
                    .byte   $01             ; SCAN CODE $21, A
                    .byte   $02             ; SCAN CODE $22, B
                    .byte   $03             ; SCAN CODE $23, C
                    .byte   $04             ; SCAN CODE $24, D
                    .byte   $05             ; SCAN CODE $25, E
                    .byte   $06             ; SCAN CODE $26, F
                    .byte   $07             ; SCAN CODE $27, G
                    .byte   $08             ; SCAN CODE $28, H
                    .byte   $09             ; SCAN CODE $29, I
                    .byte   $0A             ; SCAN CODE $2A, J
                    .byte   $0B             ; SCAN CODE $2B, K
                    .byte   $0C             ; SCAN CODE $2C, L
                    .byte   $0D             ; SCAN CODE $2D, M
                    .byte   $0E             ; SCAN CODE $2E, N
                    .byte   $0F             ; SCAN CODE $2F, O

                    .byte   $10             ; SCAN CODE $30, P
                    .byte   $11             ; SCAN CODE $31, Q
                    .byte   $12             ; SCAN CODE $32, R
                    .byte   $13             ; SCAN CODE $33, S
                    .byte   $14             ; SCAN CODE $34, T
                    .byte   $15             ; SCAN CODE $35, U
                    .byte   $16             ; SCAN CODE $36, V
                    .byte   $17             ; SCAN CODE $37, W
                    .byte   $18             ; SCAN CODE $38, X
                    .byte   $19             ; SCAN CODE $39, Y
                    .byte   $1A             ; SCAN CODE $3A, Z
                    .byte   $1B             ; SCAN CODE $3B, ESC
                    .byte   $00             ; SCAN CODE $3C, 
                    .byte   $00             ; SCAN CODE $3D, 
                    .byte   $00             ; SCAN CODE $3E, 
                    .byte   $00             ; SCAN CODE $3F, 


