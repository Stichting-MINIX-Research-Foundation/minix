/*
server/ip/gen/eth_hdr.h
*/

#ifndef __SERVER__IP__GEN__ETH_HDR_H__
#define __SERVER__IP__GEN__ETH_HDR_H__

#include <net/if_ether.h>

typedef struct eth_hdr
{
	ether_addr_t eh_dst;
	ether_addr_t eh_src;
	ether_type_t eh_proto;
} eth_hdr_t;

#endif /* __SERVER__IP__GEN__ETH_HDR_H__ */
