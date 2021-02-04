#ifndef __FS_FILE_H
#define __FS_FILE_H
#include "stdint.h"
#include "ide.h"
#include "dir.h"
#include "global.h"

// 文件结构
struct file {
    uint32_t fd_pos; // 记录当前文件操作的偏移地址, 以 0 为起始, 最大为文件大小 - 1
    uint32_t fd_flag;
    struct inode* fd_inode;
};

// 标准输入输出描述符
enum std_fd {
    stdin_no,  // 0 标准输入
    stdout_no, // 1 标准输出
    stderr_no  // 2 标准错误
};

// 位图类型
enum bitmap_type {
    INODE_BITMAP, // inode 位图
    BLOCK_BITMAP  // 块位图
};

#define MAX_FILE_OPEN 32 // 系统可打开的最大文件数

#endif