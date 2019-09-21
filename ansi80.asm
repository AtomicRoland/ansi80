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
        jmp     vga80       ; initialize ansi80 vdu driver with default keyboard input
        jmp     scan80      ; initialize alternative keyboard input (stores data in input buffer)
        .include "ansi80.inc"
        .include "ansikb.inc"
        .include "ansifnt.inc"
EndAddr:

