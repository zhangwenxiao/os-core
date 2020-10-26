TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text

; put_str 通过 put_char 来打印以 0 字符结尾的字符串
; 输入: 栈中参数为打印的字符串
; 输出: 无
global put_str
put_str:
    push ebx
    push ecx
    xor ecx, ecx
    mov ebx, [esp + 12]
.goon:
    mov cl, [ebx]
    cmp cl, 0     ; 0 表示 '\0'
    jz .str_over
    push ecx      ; 为 put_char 函数传递参数
    call put_char
    add esp, 4
    inc ebx
    jmp .goon
.str_over:
    pop ecx
    pop ebx
    ret

; put_char 把栈中的 1 个字符写入光标所在处
global put_char
put_char:
    pushad ; 备份 32 位寄存器环境
    mov ax, SELECTOR_VIDEO
    mov gs, ax
; 获取当前光标位置
    ; 先获取高 8 位
    mov dx, 0x03d4 ; 索引寄存器
    mov al, 0x0e   ; 用于提供光标位置的高 8 位
    out dx, al
    mov dx, 0x03d5 ; 通过读写数据端口 0x3d5 来获得或设置光标位置
    in al, dx      ; 得到了光标位置的高 8 位
    mov ah, al

    ; 再获取低 8 位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx

    ; 将光标存入 bx
    mov bx, ax
    ; 在栈中获取待打印的字符
    mov ecx, [esp + 36] ; pushad 压入 4 * 8 = 32 字节
                        ; 加上主调函数 4 字节的返回地址, 故 esp + 36 字节
    cmp cl, 0xd         ; CR 是 0x0d, LF 是 0x0a
    jz .is_carriage_return
    cmp cl, 0xa
    jz .is_line_feed
    cmp cl, 0x8         ; BS(backspace) 是 0x8
    jz .is_backspace
    jmp .put_other

.is_backspace:
    dec bx
    shl bx, 1              ; 光标左移 1 位等于乘 2
                           ; 表示光标对应显存中的偏移字节
    mov byte [gs:bx], 0x20 ; 将待删除的字节补为 0 或空格皆可
    inc bx
    mov byte [gs:bx], 0x70 ; 字符属性
    shr bx, 1              ; bx 从显存偏移量恢复为光标值
    jmp .set_cursor

.put_other:
    shl bx, 1              ; bx 从光标值变为显存偏移量
    mov [gs:bx], cl        ; 写入 ASCII 字符
    inc bx
    mov byte [gs:bx], 0x07 ; 写入字符属性
    shr bx, 1              ; bx 从显存偏移量恢复为光标值
    inc bx                 ; 光标前移
    cmp bx, 2000           ; 80 列 * 25 行 = 2000 个字符
    jl .set_cursor

.is_line_feed:
.is_carriage_return:
    xor dx, dx ; dx 是被除数的高 16 位, 清 0
    mov ax, bx ; ax 是被除数的低 16 位
    mov si, 80
    div si
    sub bx, dx ; 光标值减去除 80 的余数便是取整

.is_carriage_return_end:
    add bx, 80
    cmp bx, 2000
.is_line_feed_end:
    jl .set_cursor

; 屏幕行范围是 0 ~ 24, 滚屏的原理是将屏幕的第 1 ~ 24 行搬运到 0 ~ 23 行
; 再将第 24 行用空格填充
.roll_screen: ; 超出屏幕大小则开始滚屏
    cld
    mov ecx, 960        ; 2000 - 80 = 1920 个字符要搬运, 共 1920 * 2 = 3840 字节
                        ; 一次搬 4 字节，共 3840 / 4 = 960 次
    mov esi, 0xc00b80a0 ; 第 1 行行首
    mov edi, 0xc00b8000 ; 第 0 行行首, 显存起始虚拟地址是 0xc00b8000
    rep movsd

    ; 将最后一行填充为空白
    mov ebx, 3840             ; 最后一行首字符的第一个字节偏移 = 1920 * 2
    mov ecx, 80               ; 一行是 80 字符(160 字节), 每次清空 1 字符
                              ; (2 字节), 一行需要移动 80 次
.cls:
    mov word [gs:ebx], 0x0720 ; 0x0720 是黑底白字的空格键
    add ebx, 2
    loop .cls
    mov bx, 1920              ; 将光标值重置为 1920, 最后一行的首字符

.set_cursor: 
; 将光标设为 bx 值
; 先设置高 8 位
    mov dx, 0x03d4 ; 索引寄存器
    mov al, 0x0e   ; 用于提供光标位置的高 8 位
    out dx, al
    mov dx, 0x03d5 ; 通过读写数据端口 0x3d5 来获得或设置光标位置
    mov al, bh
    out dx, al
; 再设置低 8 位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

.put_char_done:
    popad
    ret
