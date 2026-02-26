#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <fcntl.h>

#define MAX_OUT 32
#define DEFAULT_TIMEOUT_MS 200

static struct sockaddr_in target;
static uint16_t *ports = NULL;
static size_t port_count = 0;
static size_t port_cap = 0;
static uint32_t timeout_ms = DEFAULT_TIMEOUT_MS;

static int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int add_port(uint16_t p)
{
    if (port_count == port_cap) {
        size_t new_cap = port_cap ? port_cap * 2 : 128;
        void *tmp = realloc(ports, new_cap * sizeof(uint16_t));
        if (!tmp) return -1;
        ports = tmp;
        port_cap = new_cap;
    }
    ports[port_count++] = p;
    return 0;
}

static int parse_u16(const char *s, uint16_t *out)
{
    char *end;
    unsigned long v;

    errno = 0;
    v = strtoul(s, &end, 10);
    if (errno || *end || v == 0 || v > 65535)
        return -1;

    *out = (uint16_t)v;
    return 0;
}

static int parse_portspec(const char *spec)
{
    char *copy = strdup(spec);
    if (!copy) return -1;

    char *tok = strtok(copy, ",");
    while (tok) {
        char *dash = strchr(tok, '-');
        if (dash) {
            *dash = 0;
            uint16_t s,e;
            if (parse_u16(tok,&s)<0 ||
                parse_u16(dash+1,&e)<0 ||
                e<s) { free(copy); return -1; }

            for (uint32_t p=s;p<=e;p++)
                if (add_port((uint16_t)p)<0) {
                    free(copy); return -1;
                }
        } else {
            uint16_t p;
            if (parse_u16(tok,&p)<0 ||
                add_port(p)<0) {
                free(copy); return -1;
            }
        }
        tok = strtok(NULL,",");
    }

    free(copy);
    return 0;
}

struct conn {
    int fd;
    uint16_t port;
    int active;
};

int main(int argc, char **argv)
{
    if (argc < 2) return 1;

    memset(&target,0,sizeof(target));
    target.sin_family = AF_INET;

    if (inet_pton(AF_INET,argv[1],&target.sin_addr)!=1)
        return 1;

    const char *portspec = "1-1024";

    for (int i=2;i<argc;i++) {
        if (!strcmp(argv[i],"--timeout") && i+1<argc) {
            timeout_ms = strtoul(argv[++i],NULL,10);
        } else {
            portspec = argv[i];
        }
    }

    if (parse_portspec(portspec)<0 || port_count==0)
        return 1;

    struct conn conns[MAX_OUT];
    struct pollfd pfds[MAX_OUT];

    memset(conns,0,sizeof(conns));

    size_t next_port = 0;
    size_t active = 0;

    while (next_port < port_count || active > 0) {

        while (active < MAX_OUT && next_port < port_count) {

            int fd = socket(AF_INET,SOCK_STREAM,0);
            if (fd < 0) break;

            if (set_nonblock(fd)<0) {
                close(fd);
                break;
            }

            struct sockaddr_in addr = target;
            addr.sin_port = htons(ports[next_port]);

            int r = connect(fd,(struct sockaddr*)&addr,sizeof(addr));

            if (r == 0) {
                dprintf(STDOUT_FILENO,"%u/tcp open\n",ports[next_port]);
                close(fd);
                next_port++;
                continue;
            }

            if (errno != EINPROGRESS) {
                close(fd);
                next_port++;
                continue;
            }

            conns[active].fd = fd;
            conns[active].port = ports[next_port];
            conns[active].active = 1;

            pfds[active].fd = fd;
            pfds[active].events = POLLOUT;

            active++;
            next_port++;
        }

        int r = poll(pfds, active, timeout_ms);
        if (r <= 0)
            continue;

        for (size_t i=0;i<active;i++) {

            if (!conns[i].active)
                continue;

            if (pfds[i].revents & POLLOUT) {

                int err=0;
                socklen_t len=sizeof(err);

                getsockopt(conns[i].fd,SOL_SOCKET,
                           SO_ERROR,&err,&len);

                if (err==0)
                    dprintf(STDOUT_FILENO,
                            "%u/tcp open\n",
                            conns[i].port);

                close(conns[i].fd);

                conns[i] = conns[active-1];
                pfds[i] = pfds[active-1];
                active--;
                i--;
            }
        }
    }

    free(ports);
    return 0;
}
