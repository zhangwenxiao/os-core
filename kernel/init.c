#include "init.h"
#include "print.h"
#include "memory.h"
#include "interrupt.h"
#include "timer.h"
#include "thread.h"

// 初始化所有模块
void init_all() {
    put_str("init_all\n");
    idt_init(); // 初始化中断
    thread_init(); // 初始化线程相关结构
    mem_init(); // 初始化内存管理系统
    timer_init(); // 初始化 PIT
}