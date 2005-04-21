/*
arp.h

Copyright 1995 Philip Homburg
*/

#ifndef ARP_H
#define ARP_H

#define ARP_ETHERNET	1

#define ARP_REQUEST	1
#define ARP_REPLY	2

/* Prototypes */
typedef void (*arp_func_t) ARGS(( int fd, ipaddr_t ipaddr,
	ether_addr_t *ethaddr ));

void arp_prep ARGS(( void ));
void arp_init ARGS(( void ));
void arp_set_ipaddr ARGS(( int eth_port, ipaddr_t ipaddr ));
int arp_set_cb ARGS(( int eth_port, int ip_port, arp_func_t arp_func ));
int arp_ip_eth ARGS(( int eth_port, ipaddr_t ipaddr, ether_addr_t *ethaddr ));

#endif /* ARP_H */

/*
 * $PchId: arp.h,v 1.5 1995/11/21 06:45:27 philip Exp $
 */
