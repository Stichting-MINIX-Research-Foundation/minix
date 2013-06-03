/*
server/ip/gen/inet.h
*/

#ifndef __SERVER__IP__GEN__INET_H__
#define __SERVER__IP__GEN__INET_H__

#include <net/gen/in.h>

ipaddr_t inet_addr( const char *addr );
ipaddr_t inet_network( const char *addr );

#endif /* __SERVER__IP__GEN__INET_H__ */
