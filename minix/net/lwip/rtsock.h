#ifndef MINIX_NET_LWIP_RTSOCK_H
#define MINIX_NET_LWIP_RTSOCK_H

#include "ifaddr.h"
#include "lldata.h"

struct route_entry;
struct rtsock_request;

void rtsock_init(void);
sockid_t rtsock_socket(int type, int protocol, struct sock ** sock,
	const struct sockevent_ops ** ops);

void rtsock_msg_ifannounce(struct ifdev * ifdev, int arrival);
void rtsock_msg_ifinfo(struct ifdev * ifdev);

void rtsock_msg_addr_dl(struct ifdev * ifdev, unsigned int type,
	ifaddr_dl_num_t num);
void rtsock_msg_addr_v4(struct ifdev * ifdev, unsigned int type,
	ifaddr_v4_num_t num);
void rtsock_msg_addr_v6(struct ifdev * ifdev, unsigned int type,
	ifaddr_v6_num_t num);

void rtsock_msg_miss(const struct sockaddr * addr);
void rtsock_msg_route(const struct route_entry * route, unsigned int type,
	const struct rtsock_request * rtr);
void rtsock_msg_arp(lldata_arp_num_t num, unsigned int type,
	const struct rtsock_request * rtr);
void rtsock_msg_ndp(lldata_ndp_num_t num, unsigned int type,
	const struct rtsock_request * rtr);

#endif /* !MINIX_NET_LWIP_RTSOCK_H */
