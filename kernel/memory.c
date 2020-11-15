#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "debug.h"
#include "print.h"
#include "string.h"

#define PG_SIZE 4096 // 页大小(字节)

#define MEM_BITMAP_BASE 0xc009a000 // 位图地址

#define K_HEAP_START 0xc0100000 // 内核虚拟地址

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22) // 获取页目录表下标
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12) // 获取页表下标

// 内存池结构, 生成两个实例用于管理内核内存池和用户内存池
struct pool {
    struct bitmap pool_bitmap; // 位图结构, 用于管理物理内存
    uint32_t phy_addr_start; // 本内存池所管理物理内存的起始地址
    uint32_t pool_size; // 本内存池字节容量
};

struct pool kernel_pool, user_pool;
struct virtual_addr kernel_vaddr;

// 在 pf 表示的虚拟内存池中申请 pg_cnt 个虚拟页
// 成功则返回虚拟页的起始地址, 失败则返回 NULL
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if(bit_idx_start == -1) {
            return NULL;
        }
        while(cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        // TODO 用户内存池
    }
    return (void*)vaddr_start;
}

// 得到虚拟地址 vaddr 对应的 pte 指针
uint32_t* pte_ptr(uint32_t vaddr) {
    uint32_t* pte = (uint32_t*)(0xffc00000 + 
        ((vaddr & 0xffc00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}

// 得到虚拟地址 vaddr 对应的 pde 指针
uint32_t* pde_ptr(uint32_t vaddr) {
    uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

// 在 m_pool 指向的物理内存池中分配 1 个物理页
// 成功则返回页框的物理地址, 失败则返回 NULL
static void* palloc(struct pool* m_pool) {
    // 扫描或设置位图要保证原子操作
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
    if(bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

// 在页表中添加虚拟地址 _vaddr 和物理地址 _page_phyaddr 的映射
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);
    
    if(*pde & 0x00000001) { // 页目录项存在
        ASSERT(!(*pte & 0x00000001));

        if(!(*pte & 0x00000001)) {
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        } else {
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    } else { // 页目录项不存在
        // 页表中用到的页框一律从内核空间分配        
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);

        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);

        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

// 分配 pg_cnt 个页空间, 成功则返回起始虚拟地址, 失败时返回 NULL
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);

    // 1 通过 vaddr_get 在虚拟内存池中申请虚拟地址
    // 2 通过 palloc 在物理内存池中申请物理页
    // 3 通过 page_table_add 将以上得到的虚拟地址和物理地址在页表中完成映射
    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if(vaddr_start == NULL) {
        return NULL;
    }

    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    while(cnt-- > 0) {
        void* page_phyaddr = palloc(mem_pool);
        if(page_phyaddr == NULL) {
            return NULL;
        }
        page_table_add((void*)vaddr, page_phyaddr);
        vaddr += PG_SIZE; // 下一个虚拟页
    }
    return vaddr_start;
}

// 从内核物理内存池中申请 1 页内存
// 成功则返回其虚拟地址, 失败则返回 NULL
void* get_kernel_pages(uint32_t pg_cnt) {
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if(vaddr != NULL) {
        memset(vaddr, 0, pg_cnt*PG_SIZE);
    }
    return vaddr;
}

// 初始化内存池
static void mem_pool_init(uint32_t all_mem) {
    put_str("mem_pool_init start\n");
    uint32_t page_table_size = PG_SIZE * 256; // 页目录表和页表占用的字节大小
    uint32_t used_mem = page_table_size + 0x100000; // 0x100000 为低端 1MB 内存
    uint32_t free_mem = all_mem - used_mem;
    
    uint16_t all_free_pages = free_mem / PG_SIZE;
    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;
    
    uint32_t kbm_length = kernel_free_pages / 8; // 内核物理内存位图的长度
    uint32_t ubm_length = user_free_pages / 8; // 用户物理内存位图的长度

    uint32_t kp_start = used_mem; // 内核内存池的起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE; // 用户内存池的起始地址

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    kernel_pool.pool_bitmap.btmp_bytes_len = ubm_length;

    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

    // 输出内存池信息
    put_str("kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str("\n");
    put_str("kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);
    put_str("\n");
    put_str("user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str("\n");
    put_str("user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);
    put_str("\n");

    // 将位图置 0
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    // 初始化内核虚拟地址的位图
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("mem_pool_init done\n");
}

// 内存管理初始化入口
void mem_init() {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
    put_str("mem_bytes_total: 0x"); put_int(mem_bytes_total);put_str("\n");
    mem_pool_init(mem_bytes_total);
    put_str("mem_init done\n");
}