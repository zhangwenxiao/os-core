#!/bin/bash

nasm -I boot/include/ -o boot/mbr.bin boot/mbr.S
nasm -I boot/include/ -o boot/loader.bin boot/loader.S
dd if=boot/mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc
dd if=boot/loader.bin of=hd60M.img bs=512 count=4 seek=2 conv=notrunc

gcc -I lib/kernel/ -I lib/ -I kernel/ -I device/ -m32 -c -fno-builtin -o build/main.o kernel/main.c
nasm -f elf -o build/print.o lib/kernel/print.S
nasm -f elf -o build/kernel.o kernel/kernel.S
gcc -I lib/kernel/ -I lib/ -I kernel/ -I device/ -m32 -c -fno-builtin -o build/interrupt.o kernel/interrupt.c
gcc -I lib/kernel/ -I lib/ -I kernel/ -I device/ -m32 -c -fno-builtin -o build/init.o kernel/init.c
gcc -I lib/kernel/ -I lib/ -I kernel/ -I device/ -m32 -c -fno-builtin -o build/timer.o device/timer.c
ld -melf_i386 -Ttext 0xc0001500 -e main -o build/kernel.bin build/main.o build/print.o build/init.o build/interrupt.o build/kernel.o build/timer.o
dd if=build/kernel.bin of=hd60M.img bs=512 count=200 seek=9 conv=notrunc

bochs -f bochsrc.disk
