        atmhdr = 1

        .include "constants.inc"        
        
.if (atmhdr = 1)        
AtmHeader:
        .SEGMENT "HEADER"
        .byte    "ANSI80"
        .word    0,0,0,0,0
        .word    StartAddr
        .word    StartAddr
        .word    EndAddr - StartAddr
.endif

        .SEGMENT "CODE"

StartAddr:
        .include "ansi80.inc"
EndAddr:

