#ifndef USER_H
#define USER_H

#include "types.h"

struct stat;
struct rtcdate;
struct sockaddr;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void const*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char const*, int);
int mknod(char const*, short, short);
int unlink(char const*);
int rename(char const*, char const*);
int fstat(int fd, struct stat*);
int link(char const*, char const*);
int mkdir(char const*);
int chdir(char const*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);

int ioctl(int, int, ...);
int socket(int, int, int);
int connect(int, struct sockaddr*, int);
int bind(int, struct sockaddr*, int);
int listen(int, int);
int accept(int, struct sockaddr*, int*);
int recvfrom(int, void*, int, struct sockaddr*, int*);
int sendto(int, void const*, int, struct sockaddr*, int);
int veth(int, int);
int addroute(ip_addr_t, ip_addr_t, ip_addr_t, char const*);

#define NS_PID (1 << 0)
#define NS_NET (1 << 1)

int unshare(int);
int setns(int, int);
int getgpid(void);
int chroot(char const*);
int dup2(int, int);
char* getcwd(char*, int);

int mount_procfs(char const*);
int cpu_restrict(int, int);

// ulib.c
int stat(char const*, struct stat*);
char* strcpy(char*, char const*);
void* memmove(void*, void const*, int);
char* strchr(char const*, char c);
int strcmp(char const*, char const*);
void printf(int, char const*, ...);
char* gets(int fd, char*, int max);
uint strlen(char const*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(char const*);
// additional functions
void hexdump(void* data, size_t size);
uint16_t hton16(uint16_t h);
uint16_t ntoh16(uint16_t n);
uint32_t hton32(uint32_t h);
uint32_t ntoh32(uint32_t n);
long strtol(char const* s, char** endptr, int base);
int ip_addr_pton(char const* p, ip_addr_t* n);

#define IP_ADDR_LEN 4
#define IP_ADDR_STR_LEN 16 /* "ddd.ddd.ddd.ddd\0" */

#endif // USER_H
