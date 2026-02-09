#!/bin/sh

clear
loadfont < fonts/ter-i12b.psf
stty cbreak raw -echo min 0
8086tiny/8086tiny 8086tiny/bios floppy.img
stty cooked echo
clear
