[bits 32]
section .text
global switch_to
switch_to:
    ; 备份当前线程的环境
    push esi
    push edi
    push ebx
    push ebp

    mov eax, [esp+20] ; 获取栈中参数 cur
    mov [eax], esp    ; 保存栈顶指针 esp 到 task_struct 的 self_kstack 字段
                      ; self_kstack 在 task_struct 中的偏移为 0
                      ; 所以直接往 thread 开头处存 4 字节即可
    ; 恢复下一个线程的环境
    mov eax, [esp+24] ; 获取栈中参数 next
    mov esp, [eax]    ; pcb 的第一个成员是 self_kstack 成员
                      ; 它用来记录 0 级栈顶指针, 被换上 cpu 时用来恢复 0 级栈
                      ; 0 级栈中保存了进程或线程所有信息, 包括 3 级栈指针
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret