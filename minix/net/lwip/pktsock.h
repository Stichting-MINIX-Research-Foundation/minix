#ifndef MINIX_NET_LWIP_PKTSOCK_H
#define MINIX_NET_LWIP_PKTSOCK_H

#include "mcast.h"

/* Packet-level socket, shared by UDP and RAW. */
struct pktsock {
	struct ipsock pkt_ipsock;	/* IP socket object, MUST be first */
	struct pbuf *pkt_rcvhead;	/* receive buffer, first packet */
	struct pbuf **pkt_rcvtailp;	/* receive buffer, last ptr-ptr */
	size_t pkt_rcvlen;		/* receive buffer, length in bytes */
	struct mcast_head pkt_mcast;	/* multicast membership list */
	ip6_addr_p_t pkt_srcaddr;	/* IPV6_PKTINFO: source address */
	uint32_t pkt_ifindex;		/* IPV6_KPTINFO: interface index */
};

#define pktsock_get_ipsock(pkt)		(&(pkt)->pkt_ipsock)
#define pktsock_get_ifindex(pkt)	((pkt)->pkt_ifindex)

/* Options when sending packets. */
struct pktopt {
	uint8_t pkto_flags;		/* packet send flags (PKTOF_) */
	uint8_t pkto_tos;		/* type of service for the packet */
	uint8_t pkto_ttl;		/* time-to-live for the packet */
	uint8_t pkto_mcast_ttl;		/* time-to-live for multicast packet */
	ip6_addr_p_t pkto_srcaddr;	/* IPV6_PKTINFO: source address */
	unsigned int pkto_ifindex;	/* IPV6_PKTINFO: interface index */
};

#define PKTOF_TTL		0x01	/* send packet with custom TTL value */
#define PKTOF_TOS		0x02	/* send packet with custom TOS value */
#define PKTOF_PKTINFO		0x04	/* send packet with src addr, on if. */

int pktsock_socket(struct pktsock * pkt, int domain, size_t sndbuf,
	size_t rcvbuf, struct sock ** sockp);
int pktsock_test_input(struct pktsock * pkt, struct pbuf * pbuf);
void pktsock_input(struct pktsock * pkt, struct pbuf * pbuf,
	const ip_addr_t * srcaddr, uint16_t port);
int pktsock_get_pktinfo(struct pktsock * pkt, struct pktopt * pkto,
	struct ifdev ** ifdevp, ip_addr_t * src_addrp);
int pktsock_get_ctl(struct pktsock * pkt, const struct sockdriver_data * ctl,
	socklen_t ctl_len, struct pktopt * pkto);
int pktsock_get_data(struct pktsock * pkt, const struct sockdriver_data * data,
	size_t len, struct pbuf * pbuf);
int pktsock_pre_recv(struct sock * sock, endpoint_t user_endpt, int flags);
int pktsock_recv(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * off, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t * ctl_off, struct sockaddr * addr,
	socklen_t * addr_len, endpoint_t user_endpt, int flags, size_t min,
	int * rflags);
int pktsock_test_recv(struct sock * sock, size_t min, size_t * size);
void pktsock_set_mcaware(struct pktsock * pkt);
int pktsock_setsockopt(struct pktsock * pkt, int level, int name,
	const struct sockdriver_data * data, socklen_t len,
	struct ipopts * ipopts);
int pktsock_getsockopt(struct pktsock * pkt, int level, int name,
	const struct sockdriver_data * data, socklen_t * len,
	struct ipopts * ipopts);
void pktsock_shutdown(struct pktsock * pkt, unsigned int mask);
void pktsock_close(struct pktsock * pkt);
size_t pktsock_get_recvlen(struct pktsock * pkt);

#endif /* !MINIX_NET_LWIP_PKTSOCK_H */
