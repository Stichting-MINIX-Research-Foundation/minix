/**
 * @file
 * Sockets BSD-Like API module
 *
 * @defgroup socket Socket API
 * @ingroup sequential_api
 * BSD-style socket API.\n
 * Thread-safe, to be called from non-TCPIP threads only.\n
 * Can be activated by defining @ref LWIP_SOCKET to 1.\n
 * Header is in posix/sys/socket.h\b
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * Improved by Marc Boucher <marc@mbsi.ca> and David Haas <dhaas@alum.rpi.edu>
 *
 */

#include "lwip/opt.h"

#if LWIP_SOCKET /* don't build if not configured for use in lwipopts.h */

#include "lwip/sockets.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/priv/tcpip_priv.h"
#if LWIP_CHECKSUM_ON_COPY
#include "lwip/inet_chksum.h"
#endif

#include <string.h>

/* If the netconn API is not required publicly, then we include the necessary
   files here to get the implementation */
#if !LWIP_NETCONN
#undef LWIP_NETCONN
#define LWIP_NETCONN 1
#include "api_msg.c"
#include "api_lib.c"
#include "netbuf.c"
#undef LWIP_NETCONN
#define LWIP_NETCONN 0
#endif

#if LWIP_IPV4
#define IP4ADDR_PORT_TO_SOCKADDR(sin, ipaddr, port) do { \
      (sin)->sin_len = sizeof(struct sockaddr_in); \
      (sin)->sin_family = AF_INET; \
      (sin)->sin_port = lwip_htons((port)); \
      inet_addr_from_ip4addr(&(sin)->sin_addr, ipaddr); \
      memset((sin)->sin_zero, 0, SIN_ZERO_LEN); }while(0)
#define SOCKADDR4_TO_IP4ADDR_PORT(sin, ipaddr, port) do { \
    inet_addr_to_ip4addr(ip_2_ip4(ipaddr), &((sin)->sin_addr)); \
    (port) = lwip_ntohs((sin)->sin_port); }while(0)
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
#define IP6ADDR_PORT_TO_SOCKADDR(sin6, ipaddr, port) do { \
      (sin6)->sin6_len = sizeof(struct sockaddr_in6); \
      (sin6)->sin6_family = AF_INET6; \
      (sin6)->sin6_port = lwip_htons((port)); \
      (sin6)->sin6_flowinfo = 0; \
      inet6_addr_from_ip6addr(&(sin6)->sin6_addr, ipaddr); \
      (sin6)->sin6_scope_id = ip6_addr_zone(ipaddr); }while(0)
#define SOCKADDR6_TO_IP6ADDR_PORT(sin6, ipaddr, port) do { \
    inet6_addr_to_ip6addr(ip_2_ip6(ipaddr), &((sin6)->sin6_addr)); \
    if (ip6_addr_has_scope(ip_2_ip6(ipaddr), IP6_UNKNOWN)) { \
      ip6_addr_set_zone(ip_2_ip6(ipaddr), (u8_t)((sin6)->sin6_scope_id)); \
    } \
    (port) = lwip_ntohs((sin6)->sin6_port); }while(0)
#endif /* LWIP_IPV6 */

#if LWIP_IPV4 && LWIP_IPV6
static void sockaddr_to_ipaddr_port(const struct sockaddr* sockaddr, ip_addr_t* ipaddr, u16_t* port);

#define IS_SOCK_ADDR_LEN_VALID(namelen)  (((namelen) == sizeof(struct sockaddr_in)) || \
                                         ((namelen) == sizeof(struct sockaddr_in6)))
#define IS_SOCK_ADDR_TYPE_VALID(name)    (((name)->sa_family == AF_INET) || \
                                         ((name)->sa_family == AF_INET6))
#define SOCK_ADDR_TYPE_MATCH(name, sock) \
       ((((name)->sa_family == AF_INET) && !(NETCONNTYPE_ISIPV6((sock)->conn->type))) || \
       (((name)->sa_family == AF_INET6) && (NETCONNTYPE_ISIPV6((sock)->conn->type))))
#define IPADDR_PORT_TO_SOCKADDR(sockaddr, ipaddr, port) do { \
    if (IP_IS_V6(ipaddr)) { \
      IP6ADDR_PORT_TO_SOCKADDR((struct sockaddr_in6*)(void*)(sockaddr), ip_2_ip6(ipaddr), port); \
    } else { \
      IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in*)(void*)(sockaddr), ip_2_ip4(ipaddr), port); \
    } } while(0)
#define SOCKADDR_TO_IPADDR_PORT(sockaddr, ipaddr, port) sockaddr_to_ipaddr_port(sockaddr, ipaddr, &(port))
#define DOMAIN_TO_NETCONN_TYPE(domain, type) (((domain) == AF_INET) ? \
  (type) : (enum netconn_type)((type) | NETCONN_TYPE_IPV6))
#elif LWIP_IPV6 /* LWIP_IPV4 && LWIP_IPV6 */
#define IS_SOCK_ADDR_LEN_VALID(namelen)  ((namelen) == sizeof(struct sockaddr_in6))
#define IS_SOCK_ADDR_TYPE_VALID(name)    ((name)->sa_family == AF_INET6)
#define SOCK_ADDR_TYPE_MATCH(name, sock) 1
#define IPADDR_PORT_TO_SOCKADDR(sockaddr, ipaddr, port) \
        IP6ADDR_PORT_TO_SOCKADDR((struct sockaddr_in6*)(void*)(sockaddr), ip_2_ip6(ipaddr), port)
#define SOCKADDR_TO_IPADDR_PORT(sockaddr, ipaddr, port) \
        SOCKADDR6_TO_IP6ADDR_PORT((const struct sockaddr_in6*)(const void*)(sockaddr), ipaddr, port)
#define DOMAIN_TO_NETCONN_TYPE(domain, netconn_type) (netconn_type)
#else /*-> LWIP_IPV4: LWIP_IPV4 && LWIP_IPV6 */
#define IS_SOCK_ADDR_LEN_VALID(namelen)  ((namelen) == sizeof(struct sockaddr_in))
#define IS_SOCK_ADDR_TYPE_VALID(name)    ((name)->sa_family == AF_INET)
#define SOCK_ADDR_TYPE_MATCH(name, sock) 1
#define IPADDR_PORT_TO_SOCKADDR(sockaddr, ipaddr, port) \
        IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in*)(void*)(sockaddr), ip_2_ip4(ipaddr), port)
#define SOCKADDR_TO_IPADDR_PORT(sockaddr, ipaddr, port) \
        SOCKADDR4_TO_IP4ADDR_PORT((const struct sockaddr_in*)(const void*)(sockaddr), ipaddr, port)
#define DOMAIN_TO_NETCONN_TYPE(domain, netconn_type) (netconn_type)
#endif /* LWIP_IPV6 */

#define IS_SOCK_ADDR_TYPE_VALID_OR_UNSPEC(name)    (((name)->sa_family == AF_UNSPEC) || \
                                                    IS_SOCK_ADDR_TYPE_VALID(name))
#define SOCK_ADDR_TYPE_MATCH_OR_UNSPEC(name, sock) (((name)->sa_family == AF_UNSPEC) || \
                                                    SOCK_ADDR_TYPE_MATCH(name, sock))
#define IS_SOCK_ADDR_ALIGNED(name)      ((((mem_ptr_t)(name)) % 4) == 0)


#define LWIP_SOCKOPT_CHECK_OPTLEN(optlen, opttype) do { if ((optlen) < sizeof(opttype)) { return EINVAL; }}while(0)
#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, opttype) do { \
  LWIP_SOCKOPT_CHECK_OPTLEN(optlen, opttype); \
  if ((sock)->conn == NULL) { return EINVAL; } }while(0)
#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, opttype) do { \
  LWIP_SOCKOPT_CHECK_OPTLEN(optlen, opttype); \
  if (((sock)->conn == NULL) || ((sock)->conn->pcb.tcp == NULL)) { return EINVAL; } }while(0)
#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, opttype, netconntype) do { \
  LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, opttype); \
  if (NETCONNTYPE_GROUP(netconn_type((sock)->conn)) != netconntype) { return ENOPROTOOPT; } }while(0)


#define LWIP_SETGETSOCKOPT_DATA_VAR_REF(name)     API_VAR_REF(name)
#define LWIP_SETGETSOCKOPT_DATA_VAR_DECLARE(name) API_VAR_DECLARE(struct lwip_setgetsockopt_data, name)
#define LWIP_SETGETSOCKOPT_DATA_VAR_FREE(name)    API_VAR_FREE(MEMP_SOCKET_SETGETSOCKOPT_DATA, name)
#if LWIP_MPU_COMPATIBLE
#define LWIP_SETGETSOCKOPT_DATA_VAR_ALLOC(name, sock) do { \
  name = (struct lwip_setgetsockopt_data *)memp_malloc(MEMP_SOCKET_SETGETSOCKOPT_DATA); \
  if (name == NULL) { \
    sock_set_errno(sock, ENOMEM); \
    return -1; \
  } }while(0)
#else /* LWIP_MPU_COMPATIBLE */
#define LWIP_SETGETSOCKOPT_DATA_VAR_ALLOC(name, sock)
#endif /* LWIP_MPU_COMPATIBLE */

#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
#define LWIP_SO_SNDRCVTIMEO_OPTTYPE int
#define LWIP_SO_SNDRCVTIMEO_SET(optval, val) (*(int *)(optval) = (val))
#define LWIP_SO_SNDRCVTIMEO_GET_MS(optval)   ((s32_t)*(const int*)(optval))
#else
#define LWIP_SO_SNDRCVTIMEO_OPTTYPE struct timeval
#define LWIP_SO_SNDRCVTIMEO_SET(optval, val)  do { \
  s32_t loc = (val); \
  ((struct timeval *)(optval))->tv_sec = (loc) / 1000U; \
  ((struct timeval *)(optval))->tv_usec = ((loc) % 1000U) * 1000U; }while(0)
#define LWIP_SO_SNDRCVTIMEO_GET_MS(optval) ((((const struct timeval *)(optval))->tv_sec * 1000U) + (((const struct timeval *)(optval))->tv_usec / 1000U))
#endif

#define NUM_SOCKETS MEMP_NUM_NETCONN

/** This is overridable for the rare case where more than 255 threads
 * select on the same socket...
 */
#ifndef SELWAIT_T
#define SELWAIT_T u8_t
#endif

union lwip_sock_lastdata {
  struct netbuf *netbuf;
  struct pbuf *pbuf;
};

/** Contains all internal pointers and states used for a socket */
struct lwip_sock {
  /** sockets currently are built on netconns, each socket has one netconn */
  struct netconn *conn;
  /** last error that occurred on this socket */
  int err;
  /** data that was left from the previous read */
  union lwip_sock_lastdata lastdata;
#if LWIP_SOCKET_SELECT
  /** number of times data was received, set by event_callback(),
      tested by the receive and select functions */
  s16_t rcvevent;
  /** number of times data was ACKed (free send buffer), set by event_callback(),
      tested by select */
  u16_t sendevent;
  /** error happened for this socket, set by event_callback(), tested by select */
  u16_t errevent;
  /** counter of how many threads are waiting for this socket using select */
  SELWAIT_T select_waiting;
#endif /* LWIP_SOCKET_SELECT */
#if LWIP_NETCONN_FULLDUPLEX
  /* counter of how many threads are using a struct lwip_sock (not the 'int') */
  u8_t fd_used;
  /* status of pending close/delete actions */
  u8_t fd_free_pending;
#define LWIP_SOCK_FD_FREE_TCP  1
#define LWIP_SOCK_FD_FREE_FREE 2
#endif
};

#if LWIP_NETCONN_SEM_PER_THREAD
#define SELECT_SEM_T        sys_sem_t*
#define SELECT_SEM_PTR(sem) (sem)
#else /* LWIP_NETCONN_SEM_PER_THREAD */
#define SELECT_SEM_T        sys_sem_t
#define SELECT_SEM_PTR(sem) (&(sem))
#endif /* LWIP_NETCONN_SEM_PER_THREAD */

/** Description for a task waiting in select */
struct lwip_select_cb {
  /** Pointer to the next waiting task */
  struct lwip_select_cb *next;
  /** Pointer to the previous waiting task */
  struct lwip_select_cb *prev;
  /** readset passed to select */
  fd_set *readset;
  /** writeset passed to select */
  fd_set *writeset;
  /** unimplemented: exceptset passed to select */
  fd_set *exceptset;
  /** don't signal the same semaphore twice: set to 1 when signalled */
  int sem_signalled;
  /** semaphore to wake up a task waiting for select */
  SELECT_SEM_T sem;
};

/** A struct sockaddr replacement that has the same alignment as sockaddr_in/
 *  sockaddr_in6 if instantiated.
 */
union sockaddr_aligned {
   struct sockaddr sa;
#if LWIP_IPV6
   struct sockaddr_in6 sin6;
#endif /* LWIP_IPV6 */
#if LWIP_IPV4
   struct sockaddr_in sin;
#endif /* LWIP_IPV4 */
};

#if LWIP_IGMP
/* Define the number of IPv4 multicast memberships, default is one per socket */
#ifndef LWIP_SOCKET_MAX_MEMBERSHIPS
#define LWIP_SOCKET_MAX_MEMBERSHIPS NUM_SOCKETS
#endif

/* This is to keep track of IP_ADD_MEMBERSHIP calls to drop the membership when
   a socket is closed */
struct lwip_socket_multicast_pair {
  /** the socket */
  struct lwip_sock* sock;
  /** the interface address */
  ip4_addr_t if_addr;
  /** the group address */
  ip4_addr_t multi_addr;
};

struct lwip_socket_multicast_pair socket_ipv4_multicast_memberships[LWIP_SOCKET_MAX_MEMBERSHIPS];

static int  lwip_socket_register_membership(int s, const ip4_addr_t *if_addr, const ip4_addr_t *multi_addr);
static void lwip_socket_unregister_membership(int s, const ip4_addr_t *if_addr, const ip4_addr_t *multi_addr);
static void lwip_socket_drop_registered_memberships(int s);
#endif /* LWIP_IGMP */

/** The global array of available sockets */
static struct lwip_sock sockets[NUM_SOCKETS];
/** The global list of tasks waiting for select */
static struct lwip_select_cb *select_cb_list;
/** This counter is increased from lwip_select when the list is changed
    and checked in event_callback to see if it has changed. */
static volatile int select_cb_ctr;

#define sock_set_errno(sk, e) do { \
  const int sockerr = (e); \
  sk->err = sockerr; \
  set_errno(sockerr); \
} while (0)

/* Forward declaration of some functions */
#if LWIP_SOCKET_SELECT
static void event_callback(struct netconn *conn, enum netconn_evt evt, u16_t len);
#define DEFAULT_SOCKET_EVENTCB event_callback
#else
#define DEFAULT_SOCKET_EVENTCB NULL
#endif
#if !LWIP_TCPIP_CORE_LOCKING
static void lwip_getsockopt_callback(void *arg);
static void lwip_setsockopt_callback(void *arg);
#endif
static int lwip_getsockopt_impl(int s, int level, int optname, void *optval, socklen_t *optlen);
static int lwip_setsockopt_impl(int s, int level, int optname, const void *optval, socklen_t optlen);
static void free_socket(struct lwip_sock *sock, int is_tcp);

#if LWIP_IPV4 && LWIP_IPV6
static void
sockaddr_to_ipaddr_port(const struct sockaddr* sockaddr, ip_addr_t* ipaddr, u16_t* port)
{
  if ((sockaddr->sa_family) == AF_INET6) {
    SOCKADDR6_TO_IP6ADDR_PORT((const struct sockaddr_in6*)(const void*)(sockaddr), ipaddr, *port);
    ipaddr->type = IPADDR_TYPE_V6;
  } else {
    SOCKADDR4_TO_IP4ADDR_PORT((const struct sockaddr_in*)(const void*)(sockaddr), ipaddr, *port);
    ipaddr->type = IPADDR_TYPE_V4;
  }
}
#endif /* LWIP_IPV4 && LWIP_IPV6 */

/** LWIP_NETCONN_SEM_PER_THREAD==1: initialize thread-local semaphore */
void
lwip_socket_thread_init(void)
{
  netconn_thread_init();
}

/** LWIP_NETCONN_SEM_PER_THREAD==1: destroy thread-local semaphore */
void
lwip_socket_thread_cleanup(void)
{
  netconn_thread_cleanup();
}

#if LWIP_NETCONN_FULLDUPLEX
/* Thread-safe increment of sock->fd_used, with overflow check */
static void
sock_inc_used(struct lwip_sock *sock)
{
  LWIP_ASSERT("sock != NULL", sock != NULL);
  SYS_ARCH_INC(sock->fd_used, 1);
  LWIP_ASSERT("sock->fd_used != 0", sock->fd_used != 0);
}

/* In full-duplex mode,sock->fd_used != 0 prevents a socket descriptor from being
 * released (and possibly reused) when used from more than one thread
 * (e.g. read-while-write or close-while-write, etc)
 * This function is called at the end of functions using (try)get_socket*().
 */
static void
done_socket(struct lwip_sock *sock)
{
  SYS_ARCH_DECL_PROTECT(lev);

  LWIP_ASSERT("sock != NULL", sock != NULL);

  SYS_ARCH_PROTECT(lev);
  LWIP_ASSERT("sock->fd_used > 0", sock->fd_used > 0);
  if (--sock->fd_used == 0) {
    if (sock->fd_free_pending) {
      /* free the socket */
      sock->fd_used = 1;
      free_socket(sock, sock->fd_free_pending & LWIP_SOCK_FD_FREE_TCP);
    }
  }
  SYS_ARCH_UNPROTECT(lev);
}
#else /* LWIP_NETCONN_FULLDUPLEX */
#define sock_inc_used(sock)
#define done_socket(sock)
#endif /* LWIP_NETCONN_FULLDUPLEX */

/* Translate a socket 'int' into a pointer (only fails if the index is invalid) */
static struct lwip_sock *
tryget_socket_unconn(int fd)
{
  int s = fd - LWIP_SOCKET_OFFSET;
  if ((s < 0) || (s >= NUM_SOCKETS)) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("tryget_socket_unconn(%d): invalid\n", fd));
    return NULL;
  }
  sock_inc_used(&sockets[s]);
  return &sockets[s];
}

/**
 * Same as get_socket but doesn't set errno
 *
 * @param fd externally used socket index
 * @return struct lwip_sock for the socket or NULL if not found
 */
static struct lwip_sock *
tryget_socket(int fd)
{
  struct lwip_sock *sock = tryget_socket_unconn(fd);
  if (sock != NULL) {
    if (sock->conn) {
      return sock;
    }
    done_socket(sock);
  }
  return NULL;
}

/**
 * Map a externally used socket index to the internal socket representation.
 *
 * @param fd externally used socket index
 * @return struct lwip_sock for the socket or NULL if not found
 */
static struct lwip_sock *
get_socket(int fd)
{
  struct lwip_sock *sock = tryget_socket(fd);
  if (!sock) {
    if ((fd < LWIP_SOCKET_OFFSET) || (fd >= (LWIP_SOCKET_OFFSET + NUM_SOCKETS))) {
      LWIP_DEBUGF(SOCKETS_DEBUG, ("get_socket(%d): invalid\n", fd));
    }
    set_errno(EBADF);
    return NULL;
  }
  return sock;
}

/**
 * Allocate a new socket for a given netconn.
 *
 * @param newconn the netconn for which to allocate a socket
 * @param accepted 1 if socket has been created by accept(),
 *                 0 if socket has been created by socket()
 * @return the index of the new socket; -1 on error
 */
static int
alloc_socket(struct netconn *newconn, int accepted)
{
  int i;
  SYS_ARCH_DECL_PROTECT(lev);
  LWIP_UNUSED_ARG(accepted);

  /* allocate a new socket identifier */
  for (i = 0; i < NUM_SOCKETS; ++i) {
    /* Protect socket array */
    SYS_ARCH_PROTECT(lev);
    if (!sockets[i].conn) {
#if LWIP_NETCONN_FULLDUPLEX
      if (sockets[i].fd_used) {
        continue;
      }
      sockets[i].fd_used    = 1;
      sockets[i].fd_free_pending = 0;
#endif
      sockets[i].conn       = newconn;
      /* The socket is not yet known to anyone, so no need to protect
         after having marked it as used. */
      SYS_ARCH_UNPROTECT(lev);
      sockets[i].lastdata.pbuf = NULL;
#if LWIP_SOCKET_SELECT
      LWIP_ASSERT("sockets[i].select_waiting == 0", sockets[i].select_waiting == 0);
      sockets[i].rcvevent   = 0;
      /* TCP sendbuf is empty, but the socket is not yet writable until connected
       * (unless it has been created by accept()). */
      sockets[i].sendevent  = (NETCONNTYPE_GROUP(newconn->type) == NETCONN_TCP ? (accepted != 0) : 1);
      sockets[i].errevent   = 0;
#endif /* LWIP_SOCKET_SELECT */
      sockets[i].err        = 0;
      return i + LWIP_SOCKET_OFFSET;
    }
    SYS_ARCH_UNPROTECT(lev);
  }
  return -1;
}

/** Free a socket. The socket's netconn must have been
 * delete before!
 *
 * @param sock the socket to free
 * @param is_tcp != 0 for TCP sockets, used to free lastdata
 */
static void
free_socket(struct lwip_sock *sock, int is_tcp)
{
  union lwip_sock_lastdata lastdata;
  SYS_ARCH_DECL_PROTECT(lev);

  /* Protect socket array */
  SYS_ARCH_PROTECT(lev);

#if LWIP_NETCONN_FULLDUPLEX
  LWIP_ASSERT("sock->fd_used > 0", sock->fd_used > 0);
  if (--sock->fd_used > 0) {
    sock->fd_free_pending = LWIP_SOCK_FD_FREE_FREE | is_tcp ? LWIP_SOCK_FD_FREE_TCP : 0;
    SYS_ARCH_UNPROTECT(lev);
    return;
  }
#endif

  lastdata         = sock->lastdata;
  sock->lastdata.pbuf = NULL;
  sock->err        = 0;
  sock->conn = NULL;
  SYS_ARCH_UNPROTECT(lev);
  /* don't use 'sock' after this line, as another task might have allocated it */

  if (lastdata.pbuf != NULL) {
    if (is_tcp) {
      pbuf_free(lastdata.pbuf);
    } else {
      netbuf_delete(lastdata.netbuf);
    }
  }
}

/* Below this, the well-known socket functions are implemented.
 * Use google.com or opengroup.org to get a good description :-)
 *
 * Exceptions are documented!
 */

int
lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
  struct lwip_sock *sock, *nsock;
  struct netconn *newconn;
  ip_addr_t naddr;
  u16_t port = 0;
  int newsock;
  err_t err;
  int recvevent;
  SYS_ARCH_DECL_PROTECT(lev);

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_accept(%d)...\n", s));
  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  /* wait for a new connection */
  err = netconn_accept(sock->conn, &newconn);
  if (err != ERR_OK) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_accept(%d): netconn_acept failed, err=%d\n", s, err));
    if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_TCP) {
      sock_set_errno(sock, EOPNOTSUPP);
    } else if (err == ERR_CLSD) {
      sock_set_errno(sock, EINVAL);
    } else {
      sock_set_errno(sock, err_to_errno(err));
    }
    done_socket(sock);
    return -1;
  }
  LWIP_ASSERT("newconn != NULL", newconn != NULL);

  newsock = alloc_socket(newconn, 1);
  if (newsock == -1) {
    netconn_delete(newconn);
    sock_set_errno(sock, ENFILE);
    done_socket(sock);
    return -1;
  }
  LWIP_ASSERT("invalid socket index", (newsock >= LWIP_SOCKET_OFFSET) && (newsock < NUM_SOCKETS + LWIP_SOCKET_OFFSET));
  nsock = &sockets[newsock - LWIP_SOCKET_OFFSET];

  /* See event_callback: If data comes in right away after an accept, even
   * though the server task might not have created a new socket yet.
   * In that case, newconn->socket is counted down (newconn->socket--),
   * so nsock->rcvevent is >= 1 here!
   */
  SYS_ARCH_PROTECT(lev);
  recvevent = (s16_t)(-1 - newconn->socket);
  newconn->socket = newsock;
  SYS_ARCH_UNPROTECT(lev);

  if (newconn->callback) {
    while(recvevent > 0) {
      recvevent--;
      newconn->callback(newconn, NETCONN_EVT_RCVPLUS, 0);
    }
  }

  /* Note that POSIX only requires us to check addr is non-NULL. addrlen must
   * not be NULL if addr is valid.
   */
  if ((addr != NULL) && (addrlen != NULL)) {
    union sockaddr_aligned tempaddr;
    /* get the IP address and port of the remote host */
    err = netconn_peer(newconn, &naddr, &port);
    if (err != ERR_OK) {
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_accept(%d): netconn_peer failed, err=%d\n", s, err));
      netconn_delete(newconn);
      free_socket(nsock, 1);
      sock_set_errno(sock, err_to_errno(err));
      done_socket(sock);
      return -1;
    }

    IPADDR_PORT_TO_SOCKADDR(&tempaddr, &naddr, port);
    if (*addrlen > tempaddr.sa.sa_len) {
      *addrlen = tempaddr.sa.sa_len;
    }
    MEMCPY(addr, &tempaddr, *addrlen);

    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_accept(%d) returning new sock=%d addr=", s, newsock));
    ip_addr_debug_print_val(SOCKETS_DEBUG, naddr);
    LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F"\n", port));
  } else {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_accept(%d) returning new sock=%d", s, newsock));
  }

  sock_set_errno(sock, 0);
  done_socket(sock);
  done_socket(nsock);
  return newsock;
}

int
lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
  struct lwip_sock *sock;
  ip_addr_t local_addr;
  u16_t local_port;
  err_t err;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (!SOCK_ADDR_TYPE_MATCH(name, sock)) {
    /* sockaddr does not match socket type (IPv4/IPv6) */
    sock_set_errno(sock, err_to_errno(ERR_VAL));
    done_socket(sock);
    return -1;
  }

  /* check size, family and alignment of 'name' */
  LWIP_ERROR("lwip_bind: invalid address", (IS_SOCK_ADDR_LEN_VALID(namelen) &&
             IS_SOCK_ADDR_TYPE_VALID(name) && IS_SOCK_ADDR_ALIGNED(name)),
             sock_set_errno(sock, err_to_errno(ERR_ARG)); return -1;);
  LWIP_UNUSED_ARG(namelen);

  SOCKADDR_TO_IPADDR_PORT(name, &local_addr, local_port);
  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_bind(%d, addr=", s));
  ip_addr_debug_print_val(SOCKETS_DEBUG, local_addr);
  LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F")\n", local_port));

#if LWIP_IPV4 && LWIP_IPV6
  /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
  if (IP_IS_V6_VAL(local_addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(&local_addr))) {
    unmap_ipv4_mapped_ipv6(ip_2_ip4(&local_addr), ip_2_ip6(&local_addr));
    IP_SET_TYPE_VAL(local_addr, IPADDR_TYPE_V4);
  }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

  err = netconn_bind(sock->conn, &local_addr, local_port);

  if (err != ERR_OK) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_bind(%d) failed, err=%d\n", s, err));
    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return -1;
  }

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_bind(%d) succeeded\n", s));
  sock_set_errno(sock, 0);
  done_socket(sock);
  return 0;
}

int
lwip_close(int s)
{
  struct lwip_sock *sock;
  int is_tcp = 0;
  err_t err;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_close(%d)\n", s));

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (sock->conn != NULL) {
    is_tcp = NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP;
  } else {
    LWIP_ASSERT("sock->lastdata == NULL", sock->lastdata.pbuf == NULL);
  }

#if LWIP_IGMP
  /* drop all possibly joined IGMP memberships */
  lwip_socket_drop_registered_memberships(s);
#endif /* LWIP_IGMP */

  err = netconn_delete(sock->conn);
  if (err != ERR_OK) {
    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return -1;
  }

  free_socket(sock, is_tcp);
  set_errno(0);
  return 0;
}

int
lwip_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
  struct lwip_sock *sock;
  err_t err;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (!SOCK_ADDR_TYPE_MATCH_OR_UNSPEC(name, sock)) {
    /* sockaddr does not match socket type (IPv4/IPv6) */
    sock_set_errno(sock, err_to_errno(ERR_VAL));
    done_socket(sock);
    return -1;
  }

  LWIP_UNUSED_ARG(namelen);
  if (name->sa_family == AF_UNSPEC) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d, AF_UNSPEC)\n", s));
    err = netconn_disconnect(sock->conn);
  } else {
    ip_addr_t remote_addr;
    u16_t remote_port;

    /* check size, family and alignment of 'name' */
    LWIP_ERROR("lwip_connect: invalid address", IS_SOCK_ADDR_LEN_VALID(namelen) &&
               IS_SOCK_ADDR_TYPE_VALID_OR_UNSPEC(name) && IS_SOCK_ADDR_ALIGNED(name),
               sock_set_errno(sock, err_to_errno(ERR_ARG)); return -1;);

    SOCKADDR_TO_IPADDR_PORT(name, &remote_addr, remote_port);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d, addr=", s));
    ip_addr_debug_print_val(SOCKETS_DEBUG, remote_addr);
    LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F")\n", remote_port));

#if LWIP_IPV4 && LWIP_IPV6
    /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
    if (IP_IS_V6_VAL(remote_addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(&remote_addr))) {
      unmap_ipv4_mapped_ipv6(ip_2_ip4(&remote_addr), ip_2_ip6(&remote_addr));
      IP_SET_TYPE_VAL(remote_addr, IPADDR_TYPE_V4);
    }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

    err = netconn_connect(sock->conn, &remote_addr, remote_port);
  }

  if (err != ERR_OK) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d) failed, err=%d\n", s, err));
    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return -1;
  }

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d) succeeded\n", s));
  sock_set_errno(sock, 0);
  done_socket(sock);
  return 0;
}

/**
 * Set a socket into listen mode.
 * The socket may not have been used for another connection previously.
 *
 * @param s the socket to set to listening mode
 * @param backlog (ATTENTION: needs TCP_LISTEN_BACKLOG=1)
 * @return 0 on success, non-zero on failure
 */
int
lwip_listen(int s, int backlog)
{
  struct lwip_sock *sock;
  err_t err;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_listen(%d, backlog=%d)\n", s, backlog));

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  /* limit the "backlog" parameter to fit in an u8_t */
  backlog = LWIP_MIN(LWIP_MAX(backlog, 0), 0xff);

  err = netconn_listen_with_backlog(sock->conn, (u8_t)backlog);

  if (err != ERR_OK) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_listen(%d) failed, err=%d\n", s, err));
    if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_TCP) {
      sock_set_errno(sock, EOPNOTSUPP);
    } else {
      sock_set_errno(sock, err_to_errno(err));
    }
    done_socket(sock);
    return -1;
  }

  sock_set_errno(sock, 0);
  done_socket(sock);
  return 0;
}

/* Helper function to loop over receiving pbufs from netconn
 * until "len" bytes are received or we're otherwise done.
 */
static int
lwip_recv_tcp(struct lwip_sock *sock, void *mem, size_t len, int flags)
{
  u8_t apiflags = NETCONN_NOAUTORCVD;
  int recvd = 0;
  int recv_left = len;

  LWIP_ASSERT("no socket given", sock != NULL);
  LWIP_ASSERT("this should be checked internally", NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP);

  if (flags & MSG_DONTWAIT) {
    apiflags = NETCONN_DONTBLOCK;
  } else {
    apiflags = 0;
  }

  do {
    struct pbuf *p;
    err_t err;
    u16_t copylen;

    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: top while sock->lastdata=%p\n", (void*)sock->lastdata.pbuf));
    /* Check if there is data left from the last recv operation. */
    if (sock->lastdata.pbuf) {
      p = sock->lastdata.pbuf;
    } else {
      /* No data was left from the previous operation, so we try to get
         some from the network. */
      err = netconn_recv_tcp_pbuf_flags(sock->conn, &p, apiflags);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: netconn_recv err=%d, pbuf=%p\n",
        err, (void*)p));

      if (err != ERR_OK) {
        if (recvd > 0) {
          /* already received data, return that (this trusts in getting the same error from
             netconn layer again next time netconn_recv is called) */
          if (err == ERR_CLSD) {
            /* closed but already received data, ensure select gets the FIN, too */
            if (sock->conn->callback != NULL) {
              sock->conn->callback(sock->conn, NETCONN_EVT_RCVPLUS, 0);
            }
          }
          goto lwip_recv_tcp_done;
        }
        /* We should really do some error checking here. */
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: p == NULL, error is \"%s\"!\n",
          lwip_strerr(err)));
        sock_set_errno(sock, err_to_errno(err));
        if (err == ERR_CLSD) {
          return 0;
        } else {
          return -1;
        }
      }
      LWIP_ASSERT("p != NULL", p != NULL);
      sock->lastdata.pbuf = p;
    }

    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: buflen=%"U16_F" recv_left=%d off=%d\n",
       p->tot_len, recv_left, recvd));

    if (recv_left > p->tot_len) {
      copylen = p->tot_len;
    } else {
      copylen = (u16_t)recv_left;
    }

    /* copy the contents of the received buffer into
    the supplied memory pointer mem */
    pbuf_copy_partial(p, (u8_t*)mem + recvd, copylen, 0);

    recvd += copylen;

    /* TCP combines multiple pbufs for one recv */
    LWIP_ASSERT("invalid copylen, len would underflow", recv_left >= copylen);
    recv_left -= copylen;

    /* Unless we peek the incoming message... */
    if ((flags & MSG_PEEK) == 0) {
      /* ... check if there is data left in the pbuf */
      LWIP_ASSERT("invalid copylen", p->tot_len >= copylen);
      if (p->tot_len - copylen > 0) {
        /* If so, it should be saved in the sock structure for the next recv call.
           We store the pbuf but hide/free the consumed data: */
        sock->lastdata.pbuf = pbuf_free_header(p, copylen);
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: lastdata now pbuf=%p\n", (void*)sock->lastdata.pbuf));
      } else {
        sock->lastdata.pbuf = NULL;
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recv_tcp: deleting pbuf=%p\n", (void*)p));
        pbuf_free(p);
      }
    }
    /* once we have some data to return, only add more if we don't need to wait */
    apiflags |= NETCONN_DONTBLOCK;
    /* @todo: do we need to support peeking more than one pbuf? */
  } while ((recv_left > 0) || (flags & MSG_PEEK));
lwip_recv_tcp_done:
  if (recvd > 0) {
    /* ensure window update after copying all data */
    netconn_tcp_recvd(sock->conn, recvd);
  }
  sock_set_errno(sock, 0);
  return recvd;
}

/* Convert a netbuf's address data to struct sockaddr */
static void
lwip_sock_make_addr(struct netconn *conn, ip_addr_t *fromaddr, u16_t port,
                    struct sockaddr *from, socklen_t *fromlen)
{
  union sockaddr_aligned saddr;

  LWIP_UNUSED_ARG(conn);

#if LWIP_IPV4 && LWIP_IPV6
  /* Dual-stack: Map IPv4 addresses to IPv4 mapped IPv6 */
  if (NETCONNTYPE_ISIPV6(netconn_type(conn)) && IP_IS_V4(fromaddr)) {
    ip4_2_ipv4_mapped_ipv6(ip_2_ip6(fromaddr), ip_2_ip4(fromaddr));
    IP_SET_TYPE(fromaddr, IPADDR_TYPE_V6);
  }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

  LWIP_DEBUGF(SOCKETS_DEBUG, (" addr="));
  IPADDR_PORT_TO_SOCKADDR(&saddr, fromaddr, port);
  ip_addr_debug_print(SOCKETS_DEBUG, fromaddr);
  LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F"", port));
  if (from && fromlen)
  {
    if (*fromlen > saddr.sa.sa_len) {
      *fromlen = saddr.sa.sa_len;
    }
    MEMCPY(from, &saddr, *fromlen);
  }
}

int
lwip_recvfrom(int s, void *mem, size_t len, int flags,
              struct sockaddr *from, socklen_t *fromlen)
{
  struct lwip_sock *sock;
  int              ret;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom(%d, %p, %"SZT_F", 0x%x, ..)\n", s, mem, len, flags));
  sock = get_socket(s);
  if (!sock) {
    return -1;
  }
  if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP) {
    ret = lwip_recv_tcp(sock, mem, len, flags);
#if !SOCKETS_DEBUG
    if (from && fromlen)
#endif /* !SOCKETS_DEBUG */
    {
      /* get remote addr/port from tcp_pcb */
      u16_t port;
      ip_addr_t tmpaddr;
      netconn_getaddr(sock->conn, &tmpaddr, &port, 0);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom(%d):", s));
      lwip_sock_make_addr(sock->conn, &tmpaddr, port, from, fromlen);
      LWIP_DEBUGF(SOCKETS_DEBUG, (" len=%d\n", ret));
    }
    done_socket(sock);
    return ret;
  } else {
    struct netbuf    *buf;
    u16_t            buflen, copylen;
    err_t            err;
    u8_t             apiflags;
    /* UDP / RAW */
    if (flags & MSG_DONTWAIT) {
      apiflags = NETCONN_DONTBLOCK;
    } else {
      apiflags = 0;
    }

    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom[UDP/RAW]: top sock->lastdata=%p\n", (void*)sock->lastdata.netbuf));
    /* Check if there is data left from the last recv operation. */
    buf = sock->lastdata.netbuf;
    if (buf == NULL) {
      /* No data was left from the previous operation, so we try to get
         some from the network. */
      err = netconn_recv_udp_raw_netbuf_flags(sock->conn, &buf, apiflags);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom[UDP/RAW]: netconn_recv err=%d, netbuf=%p\n",
        err, (void*)buf));

      if (err != ERR_OK) {
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom[UDP/RAW](%d): buf == NULL, error is \"%s\"!\n",
          s, lwip_strerr(err)));
        sock_set_errno(sock, err_to_errno(err));
        done_socket(sock);
        return -1;
      }
      LWIP_ASSERT("buf != NULL", buf != NULL);
      sock->lastdata.netbuf = buf;
    }

    buflen = buf->p->tot_len;
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom: buflen=%"U16_F" len=%"SZT_F"\n",
      buflen, len));

    if (len > buflen) {
      copylen = buflen;
    } else {
      copylen = (u16_t)len;
    }

    /* copy the contents of the received buffer into
    the supplied memory pointer mem */
    pbuf_copy_partial(buf->p, (u8_t*)mem, copylen, 0);
    ret = copylen;

    /* Check to see from where the data was.*/
#if !SOCKETS_DEBUG
    if (from && fromlen)
#endif /* !SOCKETS_DEBUG */
    {
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom(%d):", s));
      lwip_sock_make_addr(sock->conn, netbuf_fromaddr(buf), netbuf_fromport(buf), from, fromlen);
      LWIP_DEBUGF(SOCKETS_DEBUG, (" len=%d\n", ret));
    }

    /* If we don't peek the incoming message: zero lastdata pointer and free the netbuf */
    if ((flags & MSG_PEEK) == 0) {
      sock->lastdata.netbuf = NULL;
      netbuf_delete(buf);
    }
  }

  sock_set_errno(sock, 0);
  done_socket(sock);
  return ret;
}

int
lwip_read(int s, void *mem, size_t len)
{
  return lwip_recvfrom(s, mem, len, 0, NULL, NULL);
}

int
lwip_recv(int s, void *mem, size_t len, int flags)
{
  return lwip_recvfrom(s, mem, len, flags, NULL, NULL);
}

int
lwip_send(int s, const void *data, size_t size, int flags)
{
  struct lwip_sock *sock;
  err_t err;
  u8_t write_flags;
  size_t written;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_send(%d, data=%p, size=%"SZT_F", flags=0x%x)\n",
                              s, data, size, flags));

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_TCP) {
#if (LWIP_UDP || LWIP_RAW)
    done_socket(sock);
    return lwip_sendto(s, data, size, flags, NULL, 0);
#else /* (LWIP_UDP || LWIP_RAW) */
    sock_set_errno(sock, err_to_errno(ERR_ARG));
    done_socket(sock);
    return -1;
#endif /* (LWIP_UDP || LWIP_RAW) */
  }

  write_flags = NETCONN_COPY |
    ((flags & MSG_MORE)     ? NETCONN_MORE      : 0) |
    ((flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0);
  written = 0;
  err = netconn_write_partly(sock->conn, data, size, write_flags, &written);

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_send(%d) err=%d written=%"SZT_F"\n", s, err, written));
  sock_set_errno(sock, err_to_errno(err));
  done_socket(sock);
  return (err == ERR_OK ? (int)written : -1);
}

int
lwip_sendmsg(int s, const struct msghdr *msg, int flags)
{
  struct lwip_sock *sock;
  int i;
#if LWIP_TCP
  u8_t write_flags;
  size_t written;
#endif
  int size = 0;
  err_t err = ERR_OK;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  LWIP_ERROR("lwip_sendmsg: invalid msghdr", msg != NULL,
             sock_set_errno(sock, err_to_errno(ERR_ARG)); return -1;);
  LWIP_ERROR("lwip_sendmsg: invalid msghdr iov", msg->msg_iov != NULL,
             sock_set_errno(sock, err_to_errno(ERR_ARG)); return -1;);
  LWIP_ERROR("lwip_sendmsg: maximum iovs exceeded", (msg->msg_iovlen > 0) && (msg->msg_iovlen <= IOV_MAX),
             sock_set_errno(sock, EMSGSIZE); return -1;);
  LWIP_ERROR("lwip_sendmsg: unsupported flags", ((flags == 0) || (flags == MSG_NOSIGNAL)),
             sock_set_errno(sock, EOPNOTSUPP); return -1;);

  LWIP_UNUSED_ARG(msg->msg_control);
  LWIP_UNUSED_ARG(msg->msg_controllen);
  LWIP_UNUSED_ARG(msg->msg_flags);

  if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP) {
#if LWIP_TCP
    write_flags = NETCONN_COPY |
    ((flags & MSG_MORE)     ? NETCONN_MORE      : 0) |
    ((flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0);

    written = 0;
    err = netconn_write_vectors_partly(sock->conn, (struct netvector *)msg->msg_iov, (u16_t)msg->msg_iovlen, write_flags, &written);
    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return (err == ERR_OK ? (int)written : -1);
#else /* LWIP_TCP */
    sock_set_errno(sock, err_to_errno(ERR_ARG));
    done_socket(sock);
    return -1;
#endif /* LWIP_TCP */
  }
  /* else, UDP and RAW NETCONNs */
#if LWIP_UDP || LWIP_RAW
  {
    struct netbuf chain_buf;

    LWIP_UNUSED_ARG(flags);
    LWIP_ERROR("lwip_sendmsg: invalid msghdr name", (((msg->msg_name == NULL) && (msg->msg_namelen == 0)) ||
               IS_SOCK_ADDR_LEN_VALID(msg->msg_namelen)) ,
               sock_set_errno(sock, err_to_errno(ERR_ARG)); return -1;);

    /* initialize chain buffer with destination */
    memset(&chain_buf, 0, sizeof(struct netbuf));
    if (msg->msg_name) {
      u16_t remote_port;
      SOCKADDR_TO_IPADDR_PORT((const struct sockaddr *)msg->msg_name, &chain_buf.addr, remote_port);
      netbuf_fromport(&chain_buf) = remote_port;
    }
#if LWIP_NETIF_TX_SINGLE_PBUF
    for (i = 0; i < msg->msg_iovlen; i++) {
      size += msg->msg_iov[i].iov_len;
      if ((msg->msg_iov[i].iov_len > INT_MAX) || (size < (int)msg->msg_iov[i].iov_len)) {
        /* overflow */
        goto sendmsg_emsgsize;
      }
    }
    if (size > 0xFFFF) {
      /* overflow */
      goto sendmsg_emsgsize;
    }
    /* Allocate a new netbuf and copy the data into it. */
    if (netbuf_alloc(&chain_buf, (u16_t)size) == NULL) {
       err = ERR_MEM;
    } else {
      /* flatten the IO vectors */
      size_t offset = 0;
      for (i = 0; i < msg->msg_iovlen; i++) {
        MEMCPY(&((u8_t*)chain_buf.p->payload)[offset], msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
        offset += msg->msg_iov[i].iov_len;
      }
#if LWIP_CHECKSUM_ON_COPY
      {
        /* This can be improved by using LWIP_CHKSUM_COPY() and aggregating the checksum for each IO vector */
        u16_t chksum = ~inet_chksum_pbuf(chain_buf.p);
        netbuf_set_chksum(&chain_buf, chksum);
      }
#endif /* LWIP_CHECKSUM_ON_COPY */
      err = ERR_OK;
    }
#else /* LWIP_NETIF_TX_SINGLE_PBUF */
    /* create a chained netbuf from the IO vectors. NOTE: we assemble a pbuf chain
       manually to avoid having to allocate, chain, and delete a netbuf for each iov */
    for (i = 0; i < msg->msg_iovlen; i++) {
      struct pbuf *p;
      if (msg->msg_iov[i].iov_len > 0xFFFF) {
        /* overflow */
        goto sendmsg_emsgsize;
      }
      p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);
      if (p == NULL) {
        err = ERR_MEM; /* let netbuf_delete() cleanup chain_buf */
        break;
      }
      p->payload = msg->msg_iov[i].iov_base;
      p->len = p->tot_len = (u16_t)msg->msg_iov[i].iov_len;
      /* netbuf empty, add new pbuf */
      if (chain_buf.p == NULL) {
        chain_buf.p = chain_buf.ptr = p;
        /* add pbuf to existing pbuf chain */
      } else {
        if (chain_buf.p->tot_len + p->len > 0xffff) {
          /* overflow */
          pbuf_free(p);
          goto sendmsg_emsgsize;
        }
        pbuf_cat(chain_buf.p, p);
      }
    }
    /* save size of total chain */
    if (err == ERR_OK) {
      size = netbuf_len(&chain_buf);
    }
#endif /* LWIP_NETIF_TX_SINGLE_PBUF */

    if (err == ERR_OK) {
#if LWIP_IPV4 && LWIP_IPV6
      /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
      if (IP_IS_V6_VAL(chain_buf.addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(&chain_buf.addr))) {
        unmap_ipv4_mapped_ipv6(ip_2_ip4(&chain_buf.addr), ip_2_ip6(&chain_buf.addr));
        IP_SET_TYPE_VAL(chain_buf.addr, IPADDR_TYPE_V4);
      }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

      /* send the data */
      err = netconn_send(sock->conn, &chain_buf);
    }

    /* deallocated the buffer */
    netbuf_free(&chain_buf);

    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return (err == ERR_OK ? size : -1);
sendmsg_emsgsize:
    sock_set_errno(sock, EMSGSIZE);
    netbuf_free(&chain_buf);
    done_socket(sock);
    return -1;
  }
#else /* LWIP_UDP || LWIP_RAW */
  sock_set_errno(sock, err_to_errno(ERR_ARG));
  done_socket(sock);
  return -1;
#endif /* LWIP_UDP || LWIP_RAW */
}

int
lwip_sendto(int s, const void *data, size_t size, int flags,
       const struct sockaddr *to, socklen_t tolen)
{
  struct lwip_sock *sock;
  err_t err;
  u16_t short_size;
  u16_t remote_port;
  struct netbuf buf;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP) {
#if LWIP_TCP
    done_socket(sock);
    return lwip_send(s, data, size, flags);
#else /* LWIP_TCP */
    LWIP_UNUSED_ARG(flags);
    sock_set_errno(sock, err_to_errno(ERR_ARG));
    done_socket(sock);
    return -1;
#endif /* LWIP_TCP */
  }

  if (size > 0xFFFF) {
    /* cannot fit into one datagram (at least for us) */
    sock_set_errno(sock, EMSGSIZE);
    done_socket(sock);
    return -1;
  }
  short_size = (u16_t)size;
  LWIP_ERROR("lwip_sendto: invalid address", (((to == NULL) && (tolen == 0)) ||
             (IS_SOCK_ADDR_LEN_VALID(tolen) &&
             IS_SOCK_ADDR_TYPE_VALID(to) && IS_SOCK_ADDR_ALIGNED(to))),
             sock_set_errno(sock, err_to_errno(ERR_ARG)); return -1;);
  LWIP_UNUSED_ARG(tolen);

  /* initialize a buffer */
  buf.p = buf.ptr = NULL;
#if LWIP_CHECKSUM_ON_COPY
  buf.flags = 0;
#endif /* LWIP_CHECKSUM_ON_COPY */
  if (to) {
    SOCKADDR_TO_IPADDR_PORT(to, &buf.addr, remote_port);
  } else {
    remote_port = 0;
    ip_addr_set_any(NETCONNTYPE_ISIPV6(netconn_type(sock->conn)), &buf.addr);
  }
  netbuf_fromport(&buf) = remote_port;


  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_sendto(%d, data=%p, short_size=%"U16_F", flags=0x%x to=",
              s, data, short_size, flags));
  ip_addr_debug_print(SOCKETS_DEBUG, &buf.addr);
  LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F"\n", remote_port));

  /* make the buffer point to the data that should be sent */
#if LWIP_NETIF_TX_SINGLE_PBUF
  /* Allocate a new netbuf and copy the data into it. */
  if (netbuf_alloc(&buf, short_size) == NULL) {
    err = ERR_MEM;
  } else {
#if LWIP_CHECKSUM_ON_COPY
    if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_RAW) {
      u16_t chksum = LWIP_CHKSUM_COPY(buf.p->payload, data, short_size);
      netbuf_set_chksum(&buf, chksum);
    } else
#endif /* LWIP_CHECKSUM_ON_COPY */
    {
      MEMCPY(buf.p->payload, data, short_size);
    }
    err = ERR_OK;
  }
#else /* LWIP_NETIF_TX_SINGLE_PBUF */
  err = netbuf_ref(&buf, data, short_size);
#endif /* LWIP_NETIF_TX_SINGLE_PBUF */
  if (err == ERR_OK) {
#if LWIP_IPV4 && LWIP_IPV6
    /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
    if (IP_IS_V6_VAL(buf.addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(&buf.addr))) {
      unmap_ipv4_mapped_ipv6(ip_2_ip4(&buf.addr), ip_2_ip6(&buf.addr));
      IP_SET_TYPE_VAL(buf.addr, IPADDR_TYPE_V4);
    }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

    /* send the data */
    err = netconn_send(sock->conn, &buf);
  }

  /* deallocated the buffer */
  netbuf_free(&buf);

  sock_set_errno(sock, err_to_errno(err));
  done_socket(sock);
  return (err == ERR_OK ? short_size : -1);
}

int
lwip_socket(int domain, int type, int protocol)
{
  struct netconn *conn;
  int i;

  LWIP_UNUSED_ARG(domain); /* @todo: check this */

  /* create a netconn */
  switch (type) {
  case SOCK_RAW:
    conn = netconn_new_with_proto_and_callback(DOMAIN_TO_NETCONN_TYPE(domain, NETCONN_RAW),
                                               (u8_t)protocol, DEFAULT_SOCKET_EVENTCB);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_socket(%s, SOCK_RAW, %d) = ",
                                 domain == PF_INET ? "PF_INET" : "UNKNOWN", protocol));
    break;
  case SOCK_DGRAM:
    conn = netconn_new_with_callback(DOMAIN_TO_NETCONN_TYPE(domain,
                 ((protocol == IPPROTO_UDPLITE) ? NETCONN_UDPLITE : NETCONN_UDP)) ,
                 DEFAULT_SOCKET_EVENTCB);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_socket(%s, SOCK_DGRAM, %d) = ",
                                 domain == PF_INET ? "PF_INET" : "UNKNOWN", protocol));
    break;
  case SOCK_STREAM:
    conn = netconn_new_with_callback(DOMAIN_TO_NETCONN_TYPE(domain, NETCONN_TCP), DEFAULT_SOCKET_EVENTCB);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_socket(%s, SOCK_STREAM, %d) = ",
                                 domain == PF_INET ? "PF_INET" : "UNKNOWN", protocol));
    break;
  default:
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_socket(%d, %d/UNKNOWN, %d) = -1\n",
                                 domain, type, protocol));
    set_errno(EINVAL);
    return -1;
  }

  if (!conn) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("-1 / ENOBUFS (could not create netconn)\n"));
    set_errno(ENOBUFS);
    return -1;
  }

  i = alloc_socket(conn, 0);

  if (i == -1) {
    netconn_delete(conn);
    set_errno(ENFILE);
    return -1;
  }
  conn->socket = i;
  done_socket(&sockets[i - LWIP_SOCKET_OFFSET]);
  LWIP_DEBUGF(SOCKETS_DEBUG, ("%d\n", i));
  set_errno(0);
  return i;
}

int
lwip_write(int s, const void *data, size_t size)
{
  return lwip_send(s, data, size, 0);
}

int
lwip_writev(int s, const struct iovec *iov, int iovcnt)
{
  struct msghdr msg;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  /* Hack: we have to cast via number to cast from 'const' pointer to non-const.
     Blame the opengroup standard for this inconsistency. */
  msg.msg_iov = LWIP_CONST_CAST(struct iovec *, iov);
  msg.msg_iovlen = iovcnt;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  return lwip_sendmsg(s, &msg, 0);
}

#if LWIP_SOCKET_SELECT
/**
 * Go through the readset and writeset lists and see which socket of the sockets
 * set in the sets has events. On return, readset, writeset and exceptset have
 * the sockets enabled that had events.
 *
 * @param maxfdp1 the highest socket index in the sets
 * @param readset_in    set of sockets to check for read events
 * @param writeset_in   set of sockets to check for write events
 * @param exceptset_in  set of sockets to check for error events
 * @param readset_out   set of sockets that had read events
 * @param writeset_out  set of sockets that had write events
 * @param exceptset_out set os sockets that had error events
 * @return number of sockets that had events (read/write/exception) (>= 0)
 */
static int
lwip_selscan(int maxfdp1, fd_set *readset_in, fd_set *writeset_in, fd_set *exceptset_in,
             fd_set *readset_out, fd_set *writeset_out, fd_set *exceptset_out)
{
  int i, nready = 0;
  fd_set lreadset, lwriteset, lexceptset;
  struct lwip_sock *sock;
  SYS_ARCH_DECL_PROTECT(lev);

  FD_ZERO(&lreadset);
  FD_ZERO(&lwriteset);
  FD_ZERO(&lexceptset);

  /* Go through each socket in each list to count number of sockets which
     currently match */
  for (i = LWIP_SOCKET_OFFSET; i < maxfdp1; i++) {
    /* if this FD is not in the set, continue */
    if (!(readset_in && FD_ISSET(i, readset_in)) &&
        !(writeset_in && FD_ISSET(i, writeset_in)) &&
        !(exceptset_in && FD_ISSET(i, exceptset_in))) {
      continue;
    }
    /* First get the socket's status (protected)... */
    SYS_ARCH_PROTECT(lev);
    sock = tryget_socket_unconn(i);
    if (sock != NULL) {
      void* lastdata = sock->lastdata.pbuf;
      s16_t rcvevent = sock->rcvevent;
      u16_t sendevent = sock->sendevent;
      u16_t errevent = sock->errevent;
      SYS_ARCH_UNPROTECT(lev);

      /* ... then examine it: */
      /* See if netconn of this socket is ready for read */
      if (readset_in && FD_ISSET(i, readset_in) && ((lastdata != NULL) || (rcvevent > 0))) {
        FD_SET(i, &lreadset);
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_selscan: fd=%d ready for reading\n", i));
        nready++;
      }
      /* See if netconn of this socket is ready for write */
      if (writeset_in && FD_ISSET(i, writeset_in) && (sendevent != 0)) {
        FD_SET(i, &lwriteset);
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_selscan: fd=%d ready for writing\n", i));
        nready++;
      }
      /* See if netconn of this socket had an error */
      if (exceptset_in && FD_ISSET(i, exceptset_in) && (errevent != 0)) {
        FD_SET(i, &lexceptset);
        LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_selscan: fd=%d ready for exception\n", i));
        nready++;
      }
      done_socket(sock);
    } else {
      SYS_ARCH_UNPROTECT(lev);
      /* no a valid open socket */
      return -1;
    }
  }
  /* copy local sets to the ones provided as arguments */
  *readset_out = lreadset;
  *writeset_out = lwriteset;
  *exceptset_out = lexceptset;

  LWIP_ASSERT("nready >= 0", nready >= 0);
  return nready;
}

#if LWIP_NETCONN_FULLDUPLEX
/* Mark all of the set sockets in one of the three fdsets passed to select as used.
 * All sockets are marked (and later unmarked), whether they are open or not.
 * This is OK as lwip_selscan aborts select when non-open sockets are found.
 */
static void
lwip_select_inc_sockets_used_set(int maxfdp, fd_set *fdset, fd_set *used_sockets)
{
  SYS_ARCH_DECL_PROTECT(lev);
  if (fdset) {
    int i;
    for (i = LWIP_SOCKET_OFFSET; i < maxfdp; i++) {
      /* if this FD is in the set, lock it (unless already done) */
      if (FD_ISSET(i, fdset) && !FD_ISSET(i, used_sockets)) {
        struct lwip_sock *sock;
        SYS_ARCH_PROTECT(lev);
        sock = tryget_socket_unconn(i);
        if (sock != NULL) {
          sock_inc_used(sock);
          FD_SET(i, used_sockets);
        }
        SYS_ARCH_UNPROTECT(lev);
      }
    }
  }
}

/* Mark all sockets passed to select as used to prevent them from being freed
 * from other threads while select is running.
 * Marked sockets are added to 'used_sockets' to mark them only once an be able
 * to unmark them correctly.
 */
static void
lwip_select_inc_sockets_used(int maxfdp, fd_set *fdset1, fd_set *fdset2, fd_set *fdset3, fd_set *used_sockets)
{
  FD_ZERO(used_sockets);
  lwip_select_inc_sockets_used_set(maxfdp, fdset1, used_sockets);
  lwip_select_inc_sockets_used_set(maxfdp, fdset2, used_sockets);
  lwip_select_inc_sockets_used_set(maxfdp, fdset3, used_sockets);
}

/* Let go all sockets that were marked as used when starting select */
static void
lwip_select_dec_sockets_used(int maxfdp, fd_set *used_sockets)
{
  int i;
  for (i = LWIP_SOCKET_OFFSET; i < maxfdp; i++) {
    /* if this FD is not in the set, continue */
    if (FD_ISSET(i, used_sockets)) {
      struct lwip_sock *sock = tryget_socket_unconn(i);
      LWIP_ASSERT("socket gone at the end of select", sock != NULL);
      if (sock != NULL) {
        done_socket(sock);
      }
    }
  }
}
#else /* LWIP_NETCONN_FULLDUPLEX */
#define lwip_select_inc_sockets_used(maxfdp1, readset, writeset, exceptset, used_sockets)
#define lwip_select_dec_sockets_used(maxfdp1, used_sockets)
#endif /* LWIP_NETCONN_FULLDUPLEX */

int
lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset,
            struct timeval *timeout)
{
  u32_t waitres = 0;
  int nready;
  fd_set lreadset, lwriteset, lexceptset;
  u32_t msectimeout;
  struct lwip_select_cb select_cb;
  int i;
  int maxfdp2;
#if LWIP_NETCONN_SEM_PER_THREAD
  int waited = 0;
#endif
#if LWIP_NETCONN_FULLDUPLEX
  fd_set used_sockets;
#endif
  SYS_ARCH_DECL_PROTECT(lev);

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_select(%d, %p, %p, %p, tvsec=%"S32_F" tvusec=%"S32_F")\n",
                  maxfdp1, (void *)readset, (void *) writeset, (void *) exceptset,
                  timeout ? (s32_t)timeout->tv_sec : (s32_t)-1,
                  timeout ? (s32_t)timeout->tv_usec : (s32_t)-1));

  if ((maxfdp1 < 0) || (maxfdp1 > (FD_SETSIZE + LWIP_SOCKET_OFFSET))) {
    set_errno(EINVAL);
    return -1;
  }

  lwip_select_inc_sockets_used(maxfdp1, readset, writeset, exceptset, &used_sockets);

  /* Go through each socket in each list to count number of sockets which
     currently match */
  nready = lwip_selscan(maxfdp1, readset, writeset, exceptset, &lreadset, &lwriteset, &lexceptset);

  if (nready < 0) {
    set_errno(EBADF);
    return -1;
  }

  /* If we don't have any current events, then suspend if we are supposed to */
  if (!nready) {
    if (timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0) {
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_select: no timeout, returning 0\n"));
      /* This is OK as the local fdsets are empty and nready is zero,
         or we would have returned earlier. */
      goto return_copy_fdsets;
    }

    /* None ready: add our semaphore to list:
       We don't actually need any dynamic memory. Our entry on the
       list is only valid while we are in this function, so it's ok
       to use local variables. */

    select_cb.next = NULL;
    select_cb.prev = NULL;
    select_cb.readset = readset;
    select_cb.writeset = writeset;
    select_cb.exceptset = exceptset;
    select_cb.sem_signalled = 0;
#if LWIP_NETCONN_SEM_PER_THREAD
    select_cb.sem = LWIP_NETCONN_THREAD_SEM_GET();
#else /* LWIP_NETCONN_SEM_PER_THREAD */
    if (sys_sem_new(&select_cb.sem, 0) != ERR_OK) {
      /* failed to create semaphore */
      set_errno(ENOMEM);
      lwip_select_dec_sockets_used(maxfdp1, &used_sockets);
      return -1;
    }
#endif /* LWIP_NETCONN_SEM_PER_THREAD */

    /* Protect the select_cb_list */
    SYS_ARCH_PROTECT(lev);

    /* Put this select_cb on top of list */
    select_cb.next = select_cb_list;
    if (select_cb_list != NULL) {
      select_cb_list->prev = &select_cb;
    }
    select_cb_list = &select_cb;
    /* Increasing this counter tells event_callback that the list has changed. */
    select_cb_ctr++;

    /* Now we can safely unprotect */
    SYS_ARCH_UNPROTECT(lev);

    /* Increase select_waiting for each socket we are interested in */
    maxfdp2 = maxfdp1;
    for (i = LWIP_SOCKET_OFFSET; i < maxfdp1; i++) {
      if ((readset && FD_ISSET(i, readset)) ||
          (writeset && FD_ISSET(i, writeset)) ||
          (exceptset && FD_ISSET(i, exceptset))) {
        struct lwip_sock *sock;
        SYS_ARCH_PROTECT(lev);
        sock = tryget_socket_unconn(i);
        if (sock != NULL) {
          sock->select_waiting++;
          LWIP_ASSERT("sock->select_waiting > 0", sock->select_waiting > 0);
          done_socket(sock);
        } else {
          /* Not a valid socket */
          nready = -1;
          maxfdp2 = i;
          SYS_ARCH_UNPROTECT(lev);
          break;
        }
        SYS_ARCH_UNPROTECT(lev);
      }
    }

    if (nready >= 0) {
      /* Call lwip_selscan again: there could have been events between
         the last scan (without us on the list) and putting us on the list! */
      nready = lwip_selscan(maxfdp1, readset, writeset, exceptset, &lreadset, &lwriteset, &lexceptset);
      if (!nready) {
        /* Still none ready, just wait to be woken */
        if (timeout == 0) {
          /* Wait forever */
          msectimeout = 0;
        } else {
          msectimeout =  ((timeout->tv_sec * 1000) + ((timeout->tv_usec + 500)/1000));
          if (msectimeout == 0) {
            /* Wait 1ms at least (0 means wait forever) */
            msectimeout = 1;
          }
        }

        waitres = sys_arch_sem_wait(SELECT_SEM_PTR(select_cb.sem), msectimeout);
#if LWIP_NETCONN_SEM_PER_THREAD
        waited = 1;
#endif
      }
    }

    /* Decrease select_waiting for each socket we are interested in */
    for (i = LWIP_SOCKET_OFFSET; i < maxfdp2; i++) {
      if ((readset && FD_ISSET(i, readset)) ||
          (writeset && FD_ISSET(i, writeset)) ||
          (exceptset && FD_ISSET(i, exceptset))) {
        struct lwip_sock *sock;
        SYS_ARCH_PROTECT(lev);
        sock = tryget_socket_unconn(i);
        if (sock != NULL) {
          /* for now, handle select_waiting==0... */
          LWIP_ASSERT("sock->select_waiting > 0", sock->select_waiting > 0);
          if (sock->select_waiting > 0) {
            sock->select_waiting--;
          }
          done_socket(sock);
        } else {
          /* Not a valid socket */
          nready = -1;
        }
        SYS_ARCH_UNPROTECT(lev);
      }
    }
    /* Take us off the list */
    SYS_ARCH_PROTECT(lev);
    if (select_cb.next != NULL) {
      select_cb.next->prev = select_cb.prev;
    }
    if (select_cb_list == &select_cb) {
      LWIP_ASSERT("select_cb.prev == NULL", select_cb.prev == NULL);
      select_cb_list = select_cb.next;
    } else {
      LWIP_ASSERT("select_cb.prev != NULL", select_cb.prev != NULL);
      select_cb.prev->next = select_cb.next;
    }
    /* Increasing this counter tells event_callback that the list has changed. */
    select_cb_ctr++;
    SYS_ARCH_UNPROTECT(lev);

#if LWIP_NETCONN_SEM_PER_THREAD
    if (select_cb.sem_signalled && (!waited || (waitres == SYS_ARCH_TIMEOUT))) {
      /* don't leave the thread-local semaphore signalled */
      sys_arch_sem_wait(select_cb.sem, 1);
    }
#else /* LWIP_NETCONN_SEM_PER_THREAD */
    sys_sem_free(&select_cb.sem);
#endif /* LWIP_NETCONN_SEM_PER_THREAD */

    if (nready < 0) {
      /* This happens when a socket got closed while waiting */
      set_errno(EBADF);
      lwip_select_dec_sockets_used(maxfdp1, &used_sockets);
      return -1;
    }

    if (waitres == SYS_ARCH_TIMEOUT) {
      /* Timeout */
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_select: timeout expired\n"));
      /* This is OK as the local fdsets are empty and nready is zero,
         or we would have returned earlier. */
      goto return_copy_fdsets;
    }

    /* See what's set */
    nready = lwip_selscan(maxfdp1, readset, writeset, exceptset, &lreadset, &lwriteset, &lexceptset);
  }

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_select: nready=%d\n", nready));
return_copy_fdsets:
  lwip_select_dec_sockets_used(maxfdp1, &used_sockets);
  set_errno(0);
  if (readset) {
    *readset = lreadset;
  }
  if (writeset) {
    *writeset = lwriteset;
  }
  if (exceptset) {
    *exceptset = lexceptset;
  }
  return nready;
}

/**
 * Callback registered in the netconn layer for each socket-netconn.
 * Processes recvevent (data available) and wakes up tasks waiting for select.
 */
static void
event_callback(struct netconn *conn, enum netconn_evt evt, u16_t len)
{
  int s;
  struct lwip_sock *sock;
  struct lwip_select_cb *scb;
  int last_select_cb_ctr;
  SYS_ARCH_DECL_PROTECT(lev);

  LWIP_UNUSED_ARG(len);

  /* Get socket */
  if (conn) {
    s = conn->socket;
    if (s < 0) {
      /* Data comes in right away after an accept, even though
       * the server task might not have created a new socket yet.
       * Just count down (or up) if that's the case and we
       * will use the data later. Note that only receive events
       * can happen before the new socket is set up. */
      SYS_ARCH_PROTECT(lev);
      if (conn->socket < 0) {
        if (evt == NETCONN_EVT_RCVPLUS) {
          /* conn->socket is -1 on initialization
             lwip_accept adjusts sock->recvevent if conn->socket < -1 */
          conn->socket--;
        }
        SYS_ARCH_UNPROTECT(lev);
        return;
      }
      s = conn->socket;
      SYS_ARCH_UNPROTECT(lev);
    }

    sock = get_socket(s);
    if (!sock) {
      return;
    }
  } else {
    return;
  }

  SYS_ARCH_PROTECT(lev);
  /* Set event as required */
  switch (evt) {
    case NETCONN_EVT_RCVPLUS:
      sock->rcvevent++;
      break;
    case NETCONN_EVT_RCVMINUS:
      sock->rcvevent--;
      break;
    case NETCONN_EVT_SENDPLUS:
      sock->sendevent = 1;
      break;
    case NETCONN_EVT_SENDMINUS:
      sock->sendevent = 0;
      break;
    case NETCONN_EVT_ERROR:
      sock->errevent = 1;
      break;
    default:
      LWIP_ASSERT("unknown event", 0);
      break;
  }

  if (sock->select_waiting == 0) {
    /* noone is waiting for this socket, no need to check select_cb_list */
    SYS_ARCH_UNPROTECT(lev);
    done_socket(sock);
    return;
  }

  /* Now decide if anyone is waiting for this socket */
  /* NOTE: This code goes through the select_cb_list list multiple times
     ONLY IF a select was actually waiting. We go through the list the number
     of waiting select calls + 1. This list is expected to be small. */

  /* At this point, SYS_ARCH is still protected! */
again:
  for (scb = select_cb_list; scb != NULL; scb = scb->next) {
    /* remember the state of select_cb_list to detect changes */
    last_select_cb_ctr = select_cb_ctr;
    if (scb->sem_signalled == 0) {
      /* semaphore not signalled yet */
      int do_signal = 0;
      /* Test this select call for our socket */
      if (sock->rcvevent > 0) {
        if (scb->readset && FD_ISSET(s, scb->readset)) {
          do_signal = 1;
        }
      }
      if (sock->sendevent != 0) {
        if (!do_signal && scb->writeset && FD_ISSET(s, scb->writeset)) {
          do_signal = 1;
        }
      }
      if (sock->errevent != 0) {
        if (!do_signal && scb->exceptset && FD_ISSET(s, scb->exceptset)) {
          do_signal = 1;
        }
      }
      if (do_signal) {
        scb->sem_signalled = 1;
        /* Don't call SYS_ARCH_UNPROTECT() before signaling the semaphore, as this might
           lead to the select thread taking itself off the list, invalidating the semaphore. */
        sys_sem_signal(SELECT_SEM_PTR(scb->sem));
      }
    }
    /* unlock interrupts with each step */
    SYS_ARCH_UNPROTECT(lev);
    /* this makes sure interrupt protection time is short */
    SYS_ARCH_PROTECT(lev);
    if (last_select_cb_ctr != select_cb_ctr) {
      /* someone has changed select_cb_list, restart at the beginning */
      goto again;
    }
  }
  SYS_ARCH_UNPROTECT(lev);
  done_socket(sock);
}
#endif /* LWIP_SOCKET_SELECT */

/**
 * Close one end of a full-duplex connection.
 */
int
lwip_shutdown(int s, int how)
{
  struct lwip_sock *sock;
  err_t err;
  u8_t shut_rx = 0, shut_tx = 0;

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_shutdown(%d, how=%d)\n", s, how));

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (sock->conn != NULL) {
    if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_TCP) {
      sock_set_errno(sock, EOPNOTSUPP);
      done_socket(sock);
      return -1;
    }
  } else {
    sock_set_errno(sock, ENOTCONN);
    done_socket(sock);
    return -1;
  }

  if (how == SHUT_RD) {
    shut_rx = 1;
  } else if (how == SHUT_WR) {
    shut_tx = 1;
  } else if (how == SHUT_RDWR) {
    shut_rx = 1;
    shut_tx = 1;
  } else {
    sock_set_errno(sock, EINVAL);
    done_socket(sock);
    return -1;
  }
  err = netconn_shutdown(sock->conn, shut_rx, shut_tx);

  sock_set_errno(sock, err_to_errno(err));
  done_socket(sock);
  return (err == ERR_OK ? 0 : -1);
}

static int
lwip_getaddrname(int s, struct sockaddr *name, socklen_t *namelen, u8_t local)
{
  struct lwip_sock *sock;
  union sockaddr_aligned saddr;
  ip_addr_t naddr;
  u16_t port;
  err_t err;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  /* get the IP address and port */
  err = netconn_getaddr(sock->conn, &naddr, &port, local);
  if (err != ERR_OK) {
    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return -1;
  }

#if LWIP_IPV4 && LWIP_IPV6
  /* Dual-stack: Map IPv4 addresses to IPv4 mapped IPv6 */
  if (NETCONNTYPE_ISIPV6(netconn_type(sock->conn)) &&
      IP_IS_V4_VAL(naddr)) {
    ip4_2_ipv4_mapped_ipv6(ip_2_ip6(&naddr), ip_2_ip4(&naddr));
    IP_SET_TYPE_VAL(naddr, IPADDR_TYPE_V6);
  }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

  IPADDR_PORT_TO_SOCKADDR(&saddr, &naddr, port);

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getaddrname(%d, addr=", s));
  ip_addr_debug_print_val(SOCKETS_DEBUG, naddr);
  LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F")\n", port));

  if (*namelen > saddr.sa.sa_len) {
    *namelen = saddr.sa.sa_len;
  }
  MEMCPY(name, &saddr, *namelen);

  sock_set_errno(sock, 0);
  done_socket(sock);
  return 0;
}

int
lwip_getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
  return lwip_getaddrname(s, name, namelen, 0);
}

int
lwip_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
  return lwip_getaddrname(s, name, namelen, 1);
}

int
lwip_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
  int err;
  struct lwip_sock *sock = get_socket(s);
#if !LWIP_TCPIP_CORE_LOCKING
  err_t cberr;
  LWIP_SETGETSOCKOPT_DATA_VAR_DECLARE(data);
#endif /* !LWIP_TCPIP_CORE_LOCKING */

  if (!sock) {
    return -1;
  }

  if ((NULL == optval) || (NULL == optlen)) {
    sock_set_errno(sock, EFAULT);
    done_socket(sock);
    return -1;
  }

#if LWIP_TCPIP_CORE_LOCKING
  /* core-locking can just call the -impl function */
  LOCK_TCPIP_CORE();
  err = lwip_getsockopt_impl(s, level, optname, optval, optlen);
  UNLOCK_TCPIP_CORE();

#else /* LWIP_TCPIP_CORE_LOCKING */

#if LWIP_MPU_COMPATIBLE
  /* MPU_COMPATIBLE copies the optval data, so check for max size here */
  if (*optlen > LWIP_SETGETSOCKOPT_MAXOPTLEN) {
    sock_set_errno(sock, ENOBUFS);
    done_socket(sock);
    return -1;
  }
#endif /* LWIP_MPU_COMPATIBLE */

  LWIP_SETGETSOCKOPT_DATA_VAR_ALLOC(data, sock);
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).s = s;
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).level = level;
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optname = optname;
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optlen = *optlen;
#if !LWIP_MPU_COMPATIBLE
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optval.p = optval;
#endif /* !LWIP_MPU_COMPATIBLE */
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).err = 0;
#if LWIP_NETCONN_SEM_PER_THREAD
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).completed_sem = LWIP_NETCONN_THREAD_SEM_GET();
#else
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).completed_sem = &sock->conn->op_completed;
#endif
  cberr = tcpip_callback(lwip_getsockopt_callback, &LWIP_SETGETSOCKOPT_DATA_VAR_REF(data));
  if (cberr != ERR_OK) {
    LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
    sock_set_errno(sock, err_to_errno(cberr));
    done_socket(sock);
    return -1;
  }
  sys_arch_sem_wait((sys_sem_t*)(LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).completed_sem), 0);

  /* write back optlen and optval */
  *optlen = LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optlen;
#if LWIP_MPU_COMPATIBLE
  MEMCPY(optval, LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optval,
    LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optlen);
#endif /* LWIP_MPU_COMPATIBLE */

  /* maybe lwip_getsockopt_internal has changed err */
  err = LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).err;
  LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
#endif /* LWIP_TCPIP_CORE_LOCKING */

  sock_set_errno(sock, err);
  done_socket(sock);
  return err ? -1 : 0;
}

#if !LWIP_TCPIP_CORE_LOCKING
/** lwip_getsockopt_callback: only used without CORE_LOCKING
 * to get into the tcpip_thread
 */
static void
lwip_getsockopt_callback(void *arg)
{
  struct lwip_setgetsockopt_data *data;
  LWIP_ASSERT("arg != NULL", arg != NULL);
  data = (struct lwip_setgetsockopt_data*)arg;

  data->err = lwip_getsockopt_impl(data->s, data->level, data->optname,
#if LWIP_MPU_COMPATIBLE
    data->optval,
#else /* LWIP_MPU_COMPATIBLE */
    data->optval.p,
#endif /* LWIP_MPU_COMPATIBLE */
    &data->optlen);

  sys_sem_signal((sys_sem_t*)(data->completed_sem));
}
#endif  /* LWIP_TCPIP_CORE_LOCKING */

/** lwip_getsockopt_impl: the actual implementation of getsockopt:
 * same argument as lwip_getsockopt, either called directly or through callback
 */
static int
lwip_getsockopt_impl(int s, int level, int optname, void *optval, socklen_t *optlen)
{
  int err = 0;
  struct lwip_sock *sock = tryget_socket(s);
  if (!sock) {
    return EBADF;
  }

  switch (level) {

/* Level: SOL_SOCKET */
  case SOL_SOCKET:
    switch (optname) {

#if LWIP_TCP
    case SO_ACCEPTCONN:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
      if (NETCONNTYPE_GROUP(sock->conn->type) != NETCONN_TCP) {
        done_socket(sock);
        return ENOPROTOOPT;
      }
      if ((sock->conn->pcb.tcp != NULL) && (sock->conn->pcb.tcp->state == LISTEN)) {
        *(int*)optval = 1;
      } else {
        *(int*)optval = 0;
      }
      break;
#endif /* LWIP_TCP */

    /* The option flags */
    case SO_BROADCAST:
    case SO_KEEPALIVE:
#if SO_REUSE
    case SO_REUSEADDR:
#endif /* SO_REUSE */
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
      *(int*)optval = ip_get_option(sock->conn->pcb.ip, optname);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, SOL_SOCKET, optname=0x%x, ..) = %s\n",
                                  s, optname, (*(int*)optval?"on":"off")));
      break;

    case SO_TYPE:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, int);
      switch (NETCONNTYPE_GROUP(netconn_type(sock->conn))) {
      case NETCONN_RAW:
        *(int*)optval = SOCK_RAW;
        break;
      case NETCONN_TCP:
        *(int*)optval = SOCK_STREAM;
        break;
      case NETCONN_UDP:
        *(int*)optval = SOCK_DGRAM;
        break;
      default: /* unrecognized socket type */
        *(int*)optval = netconn_type(sock->conn);
        LWIP_DEBUGF(SOCKETS_DEBUG,
                    ("lwip_getsockopt(%d, SOL_SOCKET, SO_TYPE): unrecognized socket type %d\n",
                    s, *(int *)optval));
      }  /* switch (netconn_type(sock->conn)) */
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, SOL_SOCKET, SO_TYPE) = %d\n",
                  s, *(int *)optval));
      break;

    case SO_ERROR:
      LWIP_SOCKOPT_CHECK_OPTLEN(*optlen, int);
      /* only overwrite ERR_OK or temporary errors */
      if (((sock->err == 0) || (sock->err == EINPROGRESS)) && (sock->conn != NULL)) {
        sock_set_errno(sock, err_to_errno(sock->conn->last_err));
      }
      *(int *)optval = sock->err;
      sock->err = 0;
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, SOL_SOCKET, SO_ERROR) = %d\n",
                  s, *(int *)optval));
      break;

#if LWIP_SO_SNDTIMEO
    case SO_SNDTIMEO:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, LWIP_SO_SNDRCVTIMEO_OPTTYPE);
      LWIP_SO_SNDRCVTIMEO_SET(optval, netconn_get_sendtimeout(sock->conn));
      break;
#endif /* LWIP_SO_SNDTIMEO */
#if LWIP_SO_RCVTIMEO
    case SO_RCVTIMEO:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, LWIP_SO_SNDRCVTIMEO_OPTTYPE);
      LWIP_SO_SNDRCVTIMEO_SET(optval, netconn_get_recvtimeout(sock->conn));
      break;
#endif /* LWIP_SO_RCVTIMEO */
#if LWIP_SO_RCVBUF
    case SO_RCVBUF:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, int);
      *(int *)optval = netconn_get_recvbufsize(sock->conn);
      break;
#endif /* LWIP_SO_RCVBUF */
#if LWIP_SO_LINGER
    case SO_LINGER:
      {
        s16_t conn_linger;
        struct linger* linger = (struct linger*)optval;
        LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, struct linger);
        conn_linger = sock->conn->linger;
        if (conn_linger >= 0) {
          linger->l_onoff = 1;
          linger->l_linger = (int)conn_linger;
        } else {
          linger->l_onoff = 0;
          linger->l_linger = 0;
        }
      }
      break;
#endif /* LWIP_SO_LINGER */
#if LWIP_UDP
    case SO_NO_CHECK:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, *optlen, int, NETCONN_UDP);
#if LWIP_UDPLITE
      if ((udp_flags(sock->conn->pcb.udp) & UDP_FLAGS_UDPLITE) != 0) {
        /* this flag is only available for UDP, not for UDP lite */
        done_socket(sock);
        return EAFNOSUPPORT;
      }
#endif /* LWIP_UDPLITE */
      *(int*)optval = (udp_flags(sock->conn->pcb.udp) & UDP_FLAGS_NOCHKSUM) ? 1 : 0;
      break;
#endif /* LWIP_UDP*/
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, SOL_SOCKET, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;

/* Level: IPPROTO_IP */
  case IPPROTO_IP:
    switch (optname) {
    case IP_TTL:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
      *(int*)optval = sock->conn->pcb.ip->ttl;
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IP, IP_TTL) = %d\n",
                  s, *(int *)optval));
      break;
    case IP_TOS:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
      *(int*)optval = sock->conn->pcb.ip->tos;
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IP, IP_TOS) = %d\n",
                  s, *(int *)optval));
      break;
#if LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS
    case IP_MULTICAST_TTL:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, u8_t);
      if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_UDP) {
        done_socket(sock);
        return ENOPROTOOPT;
      }
      *(u8_t*)optval = udp_get_multicast_ttl(sock->conn->pcb.udp);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IP, IP_MULTICAST_TTL) = %d\n",
                  s, *(int *)optval));
      break;
    case IP_MULTICAST_IF:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, struct in_addr);
      if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_UDP) {
        done_socket(sock);
        return ENOPROTOOPT;
      }
      inet_addr_from_ip4addr((struct in_addr*)optval, udp_get_multicast_netif_addr(sock->conn->pcb.udp));
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IP, IP_MULTICAST_IF) = 0x%"X32_F"\n",
                  s, *(u32_t *)optval));
      break;
    case IP_MULTICAST_LOOP:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, u8_t);
      if ((sock->conn->pcb.udp->flags & UDP_FLAGS_MULTICAST_LOOP) != 0) {
        *(u8_t*)optval = 1;
      } else {
        *(u8_t*)optval = 0;
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IP, IP_MULTICAST_LOOP) = %d\n",
                  s, *(int *)optval));
      break;
#endif /* LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS */
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IP, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;

#if LWIP_TCP
/* Level: IPPROTO_TCP */
  case IPPROTO_TCP:
    /* Special case: all IPPROTO_TCP option take an int */
    LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, *optlen, int, NETCONN_TCP);
    if (sock->conn->pcb.tcp->state == LISTEN) {
      done_socket(sock);
      return EINVAL;
    }
    switch (optname) {
    case TCP_NODELAY:
      *(int*)optval = tcp_nagle_disabled(sock->conn->pcb.tcp);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_TCP, TCP_NODELAY) = %s\n",
                  s, (*(int*)optval)?"on":"off") );
      break;
    case TCP_KEEPALIVE:
      *(int*)optval = (int)sock->conn->pcb.tcp->keep_idle;
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_TCP, TCP_KEEPALIVE) = %d\n",
                  s, *(int *)optval));
      break;

#if LWIP_TCP_KEEPALIVE
    case TCP_KEEPIDLE:
      *(int*)optval = (int)(sock->conn->pcb.tcp->keep_idle/1000);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_TCP, TCP_KEEPIDLE) = %d\n",
                  s, *(int *)optval));
      break;
    case TCP_KEEPINTVL:
      *(int*)optval = (int)(sock->conn->pcb.tcp->keep_intvl/1000);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_TCP, TCP_KEEPINTVL) = %d\n",
                  s, *(int *)optval));
      break;
    case TCP_KEEPCNT:
      *(int*)optval = (int)sock->conn->pcb.tcp->keep_cnt;
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_TCP, TCP_KEEPCNT) = %d\n",
                  s, *(int *)optval));
      break;
#endif /* LWIP_TCP_KEEPALIVE */
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_TCP, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
#endif /* LWIP_TCP */

#if LWIP_IPV6
/* Level: IPPROTO_IPV6 */
  case IPPROTO_IPV6:
    switch (optname) {
    case IPV6_V6ONLY:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, int);
      *(int*)optval = (netconn_get_ipv6only(sock->conn) ? 1 : 0);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IPV6, IPV6_V6ONLY) = %d\n",
                  s, *(int *)optval));
      break;
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_IPV6, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
#endif /* LWIP_IPV6 */

#if LWIP_UDP && LWIP_UDPLITE
  /* Level: IPPROTO_UDPLITE */
  case IPPROTO_UDPLITE:
    /* Special case: all IPPROTO_UDPLITE option take an int */
    LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
    /* If this is no UDP lite socket, ignore any options. */
    if (!NETCONNTYPE_ISUDPLITE(netconn_type(sock->conn))) {
      done_socket(sock);
      return ENOPROTOOPT;
    }
    switch (optname) {
    case UDPLITE_SEND_CSCOV:
      *(int*)optval = sock->conn->pcb.udp->chksum_len_tx;
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV) = %d\n",
                  s, (*(int*)optval)) );
      break;
    case UDPLITE_RECV_CSCOV:
      *(int*)optval = sock->conn->pcb.udp->chksum_len_rx;
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV) = %d\n",
                  s, (*(int*)optval)) );
      break;
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_UDPLITE, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
#endif /* LWIP_UDP */
  /* Level: IPPROTO_RAW */
  case IPPROTO_RAW:
    switch (optname) {
#if LWIP_IPV6 && LWIP_RAW
    case IPV6_CHECKSUM:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, *optlen, int, NETCONN_RAW);
      if (sock->conn->pcb.raw->chksum_reqd == 0) {
        *(int *)optval = -1;
      } else {
        *(int *)optval = sock->conn->pcb.raw->chksum_offset;
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_RAW, IPV6_CHECKSUM) = %d\n",
                  s, (*(int*)optval)) );
      break;
#endif /* LWIP_IPV6 && LWIP_RAW */
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, IPPROTO_RAW, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
  default:
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_getsockopt(%d, level=0x%x, UNIMPL: optname=0x%x, ..)\n",
                                s, level, optname));
    err = ENOPROTOOPT;
    break;
  } /* switch (level) */

  done_socket(sock);
  return err;
}

int
lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
  int err = 0;
  struct lwip_sock *sock = get_socket(s);
#if !LWIP_TCPIP_CORE_LOCKING
  err_t cberr;
  LWIP_SETGETSOCKOPT_DATA_VAR_DECLARE(data);
#endif /* !LWIP_TCPIP_CORE_LOCKING */

  if (!sock) {
    return -1;
  }

  if (NULL == optval) {
    sock_set_errno(sock, EFAULT);
    done_socket(sock);
    return -1;
  }

#if LWIP_TCPIP_CORE_LOCKING
  /* core-locking can just call the -impl function */
  LOCK_TCPIP_CORE();
  err = lwip_setsockopt_impl(s, level, optname, optval, optlen);
  UNLOCK_TCPIP_CORE();

#else /* LWIP_TCPIP_CORE_LOCKING */

#if LWIP_MPU_COMPATIBLE
  /* MPU_COMPATIBLE copies the optval data, so check for max size here */
  if (optlen > LWIP_SETGETSOCKOPT_MAXOPTLEN) {
    sock_set_errno(sock, ENOBUFS);
    done_socket(sock);
    return -1;
  }
#endif /* LWIP_MPU_COMPATIBLE */

  LWIP_SETGETSOCKOPT_DATA_VAR_ALLOC(data, sock);
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).s = s;
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).level = level;
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optname = optname;
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optlen = optlen;
#if LWIP_MPU_COMPATIBLE
  MEMCPY(LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optval, optval, optlen);
#else /* LWIP_MPU_COMPATIBLE */
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).optval.pc = (const void*)optval;
#endif /* LWIP_MPU_COMPATIBLE */
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).err = 0;
#if LWIP_NETCONN_SEM_PER_THREAD
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).completed_sem = LWIP_NETCONN_THREAD_SEM_GET();
#else
  LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).completed_sem = &sock->conn->op_completed;
#endif
  cberr = tcpip_callback(lwip_setsockopt_callback, &LWIP_SETGETSOCKOPT_DATA_VAR_REF(data));
  if (cberr != ERR_OK) {
    LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
    sock_set_errno(sock, err_to_errno(cberr));
    done_socket(sock);
    return -1;
  }
  sys_arch_sem_wait((sys_sem_t*)(LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).completed_sem), 0);

  /* maybe lwip_getsockopt_internal has changed err */
  err = LWIP_SETGETSOCKOPT_DATA_VAR_REF(data).err;
  LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
#endif  /* LWIP_TCPIP_CORE_LOCKING */

  sock_set_errno(sock, err);
  done_socket(sock);
  return err ? -1 : 0;
}

#if !LWIP_TCPIP_CORE_LOCKING
/** lwip_setsockopt_callback: only used without CORE_LOCKING
 * to get into the tcpip_thread
 */
static void
lwip_setsockopt_callback(void *arg)
{
  struct lwip_setgetsockopt_data *data;
  LWIP_ASSERT("arg != NULL", arg != NULL);
  data = (struct lwip_setgetsockopt_data*)arg;

  data->err = lwip_setsockopt_impl(data->s, data->level, data->optname,
#if LWIP_MPU_COMPATIBLE
    data->optval,
#else /* LWIP_MPU_COMPATIBLE */
    data->optval.pc,
#endif /* LWIP_MPU_COMPATIBLE */
    data->optlen);

  sys_sem_signal((sys_sem_t*)(data->completed_sem));
}
#endif  /* LWIP_TCPIP_CORE_LOCKING */

/** lwip_setsockopt_impl: the actual implementation of setsockopt:
 * same argument as lwip_setsockopt, either called directly or through callback
 */
static int
lwip_setsockopt_impl(int s, int level, int optname, const void *optval, socklen_t optlen)
{
  int err = 0;
  struct lwip_sock *sock = tryget_socket(s);
  if (!sock) {
    return EBADF;
  }

  switch (level) {

/* Level: SOL_SOCKET */
  case SOL_SOCKET:
    switch (optname) {

    /* SO_ACCEPTCONN is get-only */

    /* The option flags */
    case SO_BROADCAST:
    case SO_KEEPALIVE:
#if SO_REUSE
    case SO_REUSEADDR:
#endif /* SO_REUSE */
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
      if (*(const int*)optval) {
        ip_set_option(sock->conn->pcb.ip, optname);
      } else {
        ip_reset_option(sock->conn->pcb.ip, optname);
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, SOL_SOCKET, optname=0x%x, ..) -> %s\n",
                  s, optname, (*(const int*)optval?"on":"off")));
      break;

    /* SO_TYPE is get-only */
    /* SO_ERROR is get-only */

#if LWIP_SO_SNDTIMEO
    case SO_SNDTIMEO:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, LWIP_SO_SNDRCVTIMEO_OPTTYPE);
      netconn_set_sendtimeout(sock->conn, LWIP_SO_SNDRCVTIMEO_GET_MS(optval));
      break;
#endif /* LWIP_SO_SNDTIMEO */
#if LWIP_SO_RCVTIMEO
    case SO_RCVTIMEO:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, LWIP_SO_SNDRCVTIMEO_OPTTYPE);
      netconn_set_recvtimeout(sock->conn, (int)LWIP_SO_SNDRCVTIMEO_GET_MS(optval));
      break;
#endif /* LWIP_SO_RCVTIMEO */
#if LWIP_SO_RCVBUF
    case SO_RCVBUF:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, int);
      netconn_set_recvbufsize(sock->conn, *(const int*)optval);
      break;
#endif /* LWIP_SO_RCVBUF */
#if LWIP_SO_LINGER
    case SO_LINGER:
      {
        const struct linger* linger = (const struct linger*)optval;
        LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, struct linger);
        if (linger->l_onoff) {
          int lingersec = linger->l_linger;
          if (lingersec < 0) {
            done_socket(sock);
            return EINVAL;
          }
          if (lingersec > 0xFFFF) {
            lingersec = 0xFFFF;
          }
          sock->conn->linger = (s16_t)lingersec;
        } else {
          sock->conn->linger = -1;
        }
      }
      break;
#endif /* LWIP_SO_LINGER */
#if LWIP_UDP
    case SO_NO_CHECK:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_UDP);
#if LWIP_UDPLITE
      if ((udp_flags(sock->conn->pcb.udp) & UDP_FLAGS_UDPLITE) != 0) {
        /* this flag is only available for UDP, not for UDP lite */
        done_socket(sock);
        return EAFNOSUPPORT;
      }
#endif /* LWIP_UDPLITE */
      if (*(const int*)optval) {
        udp_setflags(sock->conn->pcb.udp, udp_flags(sock->conn->pcb.udp) | UDP_FLAGS_NOCHKSUM);
      } else {
        udp_setflags(sock->conn->pcb.udp, udp_flags(sock->conn->pcb.udp) & ~UDP_FLAGS_NOCHKSUM);
      }
      break;
#endif /* LWIP_UDP */
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, SOL_SOCKET, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;

/* Level: IPPROTO_IP */
  case IPPROTO_IP:
    switch (optname) {
    case IP_TTL:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
      sock->conn->pcb.ip->ttl = (u8_t)(*(const int*)optval);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_IP, IP_TTL, ..) -> %d\n",
                  s, sock->conn->pcb.ip->ttl));
      break;
    case IP_TOS:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
      sock->conn->pcb.ip->tos = (u8_t)(*(const int*)optval);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_IP, IP_TOS, ..)-> %d\n",
                  s, sock->conn->pcb.ip->tos));
      break;
#if LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS
    case IP_MULTICAST_TTL:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, u8_t, NETCONN_UDP);
      udp_set_multicast_ttl(sock->conn->pcb.udp, (u8_t)(*(const u8_t*)optval));
      break;
    case IP_MULTICAST_IF:
      {
        ip4_addr_t if_addr;
        LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, struct in_addr, NETCONN_UDP);
        inet_addr_to_ip4addr(&if_addr, (const struct in_addr*)optval);
        udp_set_multicast_netif_addr(sock->conn->pcb.udp, &if_addr);
      }
      break;
    case IP_MULTICAST_LOOP:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, u8_t, NETCONN_UDP);
      if (*(const u8_t*)optval) {
        udp_setflags(sock->conn->pcb.udp, udp_flags(sock->conn->pcb.udp) | UDP_FLAGS_MULTICAST_LOOP);
      } else {
        udp_setflags(sock->conn->pcb.udp, udp_flags(sock->conn->pcb.udp) & ~UDP_FLAGS_MULTICAST_LOOP);
      }
      break;
#endif /* LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS */
#if LWIP_IGMP
    case IP_ADD_MEMBERSHIP:
    case IP_DROP_MEMBERSHIP:
      {
        /* If this is a TCP or a RAW socket, ignore these options. */
        /* @todo: assign membership to this socket so that it is dropped when closing the socket */
        err_t igmp_err;
        const struct ip_mreq *imr = (const struct ip_mreq *)optval;
        ip4_addr_t if_addr;
        ip4_addr_t multi_addr;
        LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, struct ip_mreq, NETCONN_UDP);
        inet_addr_to_ip4addr(&if_addr, &imr->imr_interface);
        inet_addr_to_ip4addr(&multi_addr, &imr->imr_multiaddr);
        if (optname == IP_ADD_MEMBERSHIP) {
          if (!lwip_socket_register_membership(s, &if_addr, &multi_addr)) {
            /* cannot track membership (out of memory) */
            err = ENOMEM;
            igmp_err = ERR_OK;
          } else {
            igmp_err = igmp_joingroup(&if_addr, &multi_addr);
          }
        } else {
          igmp_err = igmp_leavegroup(&if_addr, &multi_addr);
          lwip_socket_unregister_membership(s, &if_addr, &multi_addr);
        }
        if (igmp_err != ERR_OK) {
          err = EADDRNOTAVAIL;
        }
      }
      break;
#endif /* LWIP_IGMP */
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_IP, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;

#if LWIP_TCP
/* Level: IPPROTO_TCP */
  case IPPROTO_TCP:
    /* Special case: all IPPROTO_TCP option take an int */
    LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_TCP);
    if (sock->conn->pcb.tcp->state == LISTEN) {
      done_socket(sock);
      return EINVAL;
    }
    switch (optname) {
    case TCP_NODELAY:
      if (*(const int*)optval) {
        tcp_nagle_disable(sock->conn->pcb.tcp);
      } else {
        tcp_nagle_enable(sock->conn->pcb.tcp);
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_TCP, TCP_NODELAY) -> %s\n",
                  s, (*(const int *)optval)?"on":"off") );
      break;
    case TCP_KEEPALIVE:
      sock->conn->pcb.tcp->keep_idle = (u32_t)(*(const int*)optval);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_TCP, TCP_KEEPALIVE) -> %"U32_F"\n",
                  s, sock->conn->pcb.tcp->keep_idle));
      break;

#if LWIP_TCP_KEEPALIVE
    case TCP_KEEPIDLE:
      sock->conn->pcb.tcp->keep_idle = 1000*(u32_t)(*(const int*)optval);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_TCP, TCP_KEEPIDLE) -> %"U32_F"\n",
                  s, sock->conn->pcb.tcp->keep_idle));
      break;
    case TCP_KEEPINTVL:
      sock->conn->pcb.tcp->keep_intvl = 1000*(u32_t)(*(const int*)optval);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_TCP, TCP_KEEPINTVL) -> %"U32_F"\n",
                  s, sock->conn->pcb.tcp->keep_intvl));
      break;
    case TCP_KEEPCNT:
      sock->conn->pcb.tcp->keep_cnt = (u32_t)(*(const int*)optval);
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_TCP, TCP_KEEPCNT) -> %"U32_F"\n",
                  s, sock->conn->pcb.tcp->keep_cnt));
      break;
#endif /* LWIP_TCP_KEEPALIVE */
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_TCP, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
#endif /* LWIP_TCP*/

#if LWIP_IPV6
/* Level: IPPROTO_IPV6 */
  case IPPROTO_IPV6:
    switch (optname) {
    case IPV6_V6ONLY:
      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_TCP);
      if (*(const int*)optval) {
        netconn_set_ipv6only(sock->conn, 1);
      } else {
        netconn_set_ipv6only(sock->conn, 0);
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_IPV6, IPV6_V6ONLY, ..) -> %d\n",
                  s, (netconn_get_ipv6only(sock->conn) ? 1 : 0)));
      break;
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_IPV6, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
#endif /* LWIP_IPV6 */

#if LWIP_UDP && LWIP_UDPLITE
  /* Level: IPPROTO_UDPLITE */
  case IPPROTO_UDPLITE:
    /* Special case: all IPPROTO_UDPLITE option take an int */
    LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
    /* If this is no UDP lite socket, ignore any options. */
    if (!NETCONNTYPE_ISUDPLITE(netconn_type(sock->conn))) {
      done_socket(sock);
      return ENOPROTOOPT;
    }
    switch (optname) {
    case UDPLITE_SEND_CSCOV:
      if ((*(const int*)optval != 0) && ((*(const int*)optval < 8) || (*(const int*)optval > 0xffff))) {
        /* don't allow illegal values! */
        sock->conn->pcb.udp->chksum_len_tx = 8;
      } else {
        sock->conn->pcb.udp->chksum_len_tx = (u16_t)*(const int*)optval;
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV) -> %d\n",
                  s, (*(const int*)optval)) );
      break;
    case UDPLITE_RECV_CSCOV:
      if ((*(const int*)optval != 0) && ((*(const int*)optval < 8) || (*(const int*)optval > 0xffff))) {
        /* don't allow illegal values! */
        sock->conn->pcb.udp->chksum_len_rx = 8;
      } else {
        sock->conn->pcb.udp->chksum_len_rx = (u16_t)*(const int*)optval;
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV) -> %d\n",
                  s, (*(const int*)optval)) );
      break;
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_UDPLITE, UNIMPL: optname=0x%x, ..)\n",
                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
#endif /* LWIP_UDP */
  /* Level: IPPROTO_RAW */
  case IPPROTO_RAW:
    switch (optname) {
#if LWIP_IPV6 && LWIP_RAW
    case IPV6_CHECKSUM:
      /* It should not be possible to disable the checksum generation with ICMPv6
       * as per RFC 3542 chapter 3.1 */
      if(sock->conn->pcb.raw->protocol == IPPROTO_ICMPV6) {
        done_socket(sock);
        return EINVAL;
      }

      LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_RAW);
      if (*(const int *)optval < 0) {
        sock->conn->pcb.raw->chksum_reqd = 0;
      } else if (*(const int *)optval & 1) {
        /* Per RFC3542, odd offsets are not allowed */
        done_socket(sock);
        return EINVAL;
      } else {
        sock->conn->pcb.raw->chksum_reqd = 1;
        sock->conn->pcb.raw->chksum_offset = (u16_t)*(const int *)optval;
      }
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_RAW, IPV6_CHECKSUM, ..) -> %d\n",
                  s, sock->conn->pcb.raw->chksum_reqd));
      break;
#endif /* LWIP_IPV6 && LWIP_RAW */
    default:
      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, IPPROTO_RAW, UNIMPL: optname=0x%x, ..)\n",
                                  s, optname));
      err = ENOPROTOOPT;
      break;
    }  /* switch (optname) */
    break;
  default:
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_setsockopt(%d, level=0x%x, UNIMPL: optname=0x%x, ..)\n",
                s, level, optname));
    err = ENOPROTOOPT;
    break;
  }  /* switch (level) */

  done_socket(sock);
  return err;
}

int
lwip_ioctl(int s, long cmd, void *argp)
{
  struct lwip_sock *sock = get_socket(s);
  u8_t val;
#if LWIP_SO_RCVBUF
  int recv_avail;
#endif /* LWIP_SO_RCVBUF */

  if (!sock) {
    return -1;
  }

  switch (cmd) {
#if LWIP_SO_RCVBUF || LWIP_FIONREAD_LINUXMODE
  case FIONREAD:
    if (!argp) {
      sock_set_errno(sock, EINVAL);
      done_socket(sock);
      return -1;
    }
#if LWIP_FIONREAD_LINUXMODE
    if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_TCP) {
      struct netbuf *nb;
      if (sock->lastdata.netbuf) {
        nb = sock->lastdata.netbuf;
        *((int*)argp) = nb->p->tot_len;
      } else {
        struct netbuf *rxbuf;
        err_t err = netconn_recv_udp_raw_netbuf_flags(sock->conn, &rxbuf, NETCONN_DONTBLOCK);
        if (err != ERR_OK) {
          *((int*)argp) = 0;
        } else {
          sock->lastdata.netbuf = rxbuf;
          *((int*)argp) = rxbuf->p->tot_len;
        }
      }
      done_socket(sock);
      return 0;
    }
#endif /* LWIP_FIONREAD_LINUXMODE */

#if LWIP_SO_RCVBUF
    /* we come here if either LWIP_FIONREAD_LINUXMODE==0 or this is a TCP socket */
    SYS_ARCH_GET(sock->conn->recv_avail, recv_avail);
    if (recv_avail < 0) {
      recv_avail = 0;
    }

    /* Check if there is data left from the last recv operation. /maq 041215 */
    if (sock->lastdata.netbuf) {
      if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_TCP) {
        recv_avail += sock->lastdata.pbuf->tot_len;
      } else {
        recv_avail += sock->lastdata.netbuf->p->tot_len;
      }
    }
    *((int*)argp) = recv_avail;

    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_ioctl(%d, FIONREAD, %p) = %"U16_F"\n", s, argp, *((u16_t*)argp)));
    sock_set_errno(sock, 0);
    done_socket(sock);
    return 0;
#else /* LWIP_SO_RCVBUF */
    break;
#endif /* LWIP_SO_RCVBUF */
#endif /* LWIP_SO_RCVBUF || LWIP_FIONREAD_LINUXMODE */

  case (long)FIONBIO:
    val = 0;
    if (argp && *(u32_t*)argp) {
      val = 1;
    }
    netconn_set_nonblocking(sock->conn, val);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_ioctl(%d, FIONBIO, %d)\n", s, val));
    sock_set_errno(sock, 0);
    done_socket(sock);
    return 0;

  default:
    break;
  } /* switch (cmd) */
  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_ioctl(%d, UNIMPL: 0x%lx, %p)\n", s, cmd, argp));
  sock_set_errno(sock, ENOSYS); /* not yet implemented */
  done_socket(sock);
  return -1;
}

/** A minimal implementation of fcntl.
 * Currently only the commands F_GETFL and F_SETFL are implemented.
 * Only the flag O_NONBLOCK is implemented.
 */
int
lwip_fcntl(int s, int cmd, int val)
{
  struct lwip_sock *sock = get_socket(s);
  int ret = -1;

  if (!sock) {
    return -1;
  }

  switch (cmd) {
  case F_GETFL:
    ret = netconn_is_nonblocking(sock->conn) ? O_NONBLOCK : 0;
    sock_set_errno(sock, 0);
    break;
  case F_SETFL:
    if ((val & ~O_NONBLOCK) == 0) {
      /* only O_NONBLOCK, all other bits are zero */
      netconn_set_nonblocking(sock->conn, val & O_NONBLOCK);
      ret = 0;
      sock_set_errno(sock, 0);
    } else {
      sock_set_errno(sock, ENOSYS); /* not yet implemented */
    }
    break;
  default:
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_fcntl(%d, UNIMPL: %d, %d)\n", s, cmd, val));
    sock_set_errno(sock, ENOSYS); /* not yet implemented */
    break;
  }
  done_socket(sock);
  return ret;
}

#if LWIP_IGMP
/** Register a new IGMP membership. On socket close, the membership is dropped automatically.
 *
 * ATTENTION: this function is called from tcpip_thread (or under CORE_LOCK).
 *
 * @return 1 on success, 0 on failure
 */
static int
lwip_socket_register_membership(int s, const ip4_addr_t *if_addr, const ip4_addr_t *multi_addr)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return 0;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if (socket_ipv4_multicast_memberships[i].sock == NULL) {
      socket_ipv4_multicast_memberships[i].sock = sock;
      ip4_addr_copy(socket_ipv4_multicast_memberships[i].if_addr, *if_addr);
      ip4_addr_copy(socket_ipv4_multicast_memberships[i].multi_addr, *multi_addr);
      done_socket(sock);
      return 1;
    }
  }
  done_socket(sock);
  return 0;
}

/** Unregister a previously registered membership. This prevents dropping the membership
 * on socket close.
 *
 * ATTENTION: this function is called from tcpip_thread (or under CORE_LOCK).
 */
static void
lwip_socket_unregister_membership(int s, const ip4_addr_t *if_addr, const ip4_addr_t *multi_addr)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if ((socket_ipv4_multicast_memberships[i].sock == sock) &&
        ip4_addr_cmp(&socket_ipv4_multicast_memberships[i].if_addr, if_addr) &&
        ip4_addr_cmp(&socket_ipv4_multicast_memberships[i].multi_addr, multi_addr)) {
      socket_ipv4_multicast_memberships[i].sock = NULL;
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].if_addr);
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].multi_addr);
      break;
    }
  }
  done_socket(sock);
}

/** Drop all memberships of a socket that were not dropped explicitly via setsockopt.
 *
 * ATTENTION: this function is NOT called from tcpip_thread (or under CORE_LOCK).
 */
static void
lwip_socket_drop_registered_memberships(int s)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if (socket_ipv4_multicast_memberships[i].sock == sock) {
      ip_addr_t multi_addr, if_addr;
      ip_addr_copy_from_ip4(multi_addr, socket_ipv4_multicast_memberships[i].multi_addr);
      ip_addr_copy_from_ip4(if_addr, socket_ipv4_multicast_memberships[i].if_addr);
      socket_ipv4_multicast_memberships[i].sock = NULL;
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].if_addr);
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].multi_addr);

      netconn_join_leave_group(sock->conn, &multi_addr, &if_addr, NETCONN_LEAVE);
    }
  }
  done_socket(sock);
}
#endif /* LWIP_IGMP */
#endif /* LWIP_SOCKET */
