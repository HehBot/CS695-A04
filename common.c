// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "common.h"
#include "defs.h"
#include "types.h"

#define isascii(x) ((x >= 0x00) && (x <= 0x7f))
#define isprint(x) ((x >= 0x20) && (x <= 0x7e))

void hexdump(void* data, size_t size)
{
    int offset, index;
    unsigned char* src;

    src = (unsigned char*)data;
    cprintf("+------+-------------------------------------------------+------------------+\n");
    for (offset = 0; offset < (int)size; offset += 16) {
        cprintf("| %04x | ", offset);
        for (index = 0; index < 16; index++) {
            if (offset + index < (int)size) {
                cprintf("%02x ", 0xff & src[offset + index]);
            } else {
                cprintf("   ");
            }
        }
        cprintf("| ");
        for (index = 0; index < 16; index++) {
            if (offset + index < (int)size) {
                if (isascii(src[offset + index]) && isprint(src[offset + index])) {
                    cprintf("%c", src[offset + index]);
                } else {
                    cprintf(".");
                }
            } else {
                cprintf(" ");
            }
        }
        cprintf(" |\n");
    }
    cprintf("+------+-------------------------------------------------+------------------+\n");
}

#ifndef __BIG_ENDIAN
    #define __BIG_ENDIAN 4321
#endif
#ifndef __LITTLE_ENDIAN
    #define __LITTLE_ENDIAN 1234
#endif

static int endian;

static int
byteorder(void)
{
    uint32_t x = 0x00000001;
    return *(uint8_t*)&x ? __LITTLE_ENDIAN : __BIG_ENDIAN;
}

static uint16_t
byteswap16(uint16_t v)
{
    return (v & 0x00ff) << 8 | (v & 0xff00) >> 8;
}

static uint32_t
byteswap32(uint32_t v)
{
    return (v & 0x000000ff) << 24 | (v & 0x0000ff00) << 8 | (v & 0x00ff0000) >> 8 | (v & 0xff000000) >> 24;
}

uint16_t
hton16(uint16_t h)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap16(h) : h;
}

uint16_t
ntoh16(uint16_t n)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap16(n) : n;
}

uint32_t
hton32(uint32_t h)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap32(h) : h;
}

uint32_t
ntoh32(uint32_t n)
{
    if (!endian)
        endian = byteorder();
    return endian == __LITTLE_ENDIAN ? byteswap32(n) : n;
}

uint16_t
cksum16(uint16_t const* data, uint16_t size, uint32_t init)
{
    uint32_t sum;

    sum = init;
    while (size > 1) {
        sum += *(data++);
        size -= 2;
    }
    if (size) {
        sum += *(uint8_t*)data;
    }
    while (sum >> 16)
        sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);
    return ~(uint16_t)sum;
}
uint16_t recompute_cksum16(uint16_t old_checksum, uint32_t old_addr, uint32_t new_addr)
{
    uint16_t old_s1 = (old_addr & 0xffff);
    uint16_t old_s2 = ((old_addr >> 16) & 0xffff);
    uint32_t sum = (uint32_t)~old_checksum - old_s1 - old_s2;
    while (sum >> 16)
        sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);

    uint16_t new_s1 = new_addr & 0xffff;
    uint16_t new_s2 = (new_addr >> 16) & 0xffff;
    sum = sum + new_s1 + new_s2;
    while (sum >> 16)
        sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);
    sum = (uint16_t)~sum;
    return sum;
}

void init_queue(struct queue_head* queue)
{
    queue->next = queue->tail = NULL;
    initlock(&queue->next_lock, "queue->next_lock");
    initlock(&queue->tail_lock, "queue->tail_lock");
}

struct queue_entry*
queue_push(struct queue_head* queue, void* data, size_t size)
{
    if (queue == NULL)
        return NULL;

    struct queue_entry* entry = (struct queue_entry*)kalloc();
    if (!entry) {
        return NULL;
    }
    entry->data = data;
    entry->size = size;
    entry->next = NULL;

    acquire(&queue->next_lock);
    acquire(&queue->tail_lock);
    if (queue->tail) {
        queue->tail->next = entry;
    }
    queue->tail = entry;
    release(&queue->tail_lock);
    if (!queue->next) {
        queue->next = entry;
    }
    release(&queue->next_lock);
#ifdef DEBUG
    cprintf("queue %p push %p\n", queue, entry);
#endif
    return entry;
}

struct queue_entry*
queue_pop(struct queue_head* queue)
{
    if (queue == NULL)
        return NULL;

    if (!tryacquire(&queue->next_lock))
        return NULL;
    struct queue_entry* entry = queue->next;

    if (entry == NULL) {
        release(&queue->next_lock);
        return NULL;
    }

    queue->next = entry->next;
    if (!queue->next) {
        acquire(&queue->tail_lock);
        queue->tail = NULL;
        release(&queue->tail_lock);
    }
    release(&queue->next_lock);
#ifdef DEBUG
    cprintf("queue %p pop %p\n", queue, entry);
#endif
    return entry;
}

time_t
time(time_t* t)
{
    time_t tmp;
    acquire(&tickslock);
    tmp = ticks / 100;
    release(&tickslock);
    if (t)
        *t = tmp;
    return tmp;
}

unsigned long
random(void)
{
    static int initialized = 0;

    if (!initialized) {
        acquire(&tickslock);
        if (!initialized) {
            initialized = 1;
            init_genrand(ticks);
        }
        release(&tickslock);
    }
    return genrand_int32();
}
