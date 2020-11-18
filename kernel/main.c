#include "print.h"
#include "init.h"
#include "memory.h"
#include "thread.h"

void k_thread_a(void*);

int main(void) {
    put_str("I am kernel\n");
    init_all();

    void* addr = get_kernel_pages(3);
    put_str("\nget_kernel_page start vaddr is ");
    put_int((uint32_t)addr);
    put_str("\n");

    thread_start("k_thread_a", 31, k_thread_a, "argA ");

    while(1);
    return 0;
}

// 在线程中运行的函数
void k_thread_a(void* arg) {
    char* para = arg;
    while(1) {
        put_str(para);
    }
}
