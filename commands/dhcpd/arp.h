/*	arp.h - Address Resolution Protocol packet format.
 *							Author: Kees J. Bot
 *								16 Dec 2000
 */
#ifndef ARP_H
#define ARP_H

typedef struct arp46 {
	ether_addr_t	dstaddr;
	ether_addr_t	srcaddr;
	ether_type_t	ethtype;	/* ARP_PROTO. */
	u16_t		hdr, pro;	/* ARP_ETHERNET & ETH_IP_PROTO. */
	u8_t		hln, pln;	/* 6 & 4. */
	u16_t		op;		/* ARP_REQUEST or ARP_REPLY. */
	ether_addr_t	sha;		/* Source hardware address. */
	u8_t		spa[4];		/* Source protocol address. */
	ether_addr_t	tha;		/* Likewise for the target. */
	u8_t		tpa[4];
	char		padding[60 - (4*6 + 2*4 + 4*2 + 2*1)];
} arp46_t;

#define ARP_ETHERNET	1	/* ARP on Ethernet. */
#define ARP_REQUEST	1	/* A request for an IP address. */
#define ARP_REPLY	2	/* A reply to a request. */

#endif /* ARP_H */
