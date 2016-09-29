#ifndef MINIX_NET_LWIP_ADDR_H
#define MINIX_NET_LWIP_ADDR_H

int addr_is_unspec(const struct sockaddr * addr, socklen_t addr_len);

int addr_is_valid_multicast(const ip_addr_t * ipaddr);

int addr_get_inet(const struct sockaddr * addr, socklen_t addr_len,
	uint8_t type, ip_addr_t * ipaddr, int kame, uint16_t * port);
void addr_put_inet(struct sockaddr * addr, socklen_t * addr_len,
	const ip_addr_t * ipaddr, int kame, uint16_t port);

int addr_get_link(const struct sockaddr * addr, socklen_t addr_len,
	char * name, size_t name_max, uint8_t * hwaddr, size_t hwaddr_len);
void addr_put_link(struct sockaddr * addr, socklen_t * addr_len,
	uint32_t ifindex, uint32_t type, const char * name,
	const uint8_t * hwaddr, size_t hwaddr_len);

int addr_get_netmask(const struct sockaddr * addr, socklen_t addr_len,
	uint8_t type, unsigned int * prefix, ip_addr_t * ipaddr);
void addr_make_netmask(uint8_t * addr, socklen_t addr_len,
	unsigned int prefix);
void addr_put_netmask(struct sockaddr * addr, socklen_t * addr_len,
	uint8_t type, unsigned int prefix);

void addr_normalize(ip_addr_t * dst, const ip_addr_t * src,
	unsigned int prefix);
unsigned int addr_get_common_bits(const ip_addr_t * addr1,
	const ip_addr_t * addr2, unsigned int max);

void addr_make_v4mapped_v6(ip_addr_t * dst, const ip4_addr_t * src);

#endif /* !MINIX_NET_LWIP_ADDR_H */
