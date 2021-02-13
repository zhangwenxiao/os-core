#include "shell.h"
#include "stdint.h"
#include "fs.h"
#include "file.h"
#include "syscall.h"
#include "stdio.h"
#include "global.h"
#include "assert.h"
#include "string.h"

#define cmd_len 128   // 最大支持键入 128 个字符的命令行输入
#define MAX_ARG_NR 16 // 加上命令名外, 最多支持 15 个参数

// 存储输入的命令
static char cmd_line[cmd_len] = {0};

// 用来记录当前目录, 是当前目录的缓存, 每次执行 cd 命令时会更新此内容
char cwd_cache[64] = {0};

// 输出提示符
void print_prompt(void) {
    printf("[swings@localhost %s]$ ", cwd_cache);
}

// 从键盘缓冲区中最多读入 count 个字节到 buf
static void readline(char* buf, int32_t count) {
    assert(buf != NULL && count > 0);
    char* pos = buf;
    while (read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {
        switch (*pos) {
        // 找到回车或换行符后认为键入的命令结束, 直接返回
        case '\n':
        case '\r':
            *pos = 0; // 添加 cmd_line 的终止字符 0
            putchar('\n');
            return;
        case '\b':
            // 阻止删除非本次输入的信息
            if (buf[0] != '\b') {
                --pos;
                putchar('\b');
            }
            break;
        default:
            putchar(*pos);
            pos++;
        }
    }
    printf("readline: can`t find enter_key in the cmd_line, max num of char is 128");
}

// 简单的 shell
void my_shell(void) {
    cwd_cache[0] = '/';
    while (1) {
        print_prompt();
        memset(cmd_line, 0, cmd_len);
        readline(cmd_line, cmd_len);
        // 若只键入了一个回车
        if (cmd_line[0] == 0) {
            continue;
        }
    }
    panic("my_shell: should not be here");
}
