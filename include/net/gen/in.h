/*
server/ip/gen/in.h
*/

#ifndef __SERVER__IP__GEN__IN_H__
#define __SERVER__IP__GEN__IN_H__

#define IP_MIN_HDR_SIZE		20
#define IP_MAX_HDR_SIZE		60		/* 15 * 4 */
#define IP_VERSION		4
#define IP_MAX_TTL		255
#define IP_DEF_MSS		576
#define IP_MAX_PACKSIZE		40000		/* 8192 */
	/* Note: this restriction is not part of the IP-protocol but
	   introduced by this implementation. */

#define IPPROTO_ICMP		1
#define IPPROTO_TCP		6
#define IPPROTO_UDP		17

typedef u32_t ipaddr_t;
typedef u8_t ipproto_t;
typedef struct ip_hdropt
{
	u8_t iho_opt_siz;
	u8_t iho_data[IP_MAX_HDR_SIZE-IP_MIN_HDR_SIZE];
} ip_hdropt_t;

#endif /* __SERVER__IP__GEN__IN_H__ */

/*
 * $PchId: in.h,v 1.3 1995/11/17 22:27:50 philip Exp $
 */
