%include "boot.inc"
SECTION LOADER vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
jmp loader_start

; 构建 GDT 及其内部的描述符
GDT_BASE: dd 0x00000000
          dd 0x00000000
; 代码段描述符
CODE_DESC: dd 0x0000FFFF
          dd DESC_CODE_HIGH4
; 数据段和栈段描述符
DATA_STACK_DESC: dd 0x0000FFFF
                 dd DESC_DATA_HIGH4
; 显存段描述符
VIDEO_DESC: dd 0x80000007 ; limit = (0xbffff - 0xb8000) / 4k = 0x7
            dd DESC_VIDEO_HIGH4

GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1
times 60 dq 0
SELECTOR_CODE  equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA  equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

; 以下是 GDT 的指针，前 2 字节是 GDT 界限，后 4 字节是 GDT 起始地址
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

loadermsg db '2 loader in real.'

loader_start:

    mov sp, LOADER_BASE_ADDR
    mov bp, loadermsg        ; ES:BP = 字符串地址
    mov cx, 17               ; CX = 字符串长度
    mov ax, 0x1301           ; 13号子功能 (AH = 13) 显示输出方式 AL = 01h
    mov bx, 0x001f           ; 页号为 0 (BH = 0) 蓝底粉红字 (BL = 1fh)
    mov dx, 0x1800           ; 行号为 18h (DH = 18h) 列号为 0 (DL = 0)
    int 0x10                 ; 10h 号中断

    ; 准备进入保护模式
    
    ; 1 打开 A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

    ; 2 加载 GDT
    lgdt [gdt_ptr]

    ; 3 CR0 第 0 位置 1
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    jmp dword SELECTOR_CODE:p_mode_start ; 刷新流水线

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov byte [gs:160], 'P'

    jmp $ ; 程序悬停
