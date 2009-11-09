/*
server/ip/gen/ip_hdr.h
*/

#ifndef __SERVER__IP__GEN__HDR_H__
#define __SERVER__IP__GEN__HDR_H__

typedef struct ip_hdr
{
	u8_t ih_vers_ihl,
		ih_tos;
	u16_t ih_length,
		ih_id,
		ih_flags_fragoff;
	u8_t ih_ttl,
		ih_proto;
	u16_t ih_hdr_chk;
	ipaddr_t ih_src,
		ih_dst;
} ip_hdr_t;

#define IH_IHL_MASK	0xf
#define IH_VERSION_MASK	0xf
#define IH_FRAGOFF_MASK	0x1fff
#define IH_MORE_FRAGS	0x2000
#define IH_DONT_FRAG	0x4000
#define IH_FLAGS_UNUSED	0x8000

#define IP_OPT_COPIED	0x80
#define IP_OPT_NUMBER	0x1f

#define IP_OPT_EOL	0x00	/* End of Options List, RFC-791 */
#define IP_OPT_NOP	0x01	/* No Operation, RFC-791 */
#define IP_OPT_RR	0x07	/* Record Route, RFC-791 */
#define IP_OPT_TS	0x44	/* Timestamp, RFC-791 */
#define IP_OPT_SEC	0x82	/* Security, RFC-1108 */
#define IP_OPT_LSRR	0x83	/* Loose Source Route, RFC-791 */
#define IP_OPT_SSRR	0x89	/* Strict Source Route, RFC-791 */
#define IP_OPT_RTRALT	0x94	/* Router Alert, RFC-2113 */

#define IP_OPT_RR_MIN		4

#endif /* __SERVER__IP__GEN__HDR_H__ */

/*
 * $PchId: ip_hdr.h,v 1.5 2002/06/10 07:11:46 philip Exp $
 */
