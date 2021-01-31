#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "list.h"

// inode 结构
struct inode {
    uint32_t i_no; // inode 编号
    uint32_t i_size;
    uint32_t i_open_cnts; // 记录此文件被打开的次数
    bool write_deny; // 写文件不能并行, 进程写文件前检查此标识

    // i_sectors[0-11]是直接块, i_sectors[13]用来存储一级间接块指针
    uint32_t i_sectors[13];
    struct list_elem inode_tag; // 用于加入已打开的 inode 队列
};

#endif