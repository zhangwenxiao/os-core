#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
    put_str("I am kernel\n");
    init_all();

    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    intr_enable(); // 打开中断
    console_put_str(" main_pid:0x"); console_put_int(sys_getpid()); console_put_char('\n');

    thread_start("consumer_a", 31, k_thread_a, " A_");
    thread_start("consumer_b", 31, k_thread_b, " B_");
  
    while(1);
    return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {   
    char* para = arg;
    console_put_str(" thread_a_pid:0x"); console_put_int(sys_getpid()); console_put_char('\n');
    while(1);
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {
    char* para = arg;
    console_put_str(" thread_b_pid:0x"); console_put_int(sys_getpid()); console_put_char('\n');
    while(1);
}

// 测试用户进程
void u_prog_a(void) {
    char* name = "prog_a";
    printf(" I am %s, my pid: %d%c", name, getpid(), '\n');
    while(1);
}

// 测试用户进程
void u_prog_b(void) {
    char* name = "prog_b";
    printf(" I am %s, my pid: %d%c", name, getpid(), '\n');
    while(1);
}
