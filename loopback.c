#include "common.h"
#include "defs.h"
#include "net.h"
#include "types.h"

static struct netdev* lo_dev;
static struct queue_head lo_frame_queue;

struct lo_frame {
    uint16_t type;
    uint8_t packet[];
};

static void setup_lo_netdev(struct netdev* d)
{
    d->type = NETDEV_TYPE_LOOPBACK;
    d->mtu = 1000;
    d->flags = NETDEV_FLAG_BROADCAST | NETDEV_FLAG_NOARP | NETDEV_FLAG_LOOPBACK;
    d->hlen = 2;
    d->alen = 0;
}

static int lo_open(struct netdev* dev)
{
    return 0;
}
static int lo_stop(struct netdev* dev)
{
    return 0;
}

static int lo_tx(struct netdev* dev, uint16_t type, const uint8_t* packet, size_t size, const void* dst)
{
    struct lo_frame* f = (void*)kalloc();
    f->type = hton16(type);
    memcpy(&f->packet[0], packet, size);
    queue_push(&lo_frame_queue, f, size);
    return size;
}
void lo_rx(void)
{
    struct queue_entry* e;
    while ((e = queue_pop(&lo_frame_queue)) != NULL) {
        struct lo_frame* f = e->data;
        size_t size = e->size;
        kfree((void*)e);
        netdev_receive(lo_dev, f->type, &f->packet[0], size);
        kfree((void*)f);
    }
}

static struct netdev_ops lo_ops = {
    .open = lo_open,
    .stop = lo_stop,
    .xmit = lo_tx,
};

void setup_lo(void)
{
    lo_dev = netdev_alloc(&setup_lo_netdev);
    lo_dev->ops = &lo_ops;
    lo_dev->priv = NULL;
    lo_dev->flags |= NETDEV_FLAG_RUNNING;
    netdev_register(lo_dev);
    memcpy(lo_dev->name, "lo", strlen("lo") + 1);

    struct netif* lo_if = ip_netif_register(lo_dev, "127.0.0.1", "255.0.0.0", NULL);
}
