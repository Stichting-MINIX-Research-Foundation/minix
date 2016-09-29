/*
 * MINIX 3 specific hooks for lwIP.
 */
#ifndef LWIP_LWIPHOOKS_H
#define LWIP_LWIPHOOKS_H

/* TCP ISN hook. */
u32_t lwip_hook_tcp_isn(const ip_addr_t * local_ip, u16_t local_port,
	const ip_addr_t * remote_ip, u16_t remote_port);

#define LWIP_HOOK_TCP_ISN lwip_hook_tcp_isn

/*
 * IPv4 route hook.  Since we override the IPv4 routing function altogether,
 * this hook should not be called and will panic if it is called, because that
 * is an indication that something is seriously wrong.  Note that we do not use
 * the IPv4 source route hook, because that one would be called (needlessly).
 */
struct netif *lwip_hook_ip4_route(const ip4_addr_t * dst);

#define LWIP_HOOK_IP4_ROUTE lwip_hook_ip4_route

/* IPv4 gateway hook. */
const ip4_addr_t *lwip_hook_etharp_get_gw(struct netif * netif,
	const ip4_addr_t * ipaddr);

#define LWIP_HOOK_ETHARP_GET_GW lwip_hook_etharp_get_gw

/*
 * IPv6 route hook.  Since we override the IPv6 routing function altogether,
 * this hook should not be called and will panic if it is called, because that
 * is an indication that something is seriously wrong.
 */
struct netif *lwip_hook_ip6_route(const ip6_addr_t * dst,
	const ip6_addr_t * src);

#define LWIP_HOOK_IP6_ROUTE lwip_hook_ip6_route

/* IPv6 gateway (next-hop) hook. */
const ip6_addr_t *lwip_hook_nd6_get_gw(struct netif * netif,
	const ip6_addr_t * ipaddr);

#define LWIP_HOOK_ND6_GET_GW lwip_hook_nd6_get_gw

#endif /* !LWIP_LWIPHOOKS_H */
