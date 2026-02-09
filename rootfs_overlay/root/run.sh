#!/bin/sh

clear
stty cbreak raw -echo min 0
8086tiny/8086tiny 8086tiny/bios floppy.img
stty cooked echo
clear
