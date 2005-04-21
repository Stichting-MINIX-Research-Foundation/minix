/*
arp.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "type.h"

#include "arp.h"
#include "assert.h"
#include "buf.h"
#include "clock.h"
#include "eth.h"
#include "io.h"
#include "sr.h"

THIS_FILE

#define ARP_CACHE_NR	64

#define MAX_ARP_RETRIES		5
#define ARP_TIMEOUT		(HZ/2+1)	/* .5 seconds */
#ifndef ARP_EXP_TIME
#define ARP_EXP_TIME		(20L*60L*HZ)	/* 20 minutes */
#endif
#define ARP_NOTRCH_EXP_TIME	(5*HZ)		/* 5 seconds */
#define ARP_INUSE_OFFSET	(60*HZ)	/* an entry in the cache can be deleted
					   if its not used for 1 minute */

typedef struct arp46
{
	ether_addr_t a46_dstaddr;
	ether_addr_t a46_srcaddr;
	ether_type_t a46_ethtype;
	union
	{
		struct
		{
			u16_t a_hdr, a_pro;
			u8_t a_hln, a_pln;
			u16_t a_op;
			ether_addr_t a_sha;
			u8_t a_spa[4];
			ether_addr_t a_tha;
			u8_t a_tpa[4];
		} a46_data;
		char    a46_dummy[ETH_MIN_PACK_SIZE-ETH_HDR_SIZE];
	} a46_data;
} arp46_t;

#define a46_hdr a46_data.a46_data.a_hdr
#define a46_pro a46_data.a46_data.a_pro
#define a46_hln a46_data.a46_data.a_hln
#define a46_pln a46_data.a46_data.a_pln
#define a46_op a46_data.a46_data.a_op
#define a46_sha a46_data.a46_data.a_sha
#define a46_spa a46_data.a46_data.a_spa
#define a46_tha a46_data.a46_data.a_tha
#define a46_tpa a46_data.a46_data.a_tpa

typedef struct arp_port
{
	int ap_flags;
	int ap_state;
	int ap_eth_port;
	int ap_ip_port;
	int ap_eth_fd;
	ether_addr_t ap_ethaddr;
	ipaddr_t ap_ipaddr;
	timer_t ap_timer;

	ether_addr_t ap_write_ethaddr;
	ipaddr_t ap_write_ipaddr;
	int ap_write_code;

	ipaddr_t ap_req_ipaddr;
	int ap_req_count;

	arp_func_t ap_arp_func;
} arp_port_t;

#define APF_EMPTY	0
#define APF_ARP_RD_IP	0x4
#define APF_ARP_RD_SP	0x8
#define APF_ARP_WR_IP	0x10
#define APF_ARP_WR_SP	0x20
#define APF_INADDR_SET	0x100
#define APF_MORE2WRITE	0x200
#define APF_CLIENTREQ	0x400
#define APF_CLIENTWRITE	0x1000
#define APF_SUSPEND	0x2000

#define APS_INITIAL	0x00
#define	APS_GETADDR	0x01
#define	APS_ARPSTART	0x10
#define	APS_ARPPROTO	0x20
#define	APS_ARPMAIN	0x40
#define	APS_ERROR	0x80

typedef struct arp_cache
{
	int ac_flags;
	int ac_state;
	ether_addr_t ac_ethaddr;
	ipaddr_t ac_ipaddr;
	arp_port_t *ac_port;
	time_t ac_expire;
	time_t ac_lastuse;
} arp_cache_t;

#define ACF_EMPTY	0
#define ACF_GOTREQ	1

#define ACS_UNUSED	0
#define ACS_INCOMPLETE	1
#define ACS_VALID	2
#define ACS_UNREACHABLE	3

FORWARD acc_t *arp_getdata ARGS(( int fd, size_t offset,
	size_t count, int for_ioctl ));
FORWARD int arp_putdata ARGS(( int fd, size_t offset,
	acc_t *data, int for_ioctl ));
FORWARD void arp_main ARGS(( arp_port_t *arp_port ));
FORWARD void arp_timeout ARGS(( int fd, timer_t *timer ));
FORWARD void setup_write ARGS(( arp_port_t *arp_port ));
FORWARD void setup_read ARGS(( arp_port_t *arp_port ));
FORWARD void process_arp_req ARGS(( arp_port_t *arp_port, acc_t *data ));
FORWARD void client_reply ARGS(( arp_port_t *arp_port,
	ipaddr_t ipaddr, ether_addr_t *ethaddr ));
FORWARD arp_cache_t *find_cache_ent ARGS(( arp_port_t *arp_port,
	ipaddr_t ipaddr ));
FORWARD arp_cache_t *alloc_cache_ent ARGS(( void ));

PRIVATE arp_port_t *arp_port_table;
PRIVATE	arp_cache_t arp_cache[ARP_CACHE_NR];

PUBLIC void arp_prep()
{
	arp_port_table= alloc(eth_conf_nr * sizeof(arp_port_table[0]));
}

PUBLIC void arp_init()
{
	arp_port_t *arp_port;
	int i;

	assert (BUF_S >= sizeof(struct nwio_ethstat));
	assert (BUF_S >= sizeof(struct nwio_ethopt));
	assert (BUF_S >= sizeof(arp46_t));

	for (i=0, arp_port= arp_port_table; i<eth_conf_nr; i++, arp_port++)
	{
		arp_port->ap_state= APS_ERROR;	/* Mark all ports as
						 * unavailable */
	}

}

PRIVATE void arp_main(arp_port)
arp_port_t *arp_port;
{
	int result;

	switch (arp_port->ap_state)
	{
	case APS_INITIAL:
		arp_port->ap_eth_fd= eth_open(arp_port->ap_eth_port,
			arp_port->ap_eth_port, arp_getdata, arp_putdata, 0);

		if (arp_port->ap_eth_fd<0)
		{
			DBLOCK(1, printf("arp.c: unable to open ethernet\n"));
			return;
		}

		arp_port->ap_state= APS_GETADDR;

		result= eth_ioctl (arp_port->ap_eth_fd, NWIOGETHSTAT);

		if ( result == NW_SUSPEND)
		{
			arp_port->ap_flags |= APF_SUSPEND;
			return;
		}
		assert(result == NW_OK);

		/* fall through */
	case APS_GETADDR:
		/* Wait for IP address */
		if (!(arp_port->ap_flags & APF_INADDR_SET))
			return;

		/* fall through */
	case APS_ARPSTART:
		arp_port->ap_state= APS_ARPPROTO;

		{
			arp_cache_t *cache;
			int i;

			cache= arp_cache;
			for (i=0; i<ARP_CACHE_NR; i++, cache++)
			{
				cache->ac_state= ACS_UNUSED;
				cache->ac_flags= ACF_EMPTY;
				cache->ac_expire= 0;
				cache->ac_lastuse= 0;
			}
		}
		result= eth_ioctl (arp_port->ap_eth_fd, NWIOSETHOPT);

		if (result==NW_SUSPEND)
		{
			arp_port->ap_flags |= APF_SUSPEND;
			return;
		}
		assert(result == NW_OK);

		/* fall through */
	case APS_ARPPROTO:
		arp_port->ap_state= APS_ARPMAIN;
		if (arp_port->ap_flags & APF_MORE2WRITE)
			setup_write(arp_port);
		setup_read(arp_port);
		return;

#if !CRAMPED
	default:
		ip_panic((
		 "arp_main(&arp_port_table[%d]) called but ap_state=0x%x\n",
			arp_port->ap_eth_port, arp_port->ap_state ));
#endif
	}
}

PRIVATE acc_t *arp_getdata (fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	arp_port_t *arp_port;
	arp46_t *arp;
	acc_t *data;
	int result;

	arp_port= &arp_port_table[fd];

	switch (arp_port->ap_state)
	{
	case APS_ARPPROTO:
		if (!count)
		{
			result= (int)offset;
			if (result<0)
			{
				arp_port->ap_state= APS_ERROR;
				break;
			}
			if (arp_port->ap_flags & APF_SUSPEND)
			{
				arp_port->ap_flags &= ~APF_SUSPEND;
				arp_main(arp_port);
			}
			return NW_OK;
		}
		assert ((!offset) && (count == sizeof(struct nwio_ethopt)));
		{
			struct nwio_ethopt *ethopt;
			acc_t *acc;

			acc= bf_memreq(sizeof(*ethopt));
			ethopt= (struct nwio_ethopt *)ptr2acc_data(acc);
			ethopt->nweo_flags= NWEO_COPY|NWEO_EN_BROAD|
				NWEO_TYPESPEC;
			ethopt->nweo_type= HTONS(ETH_ARP_PROTO);
			return acc;
		}
	case APS_ARPMAIN:
		assert (arp_port->ap_flags & APF_ARP_WR_IP);
		if (!count)
		{
			result= (int)offset;
			if (result<0)
			{
				DIFBLOCK(1, (result != NW_SUSPEND),
					printf(
				"arp.c: write error on port %d: error %d\n",
					fd, result));

				arp_port->ap_state= APS_ERROR;
				break;
			}
			arp_port->ap_flags &= ~APF_ARP_WR_IP;
			if (arp_port->ap_flags & APF_ARP_WR_SP)
				setup_write(arp_port);
			return NW_OK;
		}
		assert (offset+count <= sizeof(arp46_t));
		data= bf_memreq(sizeof(arp46_t));
		arp= (arp46_t *)ptr2acc_data(data);
		data->acc_offset += offset;
		data->acc_length= count;
		if (arp_port->ap_write_code == ARP_REPLY)
			arp->a46_dstaddr= arp_port->ap_write_ethaddr;
		else
		{
			arp->a46_dstaddr.ea_addr[0]= 0xff;
			arp->a46_dstaddr.ea_addr[1]= 0xff;
			arp->a46_dstaddr.ea_addr[2]= 0xff;
			arp->a46_dstaddr.ea_addr[3]= 0xff;
			arp->a46_dstaddr.ea_addr[4]= 0xff;
			arp->a46_dstaddr.ea_addr[5]= 0xff;
		}
		arp->a46_hdr= HTONS(ARP_ETHERNET);
		arp->a46_pro= HTONS(ETH_IP_PROTO);
		arp->a46_hln= 6;
		arp->a46_pln= 4;
		arp->a46_op= htons(arp_port->ap_write_code);
		arp->a46_sha= arp_port->ap_ethaddr;
		memcpy (arp->a46_spa, &arp_port->ap_ipaddr, sizeof(ipaddr_t));
		arp->a46_tha= arp_port->ap_write_ethaddr;
		memcpy (arp->a46_tpa, &arp_port->ap_write_ipaddr,
			sizeof(ipaddr_t));
		return data;
	default:
#if !CRAMPED
		printf("arp_getdata(%d, 0x%d, 0x%d) called but ap_state=0x%x\n",
			fd, offset, count, arp_port->ap_state);
#endif
		break;
	}
	return 0;
}

PRIVATE int arp_putdata (fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	arp_port_t *arp_port;
	int result;
	struct nwio_ethstat *ethstat;

	arp_port= &arp_port_table[fd];

	if (arp_port->ap_flags & APF_ARP_RD_IP)
	{
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				DIFBLOCK(1, (result != NW_SUSPEND), printf(
				"arp.c: read error on port %d: error %d\n",
					fd, result));

				return NW_OK;
			}
			if (arp_port->ap_flags & APF_ARP_RD_SP)
			{
				arp_port->ap_flags &= ~(APF_ARP_RD_IP|
					APF_ARP_RD_SP);
				setup_read(arp_port);
			}
			else
				arp_port->ap_flags &= ~(APF_ARP_RD_IP|
					APF_ARP_RD_SP);
			return NW_OK;
		}
		assert (!offset);
		/* Warning: the above assertion is illegal; puts and gets of
		   data can be brokenup in any piece the server likes. However
		   we assume that the server is eth.c and it transfers only
		   whole packets. */
		data= bf_packIffLess(data, sizeof(arp46_t));
		if (data->acc_length >= sizeof(arp46_t))
			process_arp_req(arp_port,data);
		bf_afree(data);
		return NW_OK;
	}
	switch (arp_port->ap_state)
	{
	case APS_GETADDR:
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				arp_port->ap_state= APS_ERROR;
				break;
			}
			if (arp_port->ap_flags & APF_SUSPEND)
			{
				arp_port->ap_flags &= ~APF_SUSPEND;
				arp_main(arp_port);
			}
			return NW_OK;
		}
		compare (bf_bufsize(data), ==, sizeof(*ethstat));
		data= bf_packIffLess(data, sizeof(*ethstat));
		compare (data->acc_length, ==, sizeof(*ethstat));
		ethstat= (struct nwio_ethstat *)ptr2acc_data(data);
		arp_port->ap_ethaddr= ethstat->nwes_addr;
		bf_afree(data);
		return NW_OK;
	default:
#if !CRAMPED
		printf("arp_putdata(%d, 0x%d, 0x%lx) called but ap_state=0x%x\n",
			fd, offset, (unsigned long)data, arp_port->ap_state);
#endif
		break;
	}
	return EGENERIC;
}

PRIVATE void setup_read(arp_port)
arp_port_t *arp_port;
{
	int result;

	while (!(arp_port->ap_flags & APF_ARP_RD_IP))
	{
		arp_port->ap_flags |= APF_ARP_RD_IP;
		result= eth_read (arp_port->ap_eth_fd, ETH_MAX_PACK_SIZE);
		if (result == NW_SUSPEND)
		{
			arp_port->ap_flags |= APF_ARP_RD_SP;
			return;
		}
		DIFBLOCK(1, (result != NW_OK),
			printf("arp.c: eth_read(..,%d)=%d\n",
			ETH_MAX_PACK_SIZE, result));
	}
}

PRIVATE void setup_write(arp_port)
arp_port_t *arp_port;
{
	int i, result;

	while (arp_port->ap_flags & APF_MORE2WRITE)
	{
		if (arp_port->ap_flags & APF_CLIENTWRITE)
		{
			arp_port->ap_flags &= ~APF_CLIENTWRITE;
			arp_port->ap_write_ipaddr= arp_port->ap_req_ipaddr;
			arp_port->ap_write_code= ARP_REQUEST;
			clck_timer(&arp_port->ap_timer,
				get_time() + ARP_TIMEOUT,
				arp_timeout, arp_port->ap_eth_port);
		}
		else
		{
			arp_cache_t *cache;

			cache= arp_cache;
			for (i=0; i<ARP_CACHE_NR; i++, cache++)
			{
				if ((cache->ac_flags & ACF_GOTREQ) &&
					cache->ac_port == arp_port)
				{
					cache->ac_flags &= ~ACF_GOTREQ;
					arp_port->ap_write_ethaddr= cache->
						ac_ethaddr;
					arp_port->ap_write_ipaddr= cache->
						ac_ipaddr;
					arp_port->ap_write_code= ARP_REPLY;
					break;
				}
			}
			if (i>=ARP_CACHE_NR)
			{
				arp_port->ap_flags &= ~APF_MORE2WRITE;
				break;
			}
		}
		arp_port->ap_flags= (arp_port->ap_flags & ~APF_ARP_WR_SP) |
			APF_ARP_WR_IP;
		result= eth_write(arp_port->ap_eth_fd, sizeof(arp46_t));
		if (result == NW_SUSPEND)
			arp_port->ap_flags |= APF_ARP_WR_SP;
		if (result<0)
		{
			DIFBLOCK(1, (result != NW_SUSPEND),
				printf("arp.c: eth_write(..,%d)=%d\n",
				sizeof(arp46_t), result));
			return;
		}
	}
}

PRIVATE void process_arp_req (arp_port, data)
arp_port_t *arp_port;
acc_t *data;
{
	arp46_t *arp;
	arp_cache_t *ce;
	int level;
	time_t curr_time;
	ipaddr_t spa, tpa;

	curr_time= get_time();

	arp= (arp46_t *)ptr2acc_data(data);
	memcpy(&spa, arp->a46_spa, sizeof(ipaddr_t));
	memcpy(&tpa, arp->a46_tpa, sizeof(ipaddr_t));

	if (arp->a46_hdr != HTONS(ARP_ETHERNET) ||
		arp->a46_hln != 6 ||
		arp->a46_pro != HTONS(ETH_IP_PROTO) ||
		arp->a46_pln != 4)
		return;
	ce= find_cache_ent(arp_port, spa);
	if (ce && ce->ac_expire < curr_time)
	{
		DBLOCK(0x10, printf("arp: expiring entry for ");
			writeIpAddr(ce->ac_ipaddr); printf("\n"));
		ce->ac_state= ACS_UNUSED;
		ce= NULL;
	}
	if (ce == NULL)
	{
		if (tpa != arp_port->ap_ipaddr)
			return;

		DBLOCK(0x10, printf("arp: allocating entry for ");
			writeIpAddr(spa); printf("\n"));

		ce= alloc_cache_ent();
		ce->ac_flags= ACF_EMPTY;
		ce->ac_state= ACS_VALID;
		ce->ac_ethaddr= arp->a46_sha;
		ce->ac_ipaddr= spa;
		ce->ac_port= arp_port;
		ce->ac_expire= curr_time+ARP_EXP_TIME;
		ce->ac_lastuse= curr_time-ARP_INUSE_OFFSET; /* never used */
	}

	if (ce->ac_state == ACS_INCOMPLETE || ce->ac_state == ACS_UNREACHABLE)
	{
		ce->ac_ethaddr= arp->a46_sha;
		if (ce->ac_state == ACS_INCOMPLETE)
		{
			ce->ac_state= ACS_VALID;
			client_reply(arp_port, spa, &arp->a46_sha);
		}
		else
			ce->ac_state= ACS_VALID;
	}

	/* Update fields in the arp cache. */
#if !CRAMPED
	if (memcmp(&ce->ac_ethaddr, &arp->a46_sha,
		sizeof(ce->ac_ethaddr)) != 0)
	{
		printf("arp: ethernet address for IP address ");
		writeIpAddr(spa);
		printf(" changed from ");
		writeEtherAddr(&ce->ac_ethaddr);
		printf(" to ");
		writeEtherAddr(&arp->a46_sha);
		printf("\n");
		ce->ac_ethaddr= arp->a46_sha;
	}
#else
	ce->ac_ethaddr= arp->a46_sha;
#endif
	ce->ac_expire= curr_time+ARP_EXP_TIME;

	if (arp->a46_op == HTONS(ARP_REQUEST) && (tpa == arp_port->ap_ipaddr))
	{
		ce->ac_flags |= ACF_GOTREQ;
		arp_port->ap_flags |= APF_MORE2WRITE;
		if (!(arp_port->ap_flags & APF_ARP_WR_IP))
			setup_write(arp_port);
	}
}

PRIVATE void client_reply (arp_port, ipaddr, ethaddr)
arp_port_t *arp_port;
ipaddr_t ipaddr;
ether_addr_t *ethaddr;
{
	if ((arp_port->ap_flags & APF_CLIENTREQ) &&
		ipaddr == arp_port->ap_req_ipaddr)
	{
		arp_port->ap_flags &= ~(APF_CLIENTREQ|APF_CLIENTWRITE);
		clck_untimer(&arp_port->ap_timer);
	}
	(*arp_port->ap_arp_func)(arp_port->ap_ip_port, ipaddr, ethaddr);
}

PRIVATE arp_cache_t *find_cache_ent (arp_port, ipaddr)
arp_port_t *arp_port;
ipaddr_t ipaddr;
{
	arp_cache_t *cache;
	int i;

	for (i=0, cache= arp_cache; i<ARP_CACHE_NR; i++, cache++)
	{
		if (cache->ac_state != ACS_UNUSED &&
			cache->ac_port == arp_port &&
			cache->ac_ipaddr == ipaddr)
		{
			return cache;
		}
	}
	return NULL;
}

PRIVATE arp_cache_t *alloc_cache_ent()
{
	arp_cache_t *cache, *old;
	int i;

	old= NULL;
	for (i=0, cache= arp_cache; i<ARP_CACHE_NR; i++, cache++)
	{
		if (cache->ac_state == ACS_UNUSED)
			return cache;
		if (cache->ac_state == ACS_INCOMPLETE)
			continue;
		if (!old || cache->ac_lastuse < old->ac_lastuse)
			old= cache;
	}
	assert(old);
	return old;
}

PUBLIC void arp_set_ipaddr (eth_port, ipaddr)
int eth_port;
ipaddr_t ipaddr;
{
	arp_port_t *arp_port;
	int i;

	if (eth_port < 0 || eth_port >= eth_conf_nr)
		return;
	arp_port= &arp_port_table[eth_port];

	arp_port->ap_ipaddr= ipaddr;
	arp_port->ap_flags |= APF_INADDR_SET;
	arp_port->ap_flags &= ~APF_SUSPEND;
	if (arp_port->ap_state == APS_GETADDR)
		arp_main(arp_port);
}

PUBLIC int arp_set_cb(eth_port, ip_port, arp_func)
int eth_port;
int ip_port;
arp_func_t arp_func;
{
	arp_port_t *arp_port;
	int i;

	assert(eth_port >= 0);
	if (eth_port >= eth_conf_nr)
		return ENXIO;

	arp_port= &arp_port_table[eth_port];
	arp_port->ap_eth_port= eth_port;
	arp_port->ap_ip_port= ip_port;
	arp_port->ap_state= APS_INITIAL;
	arp_port->ap_flags= APF_EMPTY;
	arp_port->ap_arp_func= arp_func;

	arp_main(arp_port);

	return NW_OK;
}

PUBLIC int arp_ip_eth (eth_port, ipaddr, ethaddr)
int eth_port;
ipaddr_t ipaddr;
ether_addr_t *ethaddr;
{
	arp_port_t *arp_port;
	int i;
	arp_cache_t *ce;
	time_t curr_time;

	assert(eth_port >= 0 && eth_port < eth_conf_nr);
	arp_port= &arp_port_table[eth_port];
	assert(arp_port->ap_state == APS_ARPMAIN ||
		(printf("ap_state= %d\n", arp_port->ap_state), 0));

	curr_time= get_time();

	ce= find_cache_ent (arp_port, ipaddr);
	if (ce && ce->ac_expire < curr_time)
	{
		ce->ac_state= ACS_UNUSED;
		ce= NULL;
	}
	if (ce)
	{
		/* Found an entry. This entry should be valid, unreachable
		 * or incomplete.
		 */
		ce->ac_lastuse= curr_time;
		if (ce->ac_state == ACS_VALID)
		{
			*ethaddr= ce->ac_ethaddr;
			return NW_OK;
		}
		if (ce->ac_state == ACS_UNREACHABLE)
			return EDSTNOTRCH;
		assert(ce->ac_state == ACS_INCOMPLETE);
		return NW_SUSPEND;
	}

	if (arp_port->ap_flags & APF_CLIENTREQ)
	{
		/* We should implement something to be able to do
		 * multiple arp lookups at the same time. At the moment
		 * we just return SUSPEND.
		 */
		return NW_SUSPEND;
	}
	ce= alloc_cache_ent();
	ce->ac_flags= 0;
	ce->ac_state= ACS_INCOMPLETE;
	ce->ac_ipaddr= ipaddr;
	ce->ac_port= arp_port;
	ce->ac_expire= curr_time+ARP_EXP_TIME;
	ce->ac_lastuse= curr_time;
	arp_port->ap_flags |= APF_CLIENTREQ|APF_MORE2WRITE | APF_CLIENTWRITE;
	arp_port->ap_req_ipaddr= ipaddr;
	arp_port->ap_req_count= 0;
	if (!(arp_port->ap_flags & APF_ARP_WR_IP))
		setup_write(arp_port);
	return NW_SUSPEND;
}

PRIVATE void arp_timeout (fd, timer)
int fd;
timer_t *timer;
{
	arp_port_t *arp_port;
	arp_cache_t *ce;
	int level;
	time_t curr_time;

	arp_port= &arp_port_table[fd];

	assert (timer == &arp_port->ap_timer);

	if (++arp_port->ap_req_count < MAX_ARP_RETRIES)
	{
		arp_port->ap_flags |= APF_CLIENTWRITE|APF_MORE2WRITE;
		if (!(arp_port->ap_flags & APF_ARP_WR_IP))
			setup_write(arp_port);
	}
	else
	{
		ce= find_cache_ent(arp_port, arp_port->ap_req_ipaddr);
		if (ce) {
			assert(ce->ac_state == ACS_INCOMPLETE ||
				(printf("ce->ac_state= %d\n", ce->ac_state),0));
			curr_time= get_time();
			ce->ac_state= ACS_UNREACHABLE;
			ce->ac_expire= curr_time+ ARP_NOTRCH_EXP_TIME;
			ce->ac_lastuse= curr_time;

			client_reply(arp_port, ce->ac_ipaddr, NULL);
		}
	}
}

/*
 * $PchId: arp.c,v 1.6 1995/11/21 06:45:27 philip Exp $
 */
