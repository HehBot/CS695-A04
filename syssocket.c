// Copyright (c) 2012-2020 YAMAMOTO Masaya
// SPDX-License-Identifier: MIT

#include "defs.h"
#include "file.h"
#include "net.h"
#include "proc.h"
#include "types.h"

int sys_socket(void)
{
    int fd, domain, type, protocol;
    struct file* f;

    if (argint(0, &domain) < 0 || argint(1, &type) < 0 || argint(2, &protocol) < 0)
        return -1;
    if ((f = socketalloc(domain, type, protocol)) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        return -1;
    }
    return fd;
}

int sys_connect(void)
{
    struct file* f;
    int addrlen;
    struct sockaddr* addr;

    if (argfd(0, 0, &f) < 0 || argint(2, &addrlen) < 0 || argptr(1, (void*)&addr, addrlen) < 0)
        return -1;
    if (f->type != FD_SOCKET)
        return -1;
    return socketconnect(f->socket, addr, addrlen);
}

int sys_bind(void)
{
    struct file* f;
    int addrlen;
    struct sockaddr* addr;

    if (argfd(0, 0, &f) < 0 || argint(2, &addrlen) < 0 || argptr(1, (void*)&addr, addrlen) < 0)
        return -1;
    if (f->type != FD_SOCKET)
        return -1;
    return socketbind(f->socket, addr, addrlen);
}

int sys_listen(void)
{
    struct file* f;
    int backlog;

    if (argfd(0, 0, &f) < 0 || argint(1, &backlog) < 0)
        return -1;
    if (f->type != FD_SOCKET)
        return -1;
    return socketlisten(f->socket, backlog);
}

int sys_accept(void)
{
    struct file *f, *af;
    int *addrlen, afd;
    struct sockaddr* addr = NULL;

    if (argfd(0, 0, &f) < 0 || argptr(2, (void*)&addrlen, sizeof(*addrlen)) < 0)
        return -1;
    if (addrlen && argptr(1, (void*)&addr, *addrlen) < 0)
        return -1;
    if (f->type != FD_SOCKET)
        return -1;
    if ((af = socketaccept(f->socket, addr, addrlen)) == 0 || (afd = fdalloc(af)) < 0) {
        if (af)
            fileclose(af);
        return -1;
    }
    return afd;
}

int sys_recvfrom(void)
{
    struct file* f;
    int n;
    char* p;
    int* addrlen;
    struct sockaddr* addr = NULL;

    if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0 || argptr(4, (void*)&addrlen, sizeof(*addrlen)) < 0)
        return -1;
    if (addrlen && argptr(3, (void*)&addr, *addrlen) < 0)
        return -1;
    if (f->type != FD_SOCKET)
        return -1;
    return socketrecvfrom(f->socket, p, n, addr, addrlen);
}

int sys_sendto(void)
{
    struct file* f;
    int n;
    char* p;
    int addrlen;
    struct sockaddr* addr;

    if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0 || argint(4, &addrlen) < 0 || argptr(3, (void*)&addr, addrlen) < 0)
        return -1;
    if (f->type != FD_SOCKET)
        return -1;
    return socketsendto(f->socket, p, n, addr, addrlen);
}

int sys_veth(void)
{
    int p1, p2;
    struct proc* proc1;
    struct proc* proc2;
    if (argint(0, &p1) < 0 || argint(1, &p2) < 0 || ((proc1 = getproc(p1)) == NULL) || ((proc2 = getproc(p2)) == NULL))
        return -1;
    veth_init(proc1->net_ns, proc2->net_ns);
    return 0;
}

int sys_addroute(void)
{
    ip_addr_t network, netmask, gateway;
    char const* devname;
    if (argint(0, (int*)&network) < 0 || argint(1, (int*)&netmask) < 0 || argint(2, (int*)&gateway) < 0 || argstr(3, (char**)&devname) < 0)
        return -1;
    struct netdev* dev = netdev_by_name(myproc()->net_ns, devname);
    struct netif* iface = netdev_get_netif(dev, NETIF_FAMILY_IPV4);
    return ip_route_add(network & netmask, netmask, gateway, iface, myproc()->net_ns);
}
