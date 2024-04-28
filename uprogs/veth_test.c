#include "fcntl.h"
#include "if.h"
#include "socket.h"
#include "sockio.h"
#include "types.h"
#include "user.h"

static void
ifup(const char* name)
{
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
        return;
    strcpy(ifr.ifr_name, name);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1) {
        close(fd);
        printf(1, "ifconfig: interface %s does not exist\n", name);
        return;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) == -1) {
        close(fd);
        printf(1, "ifconfig: ioctl(SIOCSIFFLAGS) failure, interface=%s\n", name);
        return;
    }
    close(fd);
}

static void
ifset(const char* name, ip_addr_t* addr, ip_addr_t* netmask)
{
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
        return;
    strcpy(ifr.ifr_name, name);
    ifr.ifr_addr.sa_family = AF_INET;
    ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr = *addr;
    if (ioctl(fd, SIOCSIFADDR, &ifr) == -1) {
        close(fd);
        printf(1, "ifconfig: ioctl(SIOCSIFADDR) failure, interface=%s\n", name);
        return;
    }
    ifr.ifr_netmask.sa_family = AF_INET;
    ((struct sockaddr_in*)&ifr.ifr_netmask)->sin_addr = *netmask;
    if (ioctl(fd, SIOCSIFNETMASK, &ifr) == -1) {
        close(fd);
        printf(1, "ifconfig: ioctl(SIOCSIFNETMASK) failure, interface=%s\n", name);
        return;
    }
    close(fd);
}

void init()
{
    while (1) {
        sleep(100);
        wait();
    }
}

void client(void)
{
    ip_addr_t a, n;
    ip_addr_pton("192.168.0.1", &a);
    ip_addr_pton("255.255.255.0", &n);
    ifset("net1", &a, &n);
    ifup("net1");

    printf(1, "Client net ns:\n");
    if (fork() == 0) {
        char* argv[] = { "ifconfig", NULL };
        exec(argv[0], argv);
    }
    wait();

    int f = open("client.log", O_CREATE | O_WRONLY);
    dup2(f, 1);
    dup2(f, 2);
    close(f);

    printf(1, "Starting client\n");

    int soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc == -1) {
        printf(2, "socket: failure\n");
        exit();
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    ip_addr_pton("192.168.0.1", &addr.sin_addr);
    addr.sin_port = hton16(7);
    if (connect(soc, (void*)&addr, sizeof(addr)) == -1) {
        printf(2, "connect: failure\n");
        close(soc);
        exit();
    }
    char* hello = "Hello from lo client";
    size_t l = strlen(hello);
    send(soc, hello, l);

    char buf[128];
    size_t recv_len = recv(soc, &buf[0], sizeof(buf));
    if (recv_len != l) {
        printf(2, "receive size mismatch! %d recv, expected %d\n", recv_len, l);
        close(soc);
        exit();
    }
    buf[recv_len] = '\0';
    printf(1, "Recv:\n%s\n", &buf[0]);
    close(soc);
    exit();
}

int main()
{
    int i2;
    unshare(NS_PID);
    if ((i2 = fork()) == 0) {
        unshare(NS_NET);
        init();
    }

    sleep(100);

    veth(getpid(), i2);

    ip_addr_t a, n;
    ip_addr_pton("192.168.0.1", &a);
    ip_addr_pton("255.255.255.0", &n);
    ifset("net3", &a, &n);
    ifup("net3");

    setns(getpid(), NS_PID);
    if (fork() == 0) {
        printf(1, "Server net ns:\n");
        char* argv[] = { "ifconfig", NULL };
        exec(argv[0], argv);
    }
    if (fork() == 0) {
        int f = open("server.log", O_CREATE | O_WRONLY);
        dup2(f, 1);
        dup2(f, 2);
        close(f);

        printf(1, "Starting server\n");
        char* argv[] = { "/tcpechoserver", NULL };
        exec(argv[0], argv);
    }

    setns(i2, NS_PID | NS_NET);
    if (fork() == 0) {
        sleep(150);
        client();
    }

    exit();
}
