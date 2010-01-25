/*
netinet/in.h
*/

#ifndef _NETINET__IN_H
#define _NETINET__IN_H

/* Can we include <stdint.h> here or do we need an additional header that is
 * safe to include?
 */
#include <stdint.h>

/* Open Group Base Specifications Issue 6 (not complete) */
#define    INADDR_ANY              (uint32_t)0x00000000
#define    INADDR_BROADCAST        (uint32_t)0xFFFFFFFF
#define    INADDR_LOOPBACK         (uint32_t)0x7F000001

#define    IN_LOOPBACKNET          127

#define       IPPORT_RESERVED         1024

typedef uint16_t	in_port_t;

#ifndef _IN_ADDR_T
#define _IN_ADDR_T
typedef uint32_t	in_addr_t;
#endif /* _IN_ADDR_T */

#ifndef _SA_FAMILY_T
#define _SA_FAMILY_T
/* Should match corresponding typedef in <sys/socket.h> */
typedef uint8_t		sa_family_t;
#endif /* _SA_FAMILY_T */

/* Protocols */
#define IPPROTO_IP	0	/* Dummy protocol */
#define IPPROTO_ICMP	1	/* ICMP */
#define IPPROTO_TCP	6	/* TCP */
#define IPPROTO_EGP	8	/* exterior gateway protocol */
#define IPPROTO_UDP	17	/* UDP */

/* setsockopt options at IP level */
#define IP_ADD_MEMBERSHIP	12
#define IP_DROP_MEMBERSHIP	13

#ifndef _STRUCT_IN_ADDR
#define _STRUCT_IN_ADDR
struct in_addr
{
	in_addr_t	s_addr;
};
#endif

struct sockaddr_in
{
	sa_family_t	sin_family;
	in_port_t	sin_port;
	struct in_addr	sin_addr;
};

struct ip_mreq
{
	struct  in_addr imr_multiaddr;
	struct  in_addr imr_interface;
};

/* 
 * IPv6 is not supported, but some programs need these declarations 
 * nevertheless; these declarations are based on
 * http://www.opengroup.org/onlinepubs/000095399/basedefs/netinet/in.h.html
 */
struct in6_addr
{
	uint8_t	s6_addr[16];
};

struct sockaddr_in6
{
	sa_family_t	sin6_family;
	in_port_t	sin6_port;
	uint32_t	sin6_flowinfo;
	struct in6_addr	sin6_addr;
	uint32_t	sin6_scope_id;
};

#define	INET6_ADDRSTRLEN	46

/* Definitions that are not part of the Open Group Base Specifications */
#define IN_CLASSA(i)	(((uint32_t)(i) & 0x80000000) == 0)
#define IN_CLASSA_NET	0xff000000
#define IN_CLASSA_NSHIFT 24

#define IN_CLASSB(i)	(((uint32_t)(i) & 0xc0000000) == 0x80000000)
#define IN_CLASSB_NET	0xffff0000
#define IN_CLASSB_NSHIFT 16

#define IN_CLASSC(i)	(((uint32_t)(i) & 0xe0000000) == 0xc0000000)
#define IN_CLASSC_NET	0xffffff00
#define IN_CLASSC_NSHIFT 8

#define IN_CLASSD(i)	(((uint32_t)(i) & 0xf0000000) == 0xe0000000)
#define IN_CLASSD_NET	0xf0000000
#define IN_CLASSD_NSHIFT 28

#endif /* _NETINET__IN_H */
