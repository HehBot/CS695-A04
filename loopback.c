#include "common.h"
#include "defs.h"
#include "net.h"
#include "proc.h"
#include "types.h"

struct lo_frame {
    uint16_t type;
    uint8_t packet[];
};

struct lo {
    struct queue_head frame_queue;
    struct spinlock lock;
};

static int lo_open(struct netdev* dev)
{
    dev->flags |= NETDEV_FLAG_UP;
    return 0;
}
static int lo_stop(struct netdev* dev)
{
    dev->flags &= ~NETDEV_FLAG_UP;
    return 0;
}

static int lo_tx(struct netdev* dev, uint16_t type, const uint8_t* packet, size_t size, const void* dst)
{
    struct lo_frame* f = (void*)kalloc();
    f->type = hton16(type);
    memcpy(&f->packet[0], packet, size);
    queue_push(&((struct lo*)dev->priv)->frame_queue, f, size);
    return size;
}
void lo_intr_handle(struct netdev* netdev)
{
    struct lo* lo = (struct lo*)netdev->priv;

    // it's possible multiple CPUs reach here at the same time
    // when only one is needed to clear the queue
    if (!tryacquire(&lo->lock))
        return;

    struct queue_entry* e;
    while ((e = queue_pop(&lo->frame_queue)) != NULL) {
        struct lo_frame* f = e->data;
        size_t size = e->size;
        kfree((void*)e);
        netdev_receive(netdev, f->type, &f->packet[0], size);
        kfree((void*)f);
    }

    release(&lo->lock);
}

static struct netdev_ops lo_ops = {
    .open = lo_open,
    .stop = lo_stop,
    .xmit = lo_tx,
};

static void lo_init_helper(struct netdev* d)
{
    d->type = NETDEV_TYPE_LOOPBACK;
    d->mtu = 1000;
    d->flags = NETDEV_FLAG_BROADCAST | NETDEV_FLAG_NOARP | NETDEV_FLAG_LOOPBACK;
    d->hlen = 2;
    d->alen = 0;
    memcpy(d->name, "lo", strlen("lo") + 1);
}
void lo_init(net_ns_t* net_ns)
{
    struct netdev* netdev = netdev_alloc(&lo_init_helper);
    netdev->ops = &lo_ops;

    struct lo* lo = (void*)kalloc();
    memset(lo, 0, sizeof(*lo));
    netdev->priv = (void*)lo;

    netdev->flags |= NETDEV_FLAG_RUNNING;
    net_ns->lo = netdev;
    netdev_register(net_ns, netdev);
}
