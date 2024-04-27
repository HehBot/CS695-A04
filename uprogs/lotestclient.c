#include "socket.h"
#include "types.h"
#include "user.h"

int main(int argc, char* argv[])
{
    int soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc == -1) {
        printf(2, "socket: failure\n");
        exit();
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    ip_addr_pton("127.0.0.1", &addr.sin_addr);
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
