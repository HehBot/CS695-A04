#ifndef COMMON_H
#define COMMON_H

#include "spinlock.h"
#include "types.h"

struct queue_entry {
    void* data;
    size_t size;
    struct queue_entry* next;
};

struct queue_head {
    struct queue_entry* next;
    struct spinlock next_lock;
    struct queue_entry* tail;
    struct spinlock tail_lock;
};

#endif // COMMON_H
