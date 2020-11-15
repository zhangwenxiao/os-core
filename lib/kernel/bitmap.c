#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

// 将位图 btmp 初始化
void bitmap_init(struct bitmap* btmp) {
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

// 判断 bit_idx 位是否为 1
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    return (btmp->bits[byte_idx]) & (BITMAP_MASK << bit_odd);
}

// 在位图中申请连续 cnt 个位, 成功, 则返回其起始位下标, 失败, 返回 -1
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
    uint32_t idx_byte = 0;
    while((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)) {
        idx_byte++;
    }

    ASSERT(idx_byte < btmp->btmp_bytes_len);
    // 该内存池找不到可用空间
    if(idx_byte == btmp->btmp_bytes_len) {
        return -1;
    }

    int idx_bit = 0;
    while((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) {
        idx_bit++;
    }

    int bit_idx_start = idx_byte * 8 + idx_bit;
    if(cnt == 1) {
        return bit_idx_start;
    }

    // 记录还有多少位可以判断
    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1; // 空闲位个数
    
    bit_idx_start = -1;
    while(bit_left-- > 0) {
        if(!(bitmap_scan_test(btmp, next_bit))) {
            count++;
        } else {
            count = 0;
        }
        // 找到连续的 cnt 个空位
        if(count == cnt) {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

// 将位图 btmp 的 bit_idx 位设置为 value
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
    ASSERT((value == 0) || (value == 1));
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;

    if(value) {
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    } else {
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}