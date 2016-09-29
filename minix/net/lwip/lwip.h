#ifndef MINIX_NET_LWIP_LWIP_H
#define MINIX_NET_LWIP_LWIP_H

#include <minix/drivers.h>
#include <minix/sockevent.h>
#include <minix/rmib.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/bpf.h>

#include "lwip/ip.h"
#include "lwiphooks.h"

#include "addr.h"
#include "ipsock.h"
#include "ifdev.h"
#include "util.h"

/*
 * The standard sockaddr_dl is an absolute pain, because the actual structure
 * is dynamically sized, while the standard definition is neither the minimum
 * nor the maximum size.  We use our own version, which uses the maximum size
 * that we will ever produce and accept.  This greatly simplifies dealing with
 * this structure while also limiting stack usage a bit.
 */
struct sockaddr_dlx {
	uint8_t		sdlx_len;	/* actual length of this structure */
	sa_family_t	sdlx_family;	/* address family, always AF_LINK */
	uint16_t	sdlx_index;	/* interface index */
	uint8_t		sdlx_type;	/* interface type (IFT_) */
	uint8_t		sdlx_nlen;	/* interface name length, w/o nul */
	uint8_t		sdlx_alen;	/* link-layer address length */
	uint8_t		sdlx_slen;	/* selector length, always 0 */
	uint8_t		sdlx_data[IFNAMSIZ + NETIF_MAX_HWADDR_LEN];
};

STATIC_SOCKADDR_MAX_ASSERT(sockaddr_in);
STATIC_SOCKADDR_MAX_ASSERT(sockaddr_in6);
STATIC_SOCKADDR_MAX_ASSERT(sockaddr_dlx);

/* This is our own, much smaller internal version of sockaddr_storage. */
union sockaddr_any {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_dlx sdlx;
};

/* Number of bits in each of the types of IP addresses. */
#define IP4_BITS	32		/* number of bits in an IPv4 address */
#define IP6_BITS	128		/* number of bits in an IPv6 address */

/*
 * Each socket module maintains its own set of sockets, but all sockets must be
 * given globally unique identifiers.  Therefore, we use these modifier masks,
 * which are bitwise OR'ed with the per-module socket identifiers.
 */
#define SOCKID_TCP	0x00000000
#define SOCKID_UDP	0x00100000
#define SOCKID_RAW	0x00200000
#define SOCKID_RT	0x00400000
#define SOCKID_LNK	0x00800000

/*
 * Static remote MIB node identifiers for nodes that are dynamically numbered
 * on NetBSD, because they do not have a corresponding protocol family number.
 */
#define NET_INTERFACES	(PF_MAX)	/* net.interfaces (TODO) */
#define NET_BPF		(PF_MAX + 1)	/* net.bpf */

#define ROOT_EUID	0		/* effective user ID of superuser */

/*
 * Function declarations.  Modules with more extended interfaces have their own
 * header files.
 */

/* mempool.c */
void mempool_init(void);
unsigned int mempool_cur_buffers(void);
unsigned int mempool_max_buffers(void);

/* pchain.c */
struct pbuf **pchain_end(struct pbuf * pbuf);
size_t pchain_size(struct pbuf * pbuf);

/* addrpol.c */
int addrpol_get_label(const ip_addr_t * ipaddr);
int addrpol_get_scope(const ip_addr_t * ipaddr, int is_src);

/* tcpsock.c */
void tcpsock_init(void);
sockid_t tcpsock_socket(int domain, int protocol, struct sock ** sock,
	const struct sockevent_ops ** ops);

/* udpsock.c */
void udpsock_init(void);
sockid_t udpsock_socket(int domain, int protocol, struct sock ** sock,
	const struct sockevent_ops ** ops);

/* rawsock.c */
void rawsock_init(void);
sockid_t rawsock_socket(int domain, int protocol, struct sock ** sock,
	const struct sockevent_ops ** ops);

/* loopif.c */
void loopif_init(void);
ssize_t loopif_cksum(struct rmib_call * call, struct rmib_node * node,
	struct rmib_oldp * oldp, struct rmib_newp * newp);

/* lnksock.c */
void lnksock_init(void);
sockid_t lnksock_socket(int type, int protocol, struct sock ** sock,
	const struct sockevent_ops ** ops);

/* mibtree.c */
void mibtree_init(void);
void mibtree_register_inet(int domain, int protocol, struct rmib_node * node);
void mibtree_register_lwip(struct rmib_node * node);

/* ifconf.c */
void ifconf_init(void);
int ifconf_ioctl(struct sock * sock, unsigned long request,
	const struct sockdriver_data * data, endpoint_t user_endpt);

/* bpf_filter.c */
u_int bpf_filter_ext(const struct bpf_insn * pc, const struct pbuf * pbuf,
	const u_char * packet, u_int total, u_int len);

#endif /* !MINIX_NET_LWIP_LWIP_H */
