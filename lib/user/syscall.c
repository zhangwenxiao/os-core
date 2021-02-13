#include "syscall.h"
#include "thread.h"

/* 无参数的系统调用 */
#define _syscall0(NUMBER) ({				       \
   int retval;					               \
   asm volatile (					       \
   "int $0x80"						       \
   : "=a" (retval)					       \
   : "a" (NUMBER)					       \
   : "memory"						       \
   );							       \
   retval;						       \
})

/* 一个参数的系统调用 */
#define _syscall1(NUMBER, ARG1) ({			       \
   int retval;					               \
   asm volatile (					       \
   "int $0x80"						       \
   : "=a" (retval)					       \
   : "a" (NUMBER), "b" (ARG1)				       \
   : "memory"						       \
   );							       \
   retval;						       \
})

/* 两个参数的系统调用 */
#define _syscall2(NUMBER, ARG1, ARG2) ({		       \
   int retval;						       \
   asm volatile (					       \
   "int $0x80"						       \
   : "=a" (retval)					       \
   : "a" (NUMBER), "b" (ARG1), "c" (ARG2)		       \
   : "memory"						       \
   );							       \
   retval;						       \
})

/* 三个参数的系统调用 */
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({		       \
   int retval;						       \
   asm volatile (					       \
      "int $0x80"					       \
      : "=a" (retval)					       \
      : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3)       \
      : "memory"					       \
   );							       \
   retval;						       \
})

/* 返回当前任务pid */
uint32_t getpid() {
   return _syscall0(SYS_GETPID);
}

/* 打印字符串str */
uint32_t write(int32_t fd, const void* buf, uint32_t count) {
   return _syscall3(SYS_WRITE, fd, buf, count);
}

// 申请 size 字节大小的内存, 并返回结果
void* malloc(uint32_t size) {
   return (void*)_syscall1(SYS_MALLOC, size);
}

// 释放 ptr 指向的内存
void free(void* ptr) {
   _syscall1(SYS_FREE, ptr);
}

// 派生子进程, 返回子进程 pid
pid_t fork(void) {
    return _syscall0(SYS_FORK);
}

// 从文件描述符 fd 中读取 count 个字节到 buf
int32_t read(int32_t fd, void* buf, uint32_t count) {
    return _syscall3(SYS_READ, fd, buf, count);
}

// 输出一个字符
void putchar(char char_asci) {
    _syscall1(SYS_PUTCHAR, char_asci);
}

// 清空屏幕
void clear(void) {
    _syscall0(SYS_CLEAR);
}
