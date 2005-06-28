/*
net/gen/arp_io.h

Created:	Jan 2001 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

typedef struct nwio_arp
{
	int nwa_entno;
	u32_t nwa_flags;
	ipaddr_t nwa_ipaddr;
	ether_addr_t nwa_ethaddr;
} nwio_arp_t;

#define NWAF_EMPTY	0
#define NWAF_INCOMPLETE	1
#define NWAF_DEAD	2
#define NWAF_PERM	4
#define NWAF_PUB	8

/*
 * $PchId: arp_io.h,v 1.2 2004/08/03 11:01:59 philip Exp $
 */
