#!/bin/bash

nasm -I boot/include/ -o boot/mbr.bin boot/mbr.S
nasm -I boot/include/ -o boot/loader.bin boot/loader.S
dd if=boot/mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc
dd if=boot/loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc

nasm -f elf -o lib/kernel/print.o lib/kernel/print.S
gcc -I lib/ -I lib/kernel/ -m32 -c -o kernel/main.o kernel/main.c
ld kernel/main.o lib/kernel/print.o -melf_i386 -Ttext 0xc0001500 -e main -o kernel/kernel.bin
dd if=kernel/kernel.bin of=hd60M.img bs=512 count=200 seek=9 conv=notrunc

bochs -f bochsrc.disk
