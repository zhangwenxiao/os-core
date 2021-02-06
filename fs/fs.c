#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "stdint.h"
#include "stdio-kernel.h"
#include "list.h"
#include "string.h"
#include "ide.h"
#include "global.h"
#include "debug.h"
#include "memory.h"

struct partition* cur_part; // 默认情况下操作的是哪个分区

// 在分区链表中找到名为 part_name 的分区, 并将其指针赋值给 cur_part
static bool mount_partition(struct list_elem* pelem, int arg) {
    char* part_name = (char*)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    if (!strcmp(part->name, part_name)) {
        cur_part = part;
        struct disk* hd = cur_part->my_disk;

        // sb_buf 用来存储从硬盘上读入的超级块
        struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

        // 在内存中创建分区 cur_part 的超级块
        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
        if (cur_part->sb == NULL) {
            PANIC("alloc memory failed!");
        }

        // 读入超级块
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, cur_part->start_lba+1, sb_buf, 1);

        // 把 sb_buf 中超级块的信息复制到分区的超级块 sb 中
        memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

        // 将硬盘上的块位图读入到内存
        cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects*SECTOR_SIZE);
        if (cur_part->block_bitmap.bits == NULL) {
            PANIC("alloc memory failed!");
        }
        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;
        // 从硬盘上读入块位图到分区的 block_bitmap.bits
        ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);

        list_init(&cur_part->open_inodes);
        printk("mount %s done!\n", part->name);

        return true; // 使 list_traversal 停止遍历
    }
    return false; // 使 list_traversal 继续遍历
}

// 格式化分区, 也就是初始化分区的元信息, 创建文件系统
static void partition_format(struct partition* part) {
    // 为方便实现, 一个块大小是一扇区
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;
    // I 结点位图占用的扇区数, 最多支持 4096 个文件
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    uint32_t inode_table_sects = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);
    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = part->sec_cnt - used_sects;

    // 简单处理块位图占据的扇区数
    uint32_t block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    // block_bitmap_bit_len 位图中位的长度, 可用块的数量
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    // 超级块初始化
    struct super_block sb;
    sb.magic = 0x19590318;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;
    // 第 0 块是引导块, 第 1 块是超级块
    sb.block_bitmap_lba = sb.part_lba_base + 2;
    sb.block_bitmap_sects = block_bitmap_sects;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("%s info:\n", part->name);
    printk("   magic:0x%x\n", sb.magic);
    printk("   part_lba_base:0x%x\n", sb.part_lba_base);
    printk("   all_sectors:0x%x\n", sb.sec_cnt);
    printk("   inode_cnt:0x%x\n", sb.inode_cnt);
    printk("   block_bitmap_lba:0x%x\n", sb.block_bitmap_lba);
    printk("   block_bitmap_sectors:0x%x\n", sb.block_bitmap_sects);
    printk("   inode_bitmap_lba:0x%x\n", sb.inode_bitmap_lba);
    printk("   inode_bitmap_sectors:0x%x\n", sb.inode_bitmap_sects);
    printk("   inode_table_lba:0x%x\n", sb.inode_table_lba);
    printk("   inode_table_sectors:0x%x\n", sb.inode_table_sects);
    printk("   data_start_lba:0x%x\n", sb.data_start_lba);

    struct disk* hd = part->my_disk;
// 1 将超级块写入本分区的 1 扇区
    ide_write(hd, part->start_lba+1, &sb, 1);
    printk("    super_block_lba:0x%x\n", part->start_lba+1);

    // 找出数据量最大的元信息, 用其尺寸做存储缓冲区
    uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
    buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

// 2 将块位图初始化并写入 sb.block_bitmap_lba
    // 初始化块位图 block_bitmap
    buf[0] |= 0x01; // 第 1 个块预留给根目录, 位图中先占位
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
    // last_size 是位图所在最后一个扇区中, 不足一扇区的其余部分
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);
    // 1 先将位图最后一字节到其所在的扇区的结束全置为 1, 即超出实际块数的部分直接置为已占用
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);
    // 2 再将上一步中覆盖的最后一字节内的有效位重新置 0
    uint8_t bit_idx = 0;
    while (bit_idx <= block_bitmap_last_bit) {
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    }
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

// 3 将 inode 位图初始化并写入 sb.inode_bitmap_lba
    // 先清空缓冲区
    memset(buf, 0, buf_size);
    buf[0] |= 0x1; // 第 0 个 inode 分给了根目录
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

// 4 将 inode 数组初始化并写入 sb.inode_table_lba
    // 准备写 inode_table 中的第 0 项, 即根目录所在的 inode
    memset(buf, 0, buf_size);
    struct inode* i = (struct inode*)buf;
    i->i_size = sb.dir_entry_size * 2; // . 和 ..
    i->i_no = 0; // 根目录占 inode 数组中第 0 个 inode
    i->i_sectors[0] = sb.data_start_lba;
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

// 5 将根目录写入 sb.data_start_lba
    // 写入根目录的两个目录项 . 和 ..
    memset(buf, 0, buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    // 初始化当前目录 .
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;

    // 初始化当前目录父目录 ..
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0; // 根目录的父目录依然是根目录自己
    p_de->f_type = FT_DIRECTORY;
    // sb.data_start_lba 已经分配给了根目录, 里面是根目录的目录项
    ide_write(hd, sb.data_start_lba, buf, 1);

    printk("    root_dir_lba:0x%x\n", sb.data_start_lba);
    printk("%s format done\n", part->name);
    sys_free(buf);
}

// 将最上层路径名称解析出来
static char* path_parse(char* pathname, char* name_store) {
    // 根目录不需要单独解析
    if (pathname[0] == '/') {
        // 路径中出现 1 个或多个连续的字符 '/', 将这些 '/' 跳过
        while (*(++pathname) == '/');
    }

    // 开始一般的路径解析
    while (*pathname != '/' && *pathname != 0) {
        *name_store++ = *pathname++;
    }

    if (pathname[0] == 0) {
        return NULL;
    }

    return pathname;
}

// 返回路径深度, 比如 /a/b/c, 深度为 3
int32_t path_depth_cnt(char* pathname) {
    ASSERT(pathname != NULL);
    char* p = pathname;
    char name[MAX_FILE_NAME_LEN];
    uint32_t depth = 0;

    // 解析路径, 从中拆分出各级名称
    p = path_parse(p, name);
    while (name[0]) {
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (p) {
            p = path_parse(p, name);
        }
    }
    return depth;
}

// 搜索文件 pathname, 若找到则返回其 inode 号, 否则返回 -1
static int search_file(const char* pathname, struct path_search_record* searched_record) {
    // 如果待查找的是根目录, 为避免下面无用的查找, 直接返回已知根目录信息
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || 
        !strcmp(pathname, "/..")) {
        searched_record->parent_dir = &root_dir;
        searched_record->file_type = FT_DIRECTORY;
        searched_record->searched_path[0] = 0; // 搜索路径置空
        return 0;
    }

    uint32_t path_len = strlen(pathname);
    // 保证 pathname 至少是这样的路径 /x, 且小于最大长度
    ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
    char* sub_path = (char*)pathname;
    struct dir* parent_dir = &root_dir;
    struct dir_entry dir_e;

    // 记录路径解析出来的各级名称
    char name[MAX_FILE_NAME_LEN] = {0};

    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;
    uint32_t parent_inode_no = 0; // 父目录的 inode 号

    sub_path = path_parse(sub_path, name);
    while (name[0]) { // 若第一个字符就是结束符, 结束循环
        // 记录查找过的路径, 但不能超过 searched_path 的长度 512 字节
        ASSERT(strlen(searched_record->searched_path) < 512);

        //  记录已存在的父目录
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);

        // 在所给的目录中查找文件
        if (search_dir_entry(cur_part, parent_dir, name, &dir_e)) {
            memset(name, 0, MAX_FILE_NAME_LEN);
            // 若 sub_path 不等于 NULL, 也就是未结束时继续拆分路径
            if (sub_path) {
                sub_path = path_parse(sub_path, name);
            }

            // 如果被打开的是目录
            if (FT_DIRECTORY == dir_e.f_type) {
                parent_inode_no = parent_dir->inode->i_no;
                dir_close(parent_dir);
                parent_dir = dir_open(cur_part, dir_e.i_no); // 更新父目录
                searched_record->parent_dir = parent_dir;
                continue;
            } else if (FT_REGULAR == dir_e.f_type) { // 若是普通文件
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
        } else { // 若找不到, 则返回 -1
            return -1;
        }
    }

    // 执行到此, 必然是遍历了完整路径并且查找的文件或目录只有同名目录存在
    dir_close(searched_record->parent_dir);

    // 保存被查找目录的直接父目录
    searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}

// 在磁盘上搜索文件系统, 若没有则格式化分区创建文件系统
void filesys_init() {
    uint8_t channel_no = 0, dev_no, part_idx = 0;

    // sb_buf 用来存储从硬盘上读入的超级块
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

    if (sb_buf == NULL) {
        PANIC("alloc memory failed!");
    }
    printk("searching filesystem......\n");
    while (channel_no < channel_cnt) {
        dev_no = 0;
        while (dev_no < 2) {
            if (dev_no == 0) { // 跳过裸盘 hd60M.img
                dev_no++;
                continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* part = hd->prim_parts;
            while (part_idx < 12) { // 4 个主分区 + 8 个逻辑分区
                if (part_idx == 4) { // 开始处理逻辑分区
                    part = hd->logic_parts;
                }
                if (part->sec_cnt != 0) { // 如果分区存在
                    memset(sb_buf, 0, SECTOR_SIZE);
                    // 读出分区的超级块, 根据魔数是否正确来判断是否存在文件系统
                    ide_read(hd, part->start_lba+1, sb_buf, 1);

                    if (sb_buf->magic == 0x19590318) {
                        printk("%s has filesystem\n", part->name);
                    } else { // 其它文件系统不支持, 一律按无文件系统处理
                        printk("formatting %s's partition %s......\n",
                            hd->name, part->name);
                        partition_format(part);
                    }
                }
                part_idx++;
                part++; // 下一分区
            }
            dev_no++; // 下一磁盘
        }
        channel_no++; // 下一通道
    }
    sys_free(sb_buf);

    // 确定默认操作的分区
    char default_part[8] = "sdb1";
    // 挂载分区
    list_traversal(&partition_list, mount_partition, (int)default_part);
}