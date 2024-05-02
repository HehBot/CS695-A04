// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "defs.h"
#include "net.h"
#include "param.h"

struct netproto {
    struct netproto* next;
    uint16_t type;
    void (*handler)(uint8_t* packet, size_t plen, struct netdev* dev);
};

struct {
    net_ns_t table[MAXNETNS];

    struct spinlock lock;
    int used[MAXNETNS];
} net_nss = { 0 };

net_ns_t first_net_ns = { 0 };

static struct netproto* protocols;

net_ns_t* alloc_net_ns(void)
{
    acquire(&net_nss.lock);

    for (int i = 0; i < MAXNETNS; ++i) {
        if (!net_nss.used[i]) {
            net_ns_t* net_ns = &net_nss.table[i];
            net_nss.used[i] = 1;
            net_ns->devices = net_ns->lo = NULL;
            net_ns->next_index = 0;
            net_ns->nr_proc = 1;
            lo_init(net_ns);
            release(&net_nss.lock);
            return net_ns;
        }
    }
    release(&net_nss.lock);
    return NULL;
}
net_ns_t* get_net_ns(net_ns_t* net_ns)
{
    acquire(&net_ns->lock);
    net_ns->nr_proc++;
    release(&net_ns->lock);
    return net_ns;
}
void put_net_ns(net_ns_t* net_ns)
{
    acquire(&net_ns->lock);
    net_ns->nr_proc--;
    if (net_ns->nr_proc == 0) {
        if (net_ns == &first_net_ns)
            panic("initial net_ns empty");
        acquire(&net_nss.lock);
        net_nss.used[net_ns - &net_nss.table[0]] = 0;
        release(&net_nss.lock);
    }
    release(&net_ns->lock);
}

struct netdev*
netdev_alloc(void (*setup)(struct netdev*))
{
    struct netdev* dev;

    dev = (struct netdev*)kalloc();
    if (!dev) {
        return NULL;
    }
    memset(dev, 0, sizeof(struct netdev));
    setup(dev);
    return dev;
}

int netdev_register(net_ns_t* net_ns, struct netdev* dev)
{
    dev->index = net_ns->next_index++;

    if (dev->name[0] == '\0')
        snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);

    dev->next = net_ns->devices;
    dev->net_ns = net_ns;
    net_ns->devices = dev;

    cprintf("[net] [%p] netdev_register: <%s>\n", net_ns, dev->name);
    return 0;
}

struct netdev*
netdev_by_index(net_ns_t* net_ns, int index)
{
    struct netdev* dev;

    for (dev = net_ns->devices; dev; dev = dev->next)
        if (dev->index == index)
            return dev;

    return NULL;
}

struct netdev*
netdev_by_name(net_ns_t* net_ns, const char* name)
{
    struct netdev* dev;

    for (dev = net_ns->devices; dev; dev = dev->next)
        if (strcmp(dev->name, name) == 0)
            return dev;

    return NULL;
}

void netdev_receive(struct netdev* dev, uint16_t type, uint8_t* packet, unsigned int plen)
{
    struct netproto* entry;
#ifdef DEBUG
    cprintf("[net] netdev_receive: dev=%s, type=%04x, packet=%p, plen=%u\n", dev->name, type, packet, plen);
#endif
    for (entry = protocols; entry; entry = entry->next) {
        if (hton16(entry->type) == type) {
            entry->handler(packet, plen, dev);
            return;
        }
    }
}

int netdev_add_netif(struct netdev* dev, struct netif* netif)
{
    struct netif* entry;

    for (entry = dev->ifs; entry; entry = entry->next) {
        if (entry->family == netif->family) {
            return -1;
        }
    }
#ifdef DEBUG
    if (netif->family == NETIF_FAMILY_IPV4) {
        char addr[IP_ADDR_STR_LEN];
        cprintf("[net] Add <%s> to <%s>\n", ip_addr_ntop(&((struct netif_ip*)netif)->unicast, addr, sizeof(addr)), dev->name);
    }
#endif
    netif->next = dev->ifs;
    netif->dev = dev;
    dev->ifs = netif;
    return 0;
}

struct netif*
netdev_get_netif(struct netdev* dev, int family)
{
    struct netif* entry;

    for (entry = dev->ifs; entry; entry = entry->next) {
        if (entry->family == family) {
            return entry;
        }
    }
    return NULL;
}

int netproto_register(unsigned short type, void (*handler)(uint8_t* packet, size_t plen, struct netdev* dev))
{
    struct netproto* entry;

    for (entry = protocols; entry; entry = entry->next) {
        if (entry->type == type) {
            return -1;
        }
    }
    entry = (struct netproto*)kalloc();
    if (!entry) {
        return -1;
    }
    entry->next = protocols;
    entry->type = type;
    entry->handler = handler;
    protocols = entry;
    return 0;
}

void netinit(void)
{
    lo_init(&first_net_ns);

    arp_init();
    ip_init();
    icmp_init();
    udp_init();
    tcp_init();
}

void net_virt_intr(void)
{
    struct netdev* dev = first_net_ns.devices;
    while (dev != NULL) {
        if (dev->type == NETDEV_TYPE_LOOPBACK)
            lo_intr_handle(dev);
        else if (dev->type == NETDEV_TYPE_VETH)
            veth_intr_handle(dev);
        dev = dev->next;
    }

    acquire(&net_nss.lock);
    void lo_intr_handle(struct netdev*);
    for (int i = 0; i < MAXNETNS; ++i) {
        if (net_nss.used[i]) {
            struct netdev* dev = net_nss.table[i].devices;
            while (dev != NULL) {
                if (dev->type == NETDEV_TYPE_LOOPBACK)
                    lo_intr_handle(dev);
                else if (dev->type == NETDEV_TYPE_VETH)
                    veth_intr_handle(dev);
                dev = dev->next;
            }
        }
    }
    release(&net_nss.lock);
}
