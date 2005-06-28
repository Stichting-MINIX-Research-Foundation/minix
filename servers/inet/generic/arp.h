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

int arp_ioctl ARGS(( int eth_port, int fd, ioreq_t req,
	get_userdata_t get_userdata, put_userdata_t put_userdata ));

#endif /* ARP_H */

/*
 * $PchId: arp.h,v 1.7 2001/04/19 18:58:17 philip Exp $
 */
