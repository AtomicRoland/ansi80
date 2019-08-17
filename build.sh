#!/bin/bash

rm -f ansi80

echo Assembling
ca65 -l ansi80.lst -o ansi80.o ansi80.asm

echo Linking
ld65 ansi80.o -o ansi80 -C ansi80.lkr 

echo Cleaning
rm -f *.o

echo Checksumming
md5sum ansi80

