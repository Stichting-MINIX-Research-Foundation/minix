#ifndef MINIX_NET_LWIP_ROUTE_H
#define MINIX_NET_LWIP_ROUTE_H

#include <net/route.h>

struct route_entry;
struct rtsock_request;

void route_init(void);
int route_add(const ip_addr_t * addr, unsigned int prefix,
	const ip_addr_t * gateway, struct ifdev * ifdev, unsigned int flags,
	const struct rtsock_request * rtr);
int route_can_add(const ip_addr_t * addr, unsigned int prefix, int is_host);
struct route_entry *route_find(const ip_addr_t * addr, unsigned int prefix,
	int is_host);
struct route_entry *route_lookup(const ip_addr_t * addr);
void route_delete(struct route_entry * route,
	const struct rtsock_request * rtr);
void route_clear(struct ifdev * ifdev);
int route_process(unsigned int type, const struct sockaddr * dst,
	const struct sockaddr * mask, const struct sockaddr * gateway,
	const struct sockaddr * ifp, const struct sockaddr * ifa,
	unsigned int flags, unsigned long inits,
	const struct rt_metrics * rmx, const struct rtsock_request * rtr);
void route_get(const struct route_entry * route, union sockaddr_any * addr,
	union sockaddr_any * mask, union sockaddr_any * gateway,
	union sockaddr_any * ifp, union sockaddr_any * ifa,
	struct ifdev ** ifdev, unsigned int * flags, unsigned int * use);
unsigned int route_get_flags(const struct route_entry * route);
struct ifdev *route_get_ifdev(const struct route_entry * route);
int route_is_ipv6(const struct route_entry * route);
struct route_entry *route_enum_v4(struct route_entry * last);
struct route_entry *route_enum_v6(struct route_entry * last);
int route_output_v4(struct ifdev * ifdev, const ip4_addr_t * ipaddr,
	err_t * err);
int route_output_v6(struct ifdev * ifdev, const ip6_addr_t * ipaddr,
	err_t * err);

#endif /* !MINIX_NET_LWIP_ROUTE_H */
