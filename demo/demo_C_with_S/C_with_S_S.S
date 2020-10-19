section .data
str: db "asm_print says hello world!", 0xa, 0 ; 0xa 是换行符, 0 代表结束符 \0
str_len equ $ - str

section .text
extern c_print
global _start
_start:
; 调用 C 代码中的函数 c_print
    push str
    call c_print
    add esp, 4

; 退出程序
    mov eax, 1 ; 第 1 号子功能是 exit 系统调用
    int 0x80

global asm_print
asm_print:
    push ebp
    mov ebp, esp
    mov eax, 4 ; 第 4 号子功能是 write 系统调用
    mov ebx, 1
    mov ecx, [ebp + 8]
    mov edx, [ebp + 12]
    int 0x80
    pop ebp
    ret