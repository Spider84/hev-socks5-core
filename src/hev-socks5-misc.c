/*
 ============================================================================
 Name        : hev-socks5-misc.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2021 hev
 Description : Socks5 Misc
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>
#include <hev-task-dns.h>
#include <hev-memory-allocator.h>

#include "hev-socks5.h"
#include "hev-socks5-logger-priv.h"

#include "hev-socks5-misc.h"
#include "hev-socks5-misc-priv.h"

int
hev_socks5_task_io_yielder (HevTaskYieldType type, void *data)
{
    HevSocks5 *self = data;

    if (type == HEV_TASK_YIELD) {
        hev_task_yield (HEV_TASK_YIELD);
        return 0;
    }

    if (self->timeout < 0) {
        hev_task_yield (HEV_TASK_WAITIO);
    } else {
        int timeout = self->timeout;
        timeout = hev_task_sleep (timeout);
        if (timeout <= 0) {
            LOG_I ("%p io timeout", self);
            return -1;
        }
    }

    return 0;
}

int
hev_socks5_socket (int type)
{
    HevTask *task = hev_task_self ();
    int fd, res;

    fd = hev_task_io_socket_socket (AF_INET6, type, 0);
    if (fd < 0)
        return -1;

    res = hev_task_add_fd (task, fd, POLLIN | POLLOUT);
    if (res < 0)
        hev_task_mod_fd (task, fd, POLLIN | POLLOUT);

    return fd;
}

static int
hev_socks5_resolve_ipv4 (const char *addr, int port, struct sockaddr_in6 *saddr)
{
    int res;

    res = inet_pton (AF_INET, addr, &saddr->sin6_addr.s6_addr[12]);
    if (res == 0)
        return -1;

    saddr->sin6_addr.s6_addr[10] = 0xff;
    saddr->sin6_addr.s6_addr[11] = 0xff;

    return 0;
}

static int
hev_socks5_resolve_ipv6 (const char *addr, int port, struct sockaddr_in6 *saddr)
{
    int res;

    res = inet_pton (AF_INET6, addr, &saddr->sin6_addr);
    if (res == 0)
        return -1;

    return 0;
}

static int
hev_socks5_resolve_ip (const char *addr, int port, struct sockaddr_in6 *saddr)
{
    int res;

    memset (saddr, 0, sizeof (*saddr));
    saddr->sin6_family = AF_INET6;
    saddr->sin6_port = htons (port);

    res = hev_socks5_resolve_ipv4 (addr, port, saddr);
    if (res == 0)
        return 0;

    res = hev_socks5_resolve_ipv6 (addr, port, saddr);
    if (res == 0)
        return 0;

    return -1;
}

int
hev_socks5_resolve_to_sockaddr6 (const char *addr, int port,
                                 struct sockaddr_in6 *saddr)
{
    struct addrinfo hints = { 0 };
    struct addrinfo *result;
    int res;

    res = hev_socks5_resolve_ip (addr, port, saddr);
    if (res == 0)
        return 0;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

    res = hev_task_dns_getaddrinfo (addr, NULL, &hints, &result);
    if (res < 0)
        return -1;

    if (result->ai_family == AF_INET) {
        struct sockaddr_in *adp;

        saddr->sin6_family = AF_INET6;
        memset (&saddr->sin6_addr, 0, 10);
        saddr->sin6_addr.s6_addr[10] = 0xff;
        saddr->sin6_addr.s6_addr[11] = 0xff;
        adp = (struct sockaddr_in *)result->ai_addr;
        memcpy (&saddr->sin6_addr.s6_addr[12], &adp->sin_addr, 4);
    } else if (result->ai_family == AF_INET6) {
        memcpy (saddr, result->ai_addr, sizeof (*saddr));
    }

    saddr->sin6_port = htons (port);
    freeaddrinfo (result);

    return 0;
}

static int
hev_socks5_addr_to_sockaddr4 (HevSocks5Addr *addr, struct sockaddr_in *saddr)
{
    int res = -1;

    if (addr->atype == HEV_SOCKS5_ADDR_TYPE_IPV4) {
        struct sockaddr_in *adp;

        res = sizeof (*adp);
        adp = (struct sockaddr_in *)saddr;
        adp->sin_family = AF_INET;
        adp->sin_port = addr->ipv4.port;
        memcpy (&adp->sin_addr, addr->ipv4.addr, 4);
    }

    return res;
}

static int
hev_socks5_addr_to_sockaddr6 (HevSocks5Addr *addr, struct sockaddr_in6 *saddr)
{
    int res = -1;

    if (addr->atype == HEV_SOCKS5_ADDR_TYPE_IPV4) {
        res = sizeof (*saddr);
        saddr->sin6_family = AF_INET6;
        saddr->sin6_port = addr->ipv4.port;
        memset (&saddr->sin6_addr, 0, 10);
        saddr->sin6_addr.s6_addr[10] = 0xff;
        saddr->sin6_addr.s6_addr[11] = 0xff;
        memcpy (&saddr->sin6_addr.s6_addr[12], addr->ipv4.addr, 4);
    } else if (addr->atype == HEV_SOCKS5_ADDR_TYPE_IPV6) {
        res = sizeof (*saddr);
        saddr->sin6_family = AF_INET6;
        saddr->sin6_port = addr->ipv6.port;
        memcpy (&saddr->sin6_addr, addr->ipv6.addr, 16);
    }

    return res;
}

int
hev_socks5_addr_to_sockaddr (HevSocks5Addr *addr, struct sockaddr *saddr)
{
    int res = -1;

    if (saddr->sa_family == AF_INET) {
        struct sockaddr_in *adp;

        adp = (struct sockaddr_in *)saddr;
        res = hev_socks5_addr_to_sockaddr4 (addr, adp);
    } else {
        struct sockaddr_in6 *adp;

        adp = (struct sockaddr_in6 *)saddr;
        res = hev_socks5_addr_to_sockaddr6 (addr, adp);
    }

    return res;
}

int
hev_socks5_addr_from_sockaddr (HevSocks5Addr *addr, struct sockaddr *saddr)
{
    int res = -1;

    if (saddr->sa_family == AF_INET) {
        struct sockaddr_in *adp;

        res = 7;
        adp = (struct sockaddr_in *)saddr;
        addr->atype = HEV_SOCKS5_ADDR_TYPE_IPV4;
        addr->ipv4.port = adp->sin_port;
        memcpy (addr->ipv4.addr, &adp->sin_addr, 4);
    } else if (saddr->sa_family == AF_INET6) {
        struct sockaddr_in6 *adp;

        adp = (struct sockaddr_in6 *)saddr;
        if (IN6_IS_ADDR_V4MAPPED (&adp->sin6_addr)) {
            res = 7;
            addr->atype = HEV_SOCKS5_ADDR_TYPE_IPV4;
            addr->ipv4.port = adp->sin6_port;
            memcpy (addr->ipv4.addr, &adp->sin6_addr.s6_addr[12], 4);
        } else {
            res = 19;
            addr->atype = HEV_SOCKS5_ADDR_TYPE_IPV6;
            addr->ipv6.port = adp->sin6_port;
            memcpy (addr->ipv6.addr, &adp->sin6_addr, 16);
        }
    }

    return res;
}

const char *
hev_socks5_addr_to_string (HevSocks5Addr *addr, char *buf, int len)
{
    const char *res = buf;
    char sa[512];
    uint16_t port;

    switch (addr->atype) {
    case HEV_SOCKS5_ADDR_TYPE_IPV4:
        port = ntohs (addr->ipv4.port);
        inet_ntop (AF_INET, addr->ipv4.addr, sa, sizeof (sa));
        break;
    case HEV_SOCKS5_ADDR_TYPE_IPV6:
        port = ntohs (addr->ipv6.port);
        inet_ntop (AF_INET6, addr->ipv6.addr, sa, sizeof (sa));
        break;
    case HEV_SOCKS5_ADDR_TYPE_NAME:
        memcpy (sa, addr->domain.addr, addr->domain.len);
        sa[addr->domain.len] = '\0';
        memcpy (&port, addr->domain.addr + addr->domain.len, 2);
        port = ntohs (port);
        break;
    default:
        return NULL;
    }

    snprintf (buf, len, "[%s]:%u", sa, port);

    return res;
}
