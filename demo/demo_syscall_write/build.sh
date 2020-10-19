#!/bin/bash
nasm -f elf -o syscall_write.o syscall_write.S
ld -melf_i386 -o syscall_write.bin syscall_write.o