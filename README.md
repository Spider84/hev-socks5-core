# HevSocks5Core

HevSocks5Core is a simple, lightweight socks5 library.

**Features**
* IPv4/IPv6. (dual stack)
* Standard `CONNECT` command.
* Extended `FWDUDP` command. (UDP over TCP)
* Simple username/password authentication.

## Examples

### Server

```c
#include <unistd.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-task-dns.h>
#include <hev-task-system.h>

#include <hev-socks5-server.h>

static void
server_entry (void *data)
{
    HevSocks5Server *server = data;
    hev_socks5_server_run (server);
    hev_socks5_unref (HEV_SOCKS5 (server));
}

static void
listener_entry (void *data)
{
    struct addrinfo hints = { 0 };
    struct addrinfo *result;
    int fd;

    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    hev_task_dns_getaddrinfo (NULL, "1080", &hints, &result);
    fd = hev_task_io_socket_socket (AF_INET6, SOCK_STREAM, 0);
    bind (fd, result->ai_addr, result->ai_addrlen);
    freeaddrinfo (result);
    listen (fd, 5);

    hev_task_add_fd (hev_task_self (), fd, POLLIN);

    for (;;) {
        HevSocks5Server *server;
        HevTask *task;
        int nfd;

        nfd = hev_task_io_socket_accept (fd, NULL, NULL, NULL, NULL);

        task = hev_task_new (-1);
        server = hev_socks5_server_new (nfd);
        hev_task_run (task, server_entry, server);
    }

    close (fd);
}

int
main (int argc, char *argv[])
{
    HevTask *task;

    hev_task_system_init ();

    task = hev_task_new (-1);
    hev_task_run (task, listener_entry, NULL);

    hev_task_system_run ();

    hev_task_system_fini ();

    return 0;
}
```

### Client

```c
#include <stddef.h>

#include <hev-task.h>
#include <hev-task-system.h>
#include <hev-socks5-client-tcp.h>
#include <hev-socks5-client-udp.h>

static void
tcp_client_entry (void *data)
{
    HevSocks5ClientTCP *tcp;

    tcp = hev_socks5_client_tcp_new ("www.google.com", 443);
    hev_socks5_client_connect (HEV_SOCKS5_CLIENT (tcp), "127.0.0.1", 1080);
    hev_socks5_client_handshake (HEV_SOCKS5_CLIENT (tcp));

    /*
     * splice data to/from a socket fd:
     *     hev_socks5_tcp_splice (HEV_SOCKS5_TCP (tcp), fd);
     */

    hev_socks5_unref (HEV_SOCKS5 (tcp));
}

static void
udp_client_entry (void *data)
{
    HevSocks5ClientUDP *udp;

    udp = hev_socks5_client_udp_new ();
    hev_socks5_client_connect (HEV_SOCKS5_CLIENT (udp), "127.0.0.1", 1080);
    hev_socks5_client_handshake (HEV_SOCKS5_CLIENT (udp));

    /*
     * send udp packet:
     *     hev_socks5_udp_sendto (HEV_SOCKS5_UDP (udp), data, len, addr);
     *
     * recv udp packet: (with source address family AF_INET6)
     *     addr.sa_family = AF_INET6;
     *     hev_socks5_udp_recvfrom (HEV_SOCKS5_UDP (udp), data, len, addr);
     *
     * recv udp packet: (with source address family AF_INET for IPv4 only)
     *     addr.sa_family = AF_INET;
     *     hev_socks5_udp_recvfrom (HEV_SOCKS5_UDP (udp), data, len, addr);
     */

    hev_socks5_unref (HEV_SOCKS5 (udp));
}

int
main (int argc, char *argv[])
{
    HevTask *task;

    hev_task_system_init ();

    task = hev_task_new (-1);
    hev_task_run (task, tcp_client_entry, NULL);

    task = hev_task_new (-1);
    hev_task_run (task, udp_client_entry, NULL);

    hev_task_system_run ();

    hev_task_system_fini ();

    return 0;
}
```

## Users

* **HevSocks5Server** - https://github.com/heiher/hev-socks5-server
* **HevSocks5TProxy** - https://github.com/heiher/hev-socks5-tproxy
* **HevSocks5Tunnel** - https://github.com/heiher/hev-socks5-tunnel

## Authors
* **Heiher** - https://hev.cc

## License
LGPL
