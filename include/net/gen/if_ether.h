/*
server/ip/gen/if_ether.h
*/

#ifndef __SERVER__IP__GEN__IF_ETHER_H__
#define __SERVER__IP__GEN__IF_ETHER_H__

struct ether_addr;

#define _PATH_ETHERS	"/etc/ethers"

char *ether_ntoa( struct ether_addr *e );
struct ether_addr *ether_aton( const char *s );
int ether_ntohost( char *hostname, struct ether_addr *e );
int ether_hostton( char *hostname, struct ether_addr *e );
int ether_line( char *l, struct ether_addr *e, char *hostname );

#endif /* __SERVER__IP__GEN__IF_ETHER_H__ */
