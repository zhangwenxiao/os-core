; 主引导程序
; --------------------------------------------------
%include "boot.inc"
SECTION MBR vstart=0x7c00 ; 把起始地址编译为 0x7c00
    mov ax, cs     ; cs 代码段寄存器
    mov ds, ax     ; dx 数据段寄存器
    mov es, ax     ; es 附加段寄存器
    mov ss, ax     ; ss 堆栈段寄存器
    mov fs, ax     ; fs 80386 后添加的寄存器，无全称
    mov sp, 0x7c00 ; sp 堆栈指针寄存器
    mov ax, 0xb800
    mov gs, ax     ; gs 80386 后添加的寄存器，无全称
                   ; 往 gs 中存入显存段基址
                   ; 文本模式显存的起始地址是 0xb8000

; 清屏
; --------------------------------------------------
; INT 0x10    功能号: 0x06    功能描述：上卷窗口
; --------------------------------------------------
; 输入：
; AH 功能号 = 0x06
; AL = 上卷的行数(如果为0，表示全部)
; BH = 上卷行属性
; (CL, CH) = 窗口左上角的 (X, Y) 位置
; (DL, DH) = 窗口右下角的 (X, Y) 位置
; 无返回值:
    mov ax, 0x600
    mov bx, 0x700
    mov cx, 0
    mov dx, 0x184f ; 右下角: (80, 25)
                   ; VGA 文本模式种，一行只能容纳 80 个字符，共 25 行
                   ; 下标从 0 开始，所以 0x18=24, 0x4f=79
        
    int 0x10       ; int 0x10

    mov eax, LOADER_START_SECTOR ; loader 的起始扇区 LBA 地址
    mov bx, LOADER_BASE_ADDR     ; 写入的地址
    mov cx, 4                    ; 待读入的扇区数
    call rd_disk_m_16            ; 以下读取程序的起始部分（一个扇区）

    jmp LOADER_BASE_ADDR + 0x300

; 读取硬盘 n 个扇区
rd_disk_m_16:
                                 ; eax = LBA 扇区号
                                 ; bx = 将数据写入的内存地址
                                 ; cx = 读入的扇区数
    mov esi, eax    ; 备份 eax
    mov di, cx      ; 备份 cx

    ; 1. 设置要读取的扇区数
    mov dx, 0x1f2
    mov al, cl
    out dx, al      ; 读取的扇区数

    mov eax, esi    ; 恢复 ax

    ; 2. 将 LBA 地址存入 0x1f3 ~ 0x1f6
    
    ; LBA 地址 7 ~ 0 位写入端口 0x1f3
    mov dx, 0x1f3
    out dx, al

    ; LBA 地址 15 ~ 8 位写入端口 0x1f4
    mov cl, 8
    shr eax, cl
    mov dx, 0x1f4
    out dx, al

    ; LBA 地址 23 ~ 16 位写入端口 0x1f5
    shr eax, cl
    mov dx, 0x1f5
    out dx, al

    shr eax, cl
    and al, 0x0f ; LBA 地址的第 24 ~ 17 位
    or al, 0xe0  ; 设置 7 ~ 4 位为 1110，表示 LBA 模式
    mov dx, 0x1f6
    out dx, al

    ; 3. 向 0x1f7 端口写入读命令，0x20
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

    ; 4. 检测硬盘状态
    .not_ready:
        ; 从 Status 寄存器读硬盘状态
        nop
        in al, dx
        and al, 0x88   ; 第 3 位为 1 表示硬盘控制器已准备好数据传输
                       ; 第 7 位为 1 表示硬盘忙
        cmp al, 0x08
        jnz .not_ready ; 若硬盘未准备好，继续等
    
    ; 5. 从 0x1f0 端口读数据
    mov ax, di     ; di = 要读入的扇区数
    mov dx, 256    ; 一个扇区有 512 个字节，ax 寄存器宽度是 2 字节
                   ; 一共需要读 di * 512 / 2 = di * 256 次
    mul dx         ; ax = ax * dx
    mov cx, ax     ; cx = 读硬盘次数

    mov dx, 0x1f0

    .go_on_read:
        in ax, dx
        mov [bx], ax     ; bx=loader将被写到的内存地址
        add bx, 2        ; ax 的宽度是两字节，内存地址需要往后移两个字节
        loop .go_on_read 
        ret


times 510-($-$$) db 0
db 0x55, 0xaa