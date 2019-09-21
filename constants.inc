;=================================================================
; Constants used by ansi80
;=================================================================

; GODIL related addresses
GODIL         = $BDE0

DefaultAttrib = $07

ModeExtension = GODIL + 0
CursorCol     = GODIL + 2
CursorRow     = GODIL + 3
VgaCtrl       = GODIL + 4


; Workspace
vduque   = $4F0
temp     = vduque+0
quelen   = vduque+1
attrib   = vduque+2
fgcolor  = vduque+3
bgcolor  = vduque+4
saveXpos = vduque+5
saveYpos = vduque+6
bottomY  = vduque+7 
rightX   = vduque+8
leftX    = vduque+9
topY     = vduque+10
ctrlcode = vduque+11
curXpos  = $e0
curYpos  = $de
queue    = $500         ; ANSI SEQUENCE QUEUE
decoque  = $502         ; DECODED QUEUE
INPUTBUF = $580         ; KEYBOARD INPUT BUFFER
KEYRDPTR = $84          ; KEYBOARD READ POINTER
KEYWRPTR = $85          ; KEYBOARD WRITE POINTER
ZP       = $86          ; ZERO PAGE WORK SPACE
LOCKSTAT = $E7          ; STATUS OF CAPS LOCK KEY
LEDCTRL  = $BFE0        ; CONTROL REGISTER FOR LED STATUS
LEDDATA  = $BFE1        ; DATA REGISTER FOR LED STATUS
PORTB    = $B001        ; PORT B OF 8255 PPI (for shift + ctrl key) 
PORTC    = $B002        ; PORT C OF 8255 PPI (for rept key)

; MOS Entry addresses
WRCVEC   = $0208
RDCVEC   = $020A
KEYSCAN  = $FE71		; KEYBOARD SCAN ROUTINE
WAIT     = $FB8C        ; SHORT WAIT

; For VGA80 screens
;NUMROWS = 40
;NUMCOLS = 80

; Standard Atom vdu
NUMROWS = 16
NUMCOLS = 32

SCREEN = $8000
SCREENEND = SCREEN + (NUMROWS - 1) * NUMCOLS

; Send character in accumulator to the VIA
LFEFB = $FEFB

; Wait 0.1 second for debounce
LFB8A = $fB8A
	
; Scan keyboard	
LFE71 = $FE71

; Keyboard Control Code Handlers
; use Kernel for all except cursor handling (FDA2)
LFD9A = $FD9A
LFDAE = $FDAE
LFDC0 = $FDC0
LFDC2 = $FDC2
LFDC6 = $FDC6
LFDC8 = $FDC8
LFDD2 = $FDD2
LFDD6 = $FDD6
LFDD8 = $FDD8
LFDDF = $FDDF
LFDE2 = $FDE2

; Flyback
LFE66 = $FE66
LFE6B = $FE6B

