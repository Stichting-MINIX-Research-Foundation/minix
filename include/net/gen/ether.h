/*
server/ip/gen/ether.h
*/

#ifndef __SERVER__IP__GEN__ETHER_H__
#define __SERVER__IP__GEN__ETHER_H__

#define ETH_MIN_PACK_SIZE		  60
#define ETH_MAX_PACK_SIZE		1514
#define ETH_MAX_PACK_SIZE_TAGGED	1518
#define ETH_HDR_SIZE			  14
#define ETH_CRC_SIZE			   4

typedef struct ether_addr
{
	u8_t ea_addr[6];
} ether_addr_t;

typedef u16_t ether_type_t;
typedef U16_t Ether_type_t;

#define ETH_ARP_PROTO	0x806
#define ETH_IP_PROTO	0x800

#endif /* __SERVER__IP__GEN__ETHER_H__ */
