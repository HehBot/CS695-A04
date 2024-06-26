// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "defs.h"
#include "ethernet.h"
#include "net.h"
#include "types.h"

const uint8_t ETHERNET_ADDR_ANY[ETHERNET_ADDR_LEN] = { "\x00\x00\x00\x00\x00\x00" };
const uint8_t ETHERNET_ADDR_BROADCAST[ETHERNET_ADDR_LEN] = { "\xff\xff\xff\xff\xff\xff" };

int ethernet_addr_pton(const char* p, uint8_t* n)
{
    int index;
    char* ep;
    long val;

    if (!p || !n) {
        return -1;
    }
    for (index = 0; index < ETHERNET_ADDR_LEN; index++) {
        val = strtol(p, &ep, 16);
        if (ep == p || val < 0 || val > 0xff || (index < ETHERNET_ADDR_LEN - 1 && *ep != ':')) {
            break;
        }
        n[index] = (uint8_t)val;
        p = ep + 1;
    }
    if (index != ETHERNET_ADDR_LEN || *ep != '\0') {
        return -1;
    }
    return 0;
}

static const char*
netproto_type_ntoa(uint16_t type)
{
    switch (ntoh16(type)) {
    case NETPROTO_TYPE_IP:
        return "IP";
    case NETPROTO_TYPE_ARP:
        return "ARP";
    case NETPROTO_TYPE_IPV6:
        return "IPv6";
    }
    return "UNKNOWN";
}

char* ethernet_addr_ntop(const uint8_t* n, char* p, size_t size)
{
    if (!n || !p) {
        return NULL;
    }
    snprintf(p, size, "%02x:%02x:%02x:%02x:%02x:%02x", n[0], n[1], n[2], n[3], n[4], n[5]);
    return p;
}

static void
ethernet_dump(struct netdev* dev, uint8_t* frame, size_t flen)
{
    struct ethernet_hdr* hdr;
    char addr[ETHERNET_ADDR_STR_LEN];

    hdr = (struct ethernet_hdr*)frame;
    cprintf("  dev: %s (%s)\n", dev->name, ethernet_addr_ntop(dev->addr, addr, sizeof(addr)));
    cprintf("  src: %s\n", ethernet_addr_ntop(hdr->src, addr, sizeof(addr)));
    cprintf("  dst: %s\n", ethernet_addr_ntop(hdr->dst, addr, sizeof(addr)));
    cprintf(" type: 0x%04x (%s)\n", ntoh16(hdr->type), netproto_type_ntoa(hdr->type));
    cprintf("  len: %u octets\n", flen);
    hexdump(frame, flen);
}

ssize_t
ethernet_rx_helper(struct netdev* dev, uint8_t* frame, size_t flen, void (*cb)(struct netdev*, uint16_t, uint8_t*, size_t))
{
    struct ethernet_hdr* hdr;
    uint8_t* payload;
    size_t plen;

    if (flen < sizeof(struct ethernet_hdr)) {
        return -1;
    }
    hdr = (struct ethernet_hdr*)frame;
    if (memcmp(dev->addr, hdr->dst, ETHERNET_ADDR_LEN) != 0) {
        if (memcmp(ETHERNET_ADDR_BROADCAST, hdr->dst, ETHERNET_ADDR_LEN) != 0) {
            return -1;
        }
    }
#ifdef DEBUG
    cprintf(">>> ethernet_rx <<<\n");
    ethernet_dump(dev, frame, flen);
#endif
    payload = (uint8_t*)(hdr + 1);
    plen = flen - sizeof(struct ethernet_hdr);
    cb(dev, hdr->type, payload, plen);
    return 0;
}

ssize_t
ethernet_tx_helper(struct netdev* dev, uint16_t type, const uint8_t* payload, size_t plen, const void* dst, ssize_t (*cb)(struct netdev*, uint8_t*, size_t))
{
    void* frame = kalloc(); // DO NOT replace with kmalloc, ETHERNET_FRAME_SIZE_MAX bytes needed
    memset(frame, 0, ETHERNET_FRAME_SIZE_MAX);

    struct ethernet_hdr* hdr;
    size_t flen;

    if (!payload || plen > ETHERNET_PAYLOAD_SIZE_MAX || !dst) {
        return -1;
    }
    hdr = (struct ethernet_hdr*)frame;
    memcpy(hdr->dst, dst, ETHERNET_ADDR_LEN);
    memcpy(hdr->src, dev->addr, ETHERNET_ADDR_LEN);
    hdr->type = hton16(type);
    memcpy(hdr + 1, payload, plen);
    flen = sizeof(struct ethernet_hdr) + (plen < ETHERNET_PAYLOAD_SIZE_MIN ? ETHERNET_PAYLOAD_SIZE_MIN : plen);
#ifdef DEBUG
    cprintf(">>> ethernet_tx <<<\n");
    ethernet_dump(dev, frame, flen);
#endif
    ssize_t r = (cb(dev, frame, flen) == (ssize_t)flen ? (ssize_t)plen : -1);
    kfree(frame);
    return r;
}

void ethernet_netdev_setup(struct netdev* dev)
{
    dev->type = NETDEV_TYPE_ETHERNET;
    dev->mtu = ETHERNET_PAYLOAD_SIZE_MAX;
    dev->flags = NETDEV_FLAG_BROADCAST;
    dev->hlen = ETHERNET_HDR_SIZE;
    dev->alen = ETHERNET_ADDR_LEN;
}
