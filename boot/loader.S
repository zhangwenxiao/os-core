%include "boot.inc"
SECTION LOADER vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR

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

; 此处偏移 loader.bin 文件头 0x200 字节（GDT 有 64 项，每项 8 字节，64 * 8 = 0x200）
; total_mem_bytes 用于保存内存容量，以字节为单位
total_mem_bytes dd 0

; 以下是 GDT 的指针，前 2 字节是 GDT 界限，后 4 字节是 GDT 起始地址
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

; 人工对齐: total_mem_bytes(4) + gdt_ptr(6) + ards_buf(244) + ards_nr(2), 共 256 字节
ards_buf times 244 db 0
ards_nr dw 0 ; ards_nr 用于记录 ARDS 结构体数量

loader_start:

; int 15h eax = 0000E820h, edx = 534D4150h 获取内存布局
    xor ebx, ebx        ; 第一次调用时，ebx 要置 0
    mov edx, 0x534d4150
    mov di, ards_buf    ; ards 结构缓冲区

.e820_mem_get_loop: ; 循环获取每个 ARDS 内存范围描述结构
    mov eax, 0x0000e820         ; 执行 int 0x15 后，eax 值变为 0x534d4150
                                ; 所以需要更新值为子功能号
    mov ecx, 20                 ; ARDS 地址范围描述符结构大小是 20 字节
    int 0x15

    jc .e820_failed_so_try_e801 ; CF = 1 表示有错误发生，尝试 0xe801 功能

    add di, cx                  ; 使 di 增加 20 字节指向缓冲区中新的 ARDS 结构位置
    inc word [ards_nr]          ; 记录 ARDS 数量
    cmp ebx, 0                  ; 若 ebx 为 0 且 cf 不为 1, 说明 ards 已全部返回
    jnz .e820_mem_get_loop      ; ebx != 0，循环读下一个 ARDS

    ; 在所有 ARDS 结构中，找出 (base_add_low + length_low) 的最大值，即内存容量
    mov cx, [ards_nr] ; 循环次数是 ARDS 的数量
    mov ebx, ards_buf
    xor edx, edx      ; edx 为最大内存容量，先清零

.find_max_mem_area:
    mov eax, [ebx]   ; base_add_low
    add eax, [ebx+8] ; length_low
    add ebx, 20      ; 只想缓冲区中下一个 ARDS 结构
    cmp edx, eax     ; edx 保存最大内存容量
    jge .next_ards
    mov edx, eax     ; edx <= eax 则进行赋值 edx = eax
.next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok

    ; int 15h ax = E801h 获取内存大小, 最大支持 4G
.e820_failed_so_try_e801:
    mov ax, 0xe801
    int 0x15
    jc .e801_failed_so_try88 ; 若 e801 方法失败，就尝试 0x88 方法

    ; 1 计算低 15MB 的内存
    mov cx, 0x400 ; 1 KB = 0x400 字节
    mul cx        ; ax * 0x400, 乘积的高 16 位在 dx 寄存器，低 16 位在 ax 寄存器
    shl edx, 16
    and eax, 0x0000FFFF
    or edx, eax   ; (dx << 16 | eax * 0x0000FFFF) 得到完整的 32 位积（即低 15MB 内存的字节数）
    add edx, 0x100000   ; ax 只是 15MB, 故要加 1MB
    mov esi, edx        ; 先把低 15MB 的内存容量存入 esi 寄存器备份

    ; 2 计算 16MB 以上内存的字节数
    xor eax, eax
    mov ax, bx
    mov ecx, 0x10000 ; 65KB = 0x10000 字节
    mul ecx          ; eax * ecx, 积为 64 位
                     ; 高 32 位存入 edx, 低 32 位存入 eax
    add esi, eax     ; 此方法只能测出 4GB 以内的内存, 32 位 eax 已经足够
    mov edx, esi     ; edx 位总内存字节数
    jmp .mem_get_ok

    ; int 15h ah = 0x88 获取内存大小，只能获取 64MB 之内
.e801_failed_so_try88:
    mov ah, 0x88
    int 0x15
    jc .error_hlt
    and eax, 0x0000FFFF

    mov cx, 0x400 ; 1KB = 0x400 字节
    mul cx        ; cx * ax, 积的高 16 位在 dx 中，低 16 位在 ax 中
    shl edx, 16   ; 把 dx 移到高 16 位
    or edx, eax   ; 把积的低 16 位组合到 edx, 得到 32 位的积
    add edx, 0x100000 ; 0x88 子功能只会返回 1MB 以上的内存, 实际内存要加上 1MB

.error_hlt:
    hlt

.mem_get_ok:
    mov [total_mem_bytes], edx

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

    ; 加载 kernel
    mov eax, KERNEL_START_SECTOR  ; kernel.bin 所在的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR ; 从磁盘读出后，写入到 ebx 指定的地址
    mov ecx, 200                  ; 读入的扇区数

    call rd_disk_m_32

    ; 创建页目录及页表并初始化页内存位图
    call setup_page

    ; 将描述符表地址及偏移量写入内存 gdt_ptr, 一会儿用新地址重新加载
    sgdt [gdt_ptr] 

    ; 将 gdt 描述符中视频段描述符中的段基址 + 0xc0000000
    mov ebx, [gdt_ptr + 2]
    or dword [ebx + 0x18 + 4], 0xc0000000

    ; 将 gdt 的基址加上 0xc0000000 使其成为内核所在的高地址
    add dword [gdt_ptr + 2], 0xc0000000

    ; 将栈指针同样映射到内核地址
    add esp, 0xc0000000

    ; 把页目录地址赋给 cr3
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

    ; 打开 cr0 的 pg 位(第 31 位)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; 在开启分页后, 用 gdt 新的地址重新加载
    lgdt [gdt_ptr] ; 重新加载

    jmp SELECTOR_CODE:enter_kernel

enter_kernel:
    call kernel_init
    mov esp, 0xc009f000
    jmp KERNEL_ENTRY_POINT

; 创建页目录及页表
setup_page:
    mov ecx, 4096
    mov esi, 0
; 把页目录占用的空间逐字节清 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir
; 开始创建页目录项(PDE)
.create_pde:
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000 ; eax = 第一个页表的位置及属性
    mov ebx, eax

    or eax, PG_US_U | PG_RW_W | PG_P

    ; 第一个页目录项
    mov [PAGE_DIR_TABLE_POS], eax
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax ; 操作系统的虚拟地址是在 0xc0000000 以上(3GB ~ 4GB)

    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_POS + 4092], eax ; 使最后一个目录项指向页目录表自己的地址
; 创建页表项(PTR)
    mov ecx, 256 ; 1M 低端内存 / 每页大小 4k = 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte: ; 创建第一个页表的页表项(可以保存 1M 低端内存的映射关系)
    mov [ebx + esi * 4], edx ; ebx = 第一个页表的地址

    add edx, 4096
    inc esi
    loop .create_pte

; 创建内核其它页表的 PDE
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000 ; eax = 第二个页表的位置
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254 ; 范围为第 769 ~ 1022 的所有目录项数量
    mov esi, 769
.create_kernel_pde:
    mov [ebx + esi * 4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde
    ret

; 保护模式的硬盘读取函数
rd_disk_m_32:
    mov esi, eax
    mov di, cx

    mov dx, 0x1f2
    mov al, cl
    out dx, al

    mov eax, esi

    mov dx, 0x1f3
    out dx, al

    mov cl, 8
    shr eax, cl
    mov dx, 0x1f4
    out dx, al

    shr eax, cl
    mov dx, 0x1f5
    out dx, al

    shr eax, cl
    and al, 0x0f
    or al, 0xe0
    mov dx, 0x1f6
    out dx, al

    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

.not_ready:
    nop
    in al, dx
    and al, 0x88
    cmp al, 0x08
    jnz .not_ready

    mov ax, di
    mov dx, 256
    mul dx
    mov cx, ax
    mov dx, 0x1f0

.go_on_read:
    in ax, dx
    mov [ebx], ax
    add ebx, 2
    loop .go_on_read
    ret

; 将 kernel.bin 中的 segment 拷贝到编译的地址
kernel_init:
    xor eax, eax
    xor ebx, ebx ; ebx 记录程序头表地址
    xor ecx, ecx ; cx 记录程序头表中的 program header 数量
    xor edx, edx ; dx 记录 program header 尺寸，即 e_phentsize

    ; 偏移文件 42 字节处的属性是 e_phentsize, 表示 program header 大小
    mov dx, [KERNEL_BIN_BASE_ADDR + 42]
    ; 偏移文件开始部分 20 字节的地方是 e_phoff, 表示第 1 个 program header 在文件中的偏移量
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28]
    add ebx, KERNEL_BIN_BASE_ADDR
    ; 偏移文件开始部分 44 字节的地方是 e_phnum, 表示有几个 program header
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]

.each_segment:
    ; 若 p_type 等于 PT_NULL, 说明此 program header 未使用
    cmp byte [ebx], PT_NULL
    je .PTNULL

    ; 为函数 memcpy 压入参数，参数是从右往左依次压入
    ; 函数原型类似于 memcpy(dst, src, size)
    push dword [ebx + 16]         ; program header 中偏移 16 字节的地方是 p_filesz
    mov eax, [ebx + 4]            ; program header 中偏移 4 字节的地方是 p_offset
    add eax, KERNEL_BIN_BASE_ADDR ; eax = 该段的物理地址
    push eax                      ; 压入 memcpy 的第二个参数 src
    push dword [ebx + 8]          ; 压入 memcpy 的第一个参数 dst
                                  ; 偏移程序头 8 字节的位置是 p_vaddr, 这就是目的地址
    call mem_cpy                  ; 调用 mem_cpy 完成段复制
    add esp, 12                   ; 清理栈中压入的三个参数

.PTNULL:
    add ebx, edx                  ; edx 为 program header 大小，即 e_phentsize
                                  ; ebx = 下一个 program header
    loop .each_segment
    ret

; 逐字节拷贝
; 输入: 栈中三个参数(dst, src, size)
; 输出: 无
mem_cpy:
    cld
    push ebp
    mov ebp, esp
    push ecx            ; rep 指令用到了 ecx
                        ; 但 ecx 对于外层段的循环还有用，故先入栈备份
    mov edi, [ebp + 8]  ; dst
    mov esi, [ebp + 12] ; src
    mov ecx, [ebp + 16] ; size
    rep movsb           ; 逐字节拷贝

    ; 恢复环境
    pop ecx
    pop ebp
    ret