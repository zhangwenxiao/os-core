#include "assert.h"
#include "console.h"
#include "dir.h"
#include "fs.h"
#include "init.h"
#include "interrupt.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "shell.h"
#include "stdio-kernel.h"
#include "stdio.h"
#include "syscall-init.h"
#include "syscall.h"
#include "thread.h"

void init(void);

int main(void) {
    put_str("I am kernel\n");
    init_all();

// 写入应用程序
    uint32_t file_size = 5352;
    uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
    struct disk* sda = &channels[0].devices[0];
    void* prog_buf = sys_malloc(file_size);
    ide_read(sda, 300, prog_buf, sec_cnt);
    int32_t fd = sys_open("/cat", O_CREAT|O_RDWR);
    if (fd != -1) {
        if (sys_write(fd, prog_buf, file_size) == -1) {
            printk("file write error!\n");
            while(1);
        }
    }
// 写入应用程序结束

    cls_screen();
    console_put_str("[swings@localhost /]$ ");

    thread_exit(running_thread(), true);
    return 0;
}

// init 进程
void init(void) {
    uint32_t ret_pid = fork();
    if (ret_pid) {
        int status;
        int child_pid;
        // init 在此处不停地回收僵尸进程
        while(1) {
            child_pid = wait(&status);
            printf("I'm init, my pid is 1, I receive a child, it's pid is %d, status is %d\n", child_pid, status);
        }
    } else {
        my_shell();
    }
    panic("init: should not be here");
}
