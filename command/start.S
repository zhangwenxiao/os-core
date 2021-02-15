[bits 32]
extern main
section .text
global _start ; _start 是链接器默认的入口符号
_start:
	; 下面这两个要和 execv 中 load 之后指定的寄存器一致
	push ebx ; 压入 argv
	push ecx ; 压入 argc
	call main
