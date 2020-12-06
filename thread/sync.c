#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"

// 初始化信号量
void sema_init(struct semaphore* psema, uint8_t value) {
    psema->value = value;
    list_init(&psema->waiters);
}

// 初始化锁 plock
void lock_init(struct lock* plock) {
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1); // 锁的信号量初值为 1
}

// 信号量 down 操作
void sema_down(struct semaphore* psema) {
    // 关中断来保证原子操作
    enum intr_status old_status = intr_disable();
    while(psema->value == 0) { // value 为0, 表示已经被别人持有
        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
        if(elem_find(&psema->waiters, &running_thread()->general_tag)) {
            PANIC("sema_down: thread blocked has been in waiters_list\n");
        }
        // 若信号量等于 0, 则当前线程把自己加入该锁的等待队列, 然后阻塞自己
        list_append(&psema->waiters, &running_thread()->general_tag);
        thread_block(TASK_BLOCKED); // 阻塞线程, 直到被唤醒
    }
    // 若 value 为 1 或被唤醒后, 会执行下面的代码, 也就是获得了锁
    psema->value--;
    ASSERT(psema->value==0);
    intr_set_status(old_status);
}

// 信号量 up 操作
void sema_up(struct semaphore* psema) {
    // 关中断保证原子操作
    enum intr_status old_status = intr_disable();
    ASSERT(psema->value == 0);
    if(!list_empty(&psema->waiters)) {
        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_status(old_status);
}

// 获取锁 plock
void lock_acquire(struct lock* plock) {
    // 排除曾经自己已经持有锁但还未将其释放的情况
    if(plock->holder != running_thread()) {
        sema_down(&plock->semaphore);
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    } else {
        plock->holder_repeat_nr++;
    }
}

// 释放锁 plock
void lock_release(struct lock* plock) {
    ASSERT(plock->holder == running_thread());
    if(plock->holder_repeat_nr > 1) {
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);
}