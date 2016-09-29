#ifndef MINIX_NET_LWIP_IFADDR_H
#define MINIX_NET_LWIP_IFADDR_H

/* Possible values of ifdev_v6flags[] elements. */
#define IFADDR_V6F_AUTOCONF	0x01	/* autoconfigured address, no subnet */
#define IFADDR_V6F_TEMPORARY	0x02	/* temporary (privacy) address */
#define IFADDR_V6F_HWBASED	0x04	/* auto-derived from MAC address */

typedef int ifaddr_v4_num_t;		/* interface IPv4 address number */
typedef int ifaddr_v6_num_t;		/* interface IPv6 address number */
typedef int ifaddr_dl_num_t;		/* interface link address number */

extern int ifaddr_auto_linklocal;
extern int ifaddr_accept_rtadv;

void ifaddr_init(struct ifdev * ifdev);

int ifaddr_v4_find(struct ifdev * ifdev, const struct sockaddr_in * addr,
	ifaddr_v4_num_t * num);
int ifaddr_v4_enum(struct ifdev * ifdev, ifaddr_v4_num_t * num);
int ifaddr_v4_get(struct ifdev * ifdev, ifaddr_v4_num_t num,
	struct sockaddr_in * addr, struct sockaddr_in * mask,
	struct sockaddr_in * bcast, struct sockaddr_in * dest);
int ifaddr_v4_get_flags(struct ifdev * ifdev, ifaddr_v4_num_t num);
int ifaddr_v4_add(struct ifdev * ifdev, const struct sockaddr_in * addr,
	const struct sockaddr_in * mask, const struct sockaddr_in * bcast,
	const struct sockaddr_in * dest, int flags);
void ifaddr_v4_del(struct ifdev * ifdev, ifaddr_v4_num_t num);
void ifaddr_v4_clear(struct ifdev * ifdev);
struct ifdev *ifaddr_v4_map_by_addr(const ip4_addr_t * ip4addr);

int ifaddr_v6_find(struct ifdev * ifdev, const struct sockaddr_in6 * addr6,
	ifaddr_v6_num_t * num);
int ifaddr_v6_enum(struct ifdev * ifdev, ifaddr_v6_num_t * num);
void ifaddr_v6_get(struct ifdev * ifdev, ifaddr_v6_num_t num,
	struct sockaddr_in6 * addr6, struct sockaddr_in6 * mask6,
	struct sockaddr_in6 * dest6);
int ifaddr_v6_get_flags(struct ifdev * ifdev, ifaddr_v6_num_t num);
void ifaddr_v6_get_lifetime(struct ifdev * ifdev, ifaddr_v6_num_t num,
	struct in6_addrlifetime * lifetime);
int ifaddr_v6_add(struct ifdev * ifdev, const struct sockaddr_in6 * addr6,
	const struct sockaddr_in6 * mask6, const struct sockaddr_in6 * dest6,
	int flags, const struct in6_addrlifetime * lifetime);
void ifaddr_v6_del(struct ifdev * ifdev, ifaddr_v6_num_t num);
void ifaddr_v6_clear(struct ifdev * ifdev);
void ifaddr_v6_check(struct ifdev * ifdev);
void ifaddr_v6_set_up(struct ifdev * ifdev);
void ifaddr_v6_set_linklocal(struct ifdev * ifdev);
struct ifdev *ifaddr_v6_map_by_addr(const ip6_addr_t * ip6addr);

struct ifdev *ifaddr_map_by_addr(const ip_addr_t * ipaddr);
struct ifdev *ifaddr_map_by_subnet(const ip_addr_t * ipaddr);
const ip_addr_t *ifaddr_select(const ip_addr_t * dst_addr,
	struct ifdev * ifdev, struct ifdev ** ifdevp);
int ifaddr_is_zone_mismatch(const ip6_addr_t * ipaddr, struct ifdev * ifdev);

int ifaddr_dl_find(struct ifdev * ifdev, const struct sockaddr_dlx * addr,
	socklen_t addr_len, ifaddr_dl_num_t * num);
int ifaddr_dl_enum(struct ifdev * ifdev, ifaddr_dl_num_t * num);
void ifaddr_dl_get(struct ifdev * ifdev, ifaddr_dl_num_t num,
	struct sockaddr_dlx * addr);
int ifaddr_dl_get_flags(struct ifdev * ifdev, ifaddr_dl_num_t num);
int ifaddr_dl_add(struct ifdev * ifdev, const struct sockaddr_dlx * addr,
	socklen_t addr_len, int flags);
int ifaddr_dl_del(struct ifdev * ifdev, ifaddr_dl_num_t num);
void ifaddr_dl_clear(struct ifdev * ifdev);
void ifaddr_dl_update(struct ifdev * ifdev, const uint8_t * hwaddr,
	int is_factory);

#endif /* !MINIX_NET_LWIP_IFADDR_H */
