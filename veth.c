#include "common.h"
#include "defs.h"
#include "net.h"
#include "types.h"

struct veth_frame {
    uint16_t type;
    uint8_t packet[];
};

struct veth {
    struct veth* peer;
    struct queue_head frame_queue;
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

static int veth_tx_helper(struct netdev* netdev, uint16_t type, uint8_t const* frame, size_t len)
{
    struct veth_frame* f = (void*)kalloc();
    f->type = hton16(type);
    memcpy(&f->packet[0], frame, len);

    struct veth* veth = netdev->priv;
    queue_push(&veth->peer->frame_queue, f, len);

    return len;
}

static int veth_tx(struct netdev* dev, uint16_t type, uint8_t const* segment, size_t size, const void* dst)
{
    return veth_tx_helper(dev, type, segment, size);
}

void veth_intr_handle(struct netdev* netdev)
{
    struct veth* veth = (struct veth*)netdev->priv;

    // it's possible multiple CPUs reach here at the same time
    // when only one is needed to clear the queue

    struct queue_entry* e;
    while ((e = queue_pop(&veth->frame_queue)) != NULL) {
        struct veth_frame* f = e->data;
        size_t size = e->size;
        kfree((void*)e);
        netdev_receive(netdev, f->type, &f->packet[0], size);
        kfree((void*)f);
    }
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

    netdev->flags |= NETDEV_FLAG_RUNNING | NETDEV_FLAG_NOARP;
    netdev_register(net_ns_1, netdev);

    netdev = netdev_alloc(&veth_init_helper);
    netdev->type = NETDEV_TYPE_VETH;
    netdev->ops = &veth_ops;
    netdev->priv = (void*)veth2;

    netdev->flags |= NETDEV_FLAG_RUNNING | NETDEV_FLAG_NOARP;
    netdev_register(net_ns_2, netdev);
}
