; Routines related to graphics

; Invert the screen  (the lazy author of this routine assumes always clear4 memory :-)
invert:         ldy     #$00            ; load index
                sty     CLRSTART        ; write to workspace
                lda     #$97
                sta     CLRSTART+1
invert1:        lda     (CLRSTART),y    ; load data
                eor     #$FF            ; invert
                sta     (CLRSTART),y    ; write data back
                iny                     ; increment index
                bne     invert1         ; jmp if block not ready
                dec     CLRSTART+1      ; increment memory block
                bmi     invert1         ; jmp if not ready
                rts                     ; end routine
             
; VDU control commands
;   1st parameter is the command number
;   - 1,m   set graphics mode
;   - 2,c   set colour
;   - 3,p   set palette
;   - 4     invert screen

vdu_cmd:        lda     decoque         ; load command number
                cmp     #1              ; test for graphics mode set
                beq     exevdu22
                cmp     #2              ; test for colour select
                beq     LDF05
                cmp     #3              ; test for colour set select
                beq     exevdu18
                cmp     #4              ; test for invert screen
                beq     invert
                rts                     ; otherwise, unrecognized function: return

; Switch to graphics mode. Mode is in decoque+1
exevdu22:       ldy     #0              ; reset y register
                lda     decoque+1       ; load mode
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

; Change colour set. Parameter: 0 or 1
exevdu18:       lda     decoque+1       ; load colour set number (0 or 1)
                bne     palette1        ; jmp if non-zero (i.e. palette 1)
                lda     $B002           ; load current video controller setting
                and     #$F7            ; reset bit 4
                jmp     palette2        ; continue
palette1:       lda     $B002           ; same procedure to set bit 4
                ora     #$08
palette2:       sta     $B002           ; write to video controller setting
                rts                     ; end of routine

; Change colour. Parameter: colour number (0...3 for standard Atom graphics)
LDF05:          lda     decoque+1       ; load colour number
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

; Plot routine. There are three parameters:
;   decoque     = plot mode
;   decoque+1,2 = x-coordinate (low byte first)
;   decoque+3,4 = y-coordinate (low byte first)
exevdu25:       lda     decoque+1       ; copy coordinates
                sta     $5A             ; set in zeropage/workspace
                lda     decoque+2       ; copy coordinates
                sta     $5B             ; set in zeropage/workspace
                lda     decoque+3       ; copy coordinates
                sta     $5C             ; set in zeropage/workspace
                lda     decoque+4       ; copy coordinates
                sta     $5D             ; set in zeropage/workspace
                lda     decoque         ; set plot mode
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
                lda     decoque         ; load the plot mode
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

                
