/*
server/ip/gen/tcp.h
*/

#ifndef __SERVER__IP__GEN__TCP_H__
#define __SERVER__IP__GEN__TCP_H__

#define TCP_MIN_HDR_SIZE	20
#define TCP_MAX_HDR_SIZE	60

#define TCPPORT_TELNET		23
#define TCPPORT_FINGER		79

#define TCPPORT_RESERVED	1024

typedef u16_t tcpport_t;
typedef U16_t Tcpport_t;	/* for use in prototypes */

#endif /* __SERVER__IP__GEN__TCP_H__ */
