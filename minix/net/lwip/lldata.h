#ifndef MINIX_NET_LWIP_LLDATA_H
#define MINIX_NET_LWIP_LLDATA_H

struct rtsock_request;

typedef int lldata_arp_num_t;		/* ARP table entry number */
typedef int lldata_ndp_num_t;		/* NDP table entry number */

int lldata_arp_enum(lldata_arp_num_t * num);
void lldata_arp_get(lldata_arp_num_t num, struct sockaddr_in * addr,
	struct sockaddr_dlx * gateway, struct ifdev ** ifdevp,
	unsigned int * flagsp);

int lldata_ndp_find(struct ifdev * ifdev,
	const struct sockaddr_in6 * addr, lldata_ndp_num_t * nump);
int lldata_ndp_enum(lldata_ndp_num_t * num);
void lldata_ndp_get(lldata_ndp_num_t num, struct sockaddr_in6 * addr,
	struct sockaddr_dlx * gateway, struct ifdev ** ifdevp,
	unsigned int * flagsp);
void lldata_ndp_get_info(lldata_ndp_num_t num, long * asked, int * isrouter,
	int * state, int * expire);

int lldata_process(unsigned int type, const ip_addr_t * dst_addr,
	const struct sockaddr * gateway, struct ifdev * ifdev,
	unsigned int flags, const struct rtsock_request * rtr);

#endif /* !MINIX_NET_LWIP_LLDATA_H */
