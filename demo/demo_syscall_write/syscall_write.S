section .data
str_c_lib: db "c library says: hello world!", 0xa ; 0xa 为 LF ASCII 码
str_c_lib_len equ $ - str_c_lib

str_syscall: db "syscall says: hello world!", 0xa
str_syscall_len equ $ - str_syscall

section .text
global _start
_start:
; 模拟 C 语言中系统调用库函数 write
    push str_c_lib_len
    push str_c_lib
    push 1

    call simu_write
    add esp, 12

; 跨过库函数，直接进行系统调用
    mov eax, 4 ; 第 4 号子功能是 write 系统调用
    mov ebx, 1
    mov ecx, str_syscall
    mov edx, str_syscall_len
    int 0x80

; 退出程序
    mov eax, 1 ; 第 1 号子功能是 exit
    int 0x80

; 模拟 C 库中的 write 函数
simu_write:
    push ebp
    mov ebp, esp
    mov eax, 4 ; 第 4 号子功能是 write 系统调用
    mov ebx, [ebp + 8]
    mov ecx, [ebp + 12]
    mov edx, [ebp + 16]
    int 0x80
    pop ebp
    ret
