#include "debug.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "inode.h"
#include "inode.h"
#include "interrupt.h"
#include "memory.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"
#include "thread.h"

// 文件表
struct file file_table[MAX_FILE_OPEN];

// 从文件表 file_table 中获取一个空闲位, 成功返回下标, 失败返回 -1
int32_t get_free_slot_in_global(void) {
    uint32_t fd_idx = 3;
    while (fd_idx < MAX_FILE_OPEN) {
        if (file_table[fd_idx].fd_inode == NULL) {
            break;
        }
        fd_idx++;
    }
    if (fd_idx == MAX_FILE_OPEN) {
        printk("exceed max open files\n");
        return -1;
    }
    return fd_idx;
}

// 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中
// 成功返回下标, 失败返回 -1
int32_t pcb_fd_install(int32_t globa_fd_idx) {
    struct task_struct* cur = running_thread();
    uint8_t local_fd_idx = 3;
    while (local_fd_idx < MAX_FILES_OPEN_PER_PROC) {
        if (cur->fd_table[local_fd_idx] == -1) {
            cur->fd_table[local_fd_idx] = globa_fd_idx;
            break;
        }
        local_fd_idx++;
    }
    if (local_fd_idx == MAX_FILES_OPEN_PER_PROC) {
        printk("exceed max open files_per_proc\n");
        return -1;
    }
    return local_fd_idx;
}

// 分配一个 inode, 返回 inode 号
int32_t inode_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->inode_bitmap, bit_idx, 1);
    return bit_idx;
}

// 分配 1 个扇区, 返回其扇区地址
int32_t block_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
    if (bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->block_bitmap, bit_idx, 1);
    // 和 inode_bitmap_malloc 不同, 此处返回的不是位索引
    // 而是具体可用的扇区地址
    return (part->sb->data_start_lba + bit_idx);
}

// 将内存中 bitmap 第 bit_idx 位所在的 512 字节同步到硬盘
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp) {
    // 本 inode 索引相对于位图的扇区偏移量
    uint32_t off_sec = bit_idx / 4096;
    // 本 inode 索引相对于位图的字节偏移量
    uint32_t off_size = off_sec * BLOCK_SIZE;
    uint32_t sec_lba;
    uint8_t* bitmap_off;

    switch (btmp) {
        case INODE_BITMAP:
            sec_lba = part->sb->inode_bitmap_lba + off_sec;
            bitmap_off = part->inode_bitmap.bits + off_size;
            break;
        case BLOCK_BITMAP:
            sec_lba = part->sb->block_bitmap_lba + off_sec;
            bitmap_off = part->block_bitmap.bits + off_size;
            break;
    }
    ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

// 创建文件, 若成功则返回文件描述符, 否则返回 -1
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {
    // 后续操作的公共缓冲区
    void* io_buf = sys_malloc(1024);
    if (io_buf == NULL) {
        printk("in file_creat: sys_malloc for io_buf failed\n");
        return -1;
    }

    uint8_t rollback_step = 0; // 用于操作失败时回滚各资源状态

    // 为新文件分配 inode
    int32_t inode_no = inode_bitmap_alloc(cur_part);
    if (inode_no == -1) {
        printk("in file_creat: allocate inode failed\n");
        return -1;
    }

    // 此 inode 要从堆中申请内存, 不可生成局部变量(函数退出时会释放)
    // 因为 file_table 数组中的文件描述符的 inode 指针要指向它
    struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
    if (new_file_inode == NULL) {
        printk("file_create: sys_malloc for inode failed\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode); // 初始化 inode

    // 返回的是 file_table 数组的下标
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
        printk("exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }

    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_inode->write_deny = false;

    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));

    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

    // 同步内存数据到硬盘
    // a 在目录 parent_dir 下安装目录项 new_dir_entry
    // 写入硬盘后返回 true, 否则 false
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
        printk("sync dir_entry to disk failed\n");
        rollback_step = 3;
        goto rollback;
    }

    memset(io_buf, 0, 1024);
    // b 将父目录 inode 的内容同步到硬盘
    inode_sync(cur_part, parent_dir->inode, io_buf);

    memset(io_buf, 0, 1024);
    // c 将新创建文件的 inode 内容同步到硬盘
    inode_sync(cur_part, new_file_inode, io_buf);

    // d 将 inode_bitmap 位图同步到硬盘
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    // e 将创建的文件 inode 添加到 open_inodes 链表
    list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnts = 1;

    sys_free(io_buf);
    return pcb_fd_install(fd_idx);

// 创建文件需要创建相关的多个资源
// 若某步失败则会执行到下面的回滚步骤
rollback:
    switch (rollback_step) {
        case 3:
            // 失败时, 将 file_table 中的相应位清空
            memset(&file_table[fd_idx], 0, sizeof(struct file));
        case 2:
            sys_free(new_file_inode);
        case 1:
            // 如果新文件的 inode 创建失败
            // 之前位图中分配的 inode_no 也要恢复
            bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
            break;
    }
    sys_free(io_buf);
    return -1;
}

// 打开编号为 inode_no 的 inode 对应的文件, 若成功则返回文件描述符, 否则返回 -1
int32_t file_open(uint32_t inode_no, uint8_t flag) {
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
        printk("exceed max open files\n");
        return -1;
    }
    file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
    file_table[fd_idx].fd_pos = 0; // 每次打开文件, 要将 fd_pos 还原为 0, 即让文件内的指针指向开头
    file_table[fd_idx].fd_flag = flag;
    bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;

    // 只要是关于写文件, 判断是否有其它进程正写此文件
    if (flag & O_WRONLY || flag & O_RDWR) {
        // 进入临界区要关中断
        enum intr_status old_status = intr_disable();
        // 若当前没有其它进程写该文件, 将其占用
        if (!(*write_deny)) {
            *write_deny = true; // 置为 true, 避免多个进程同时写此文件
            intr_set_status(old_status);
        } else {
            intr_set_status(old_status);
            printk("file can`t be write now, try again later\n");
            return -1;
        }
    }
    return pcb_fd_install(fd_idx);
}

// 关闭文件
int32_t file_close(struct file* file) {
    if (file == NULL) {
        return -1;
    }
    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL; // 使文件结构可用
    return 0;
}
