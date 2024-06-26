#include "socket.h"
#include "types.h"
#include "user.h"

int main(int argc, char* argv[])
{
    int soc, acc, peerlen, ret;
    struct sockaddr_in self, peer;
    unsigned char* addr;
    char buf[2048];

    printf(1, "Starting TCP Echo Server\n");
    soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc == -1) {
        printf(2, "socket: failure\n");
        exit();
    }
    printf(1, "socket: success, soc=%d\n", soc);
    self.sin_family = AF_INET;
    self.sin_addr = INADDR_ANY;
    self.sin_port = hton16(7);
    if (bind(soc, (struct sockaddr*)&self, sizeof(self)) == -1) {
        printf(2, "bind: failure\n");
        close(soc);
        exit();
    }
    addr = (unsigned char*)&self.sin_addr;
    printf(1, "bind: success, self=%d.%d.%d.%d:%d\n", addr[0], addr[1], addr[2], addr[3], ntoh16(self.sin_port));
    listen(soc, 100);
    printf(1, "waiting for connection...\n");
    peerlen = sizeof(peer);
    acc = accept(soc, (struct sockaddr*)&peer, &peerlen);
    if (acc == -1) {
        printf(2, "accept: failure\n");
        close(soc);
        exit();
    }
    addr = (unsigned char*)&peer.sin_addr;
    printf(1, "accept: success, peer=%d.%d.%d.%d:%d\n", addr[0], addr[1], addr[2], addr[3], ntoh16(peer.sin_port));
    while (1) {
        ret = read(acc, buf, sizeof(buf));
        if (ret <= 0) {
            printf(1, "EOF\n");
            break;
        }
        printf(1, "recv: %d bytes data received\n", ret);
        hexdump(buf, ret);
        write(acc, buf, ret);
    }
    close(soc);
    exit();
}
