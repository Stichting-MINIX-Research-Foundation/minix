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
#define IPPROTO_TCP	6	/* TCP */
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

#endif /* _NETINET__IN_H */
