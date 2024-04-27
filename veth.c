#include "common.h"
#include "defs.h"
#include "net.h"
#include "types.h"

struct veth {
    struct veth* peer;
    struct queue_head frame_queue;
    struct spinlock lock;
};

static int veth_open(struct netdev* dev)
{
    dev->flags |= NETDEV_FLAG_UP;
    return 0;
}
static int veth_stop(struct netdev* dev)
{
    dev->flags &= ~NETDEV_FLAG_UP;
    return 0;
}

static int veth_tx_helper(struct netdev* netdev, uint8_t* frame, size_t len)
{
    void* f = (void*)kalloc();
    memcpy(f, frame, len);
    queue_push(&((struct veth*)netdev->priv)->frame_queue, f, len);
    return len;
}

static int veth_tx(struct netdev* dev, uint16_t type, const uint8_t* segment, size_t size, const void* dst)
{
    return ethernet_tx_helper(dev, type, segment, size, dst, &veth_tx_helper);
}

void veth_intr_handle(struct netdev* netdev)
{
    struct veth* veth = (struct veth*)netdev->priv;

    // it's possible multiple CPUs reach here at the same time
    // when only one is needed to clear the queue
    if (!tryacquire(&veth->lock))
        return;

    struct queue_entry* e;
    while ((e = queue_pop(&veth->frame_queue)) != NULL) {
        void* f = e->data;
        size_t size = e->size;
        kfree((void*)e);
        ethernet_rx_helper(netdev, f, size, netdev_receive);
        kfree(f);
    }

    release(&veth->lock);
}

static struct netdev_ops veth_ops = {
    .open = veth_open,
    .stop = veth_stop,
    .xmit = veth_tx,
};

static void veth_init_helper(struct netdev* d)
{
    ethernet_netdev_setup(d);
}
void veth_init(net_ns_t* net_ns_1, net_ns_t* net_ns_2)
{
    struct veth* veth1 = (void*)kalloc();
    memset(veth1, 0, sizeof(*veth1));
    struct veth* veth2 = (void*)kalloc();
    memset(veth2, 0, sizeof(*veth2));
    veth1->peer = veth2;
    veth2->peer = veth1;

    struct netdev* netdev = netdev_alloc(&veth_init_helper);
    netdev->type = NETDEV_TYPE_VETH;
    netdev->ops = &veth_ops;
    netdev->priv = (void*)veth1;

    netdev->flags |= NETDEV_FLAG_RUNNING;
    netdev_register(net_ns_1, netdev);

    netdev = netdev_alloc(&veth_init_helper);
    netdev->type = NETDEV_TYPE_VETH;
    netdev->ops = &veth_ops;
    netdev->priv = (void*)veth2;

    netdev->flags |= NETDEV_FLAG_RUNNING;
    netdev_register(net_ns_2, netdev);
}
