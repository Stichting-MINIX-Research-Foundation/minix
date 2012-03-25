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
#include "event.h"
#include "eth.h"
#include "io.h"

THIS_FILE

#define ARP_CACHE_NR	 256
#define AP_REQ_NR	  32

#define ARP_HASH_NR	256
#define ARP_HASH_MASK	0xff
#define ARP_HASH_WIDTH	4

#define MAX_ARP_RETRIES		5
#define ARP_TIMEOUT		(HZ/2+1)	/* .5 seconds */
#ifndef ARP_EXP_TIME
#define ARP_EXP_TIME		(20L*60L*HZ)	/* 20 minutes */
#endif
#define ARP_NOTRCH_EXP_TIME	(30*HZ)		/* 30 seconds */
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

	ether_addr_t ap_ethaddr;	/* Ethernet address of this port */
	ipaddr_t ap_ipaddr;		/* IP address of this port */

	struct arp_req
	{
		timer_t ar_timer;
		int ar_entry;
		int ar_req_count;
	} ap_req[AP_REQ_NR];

	arp_func_t ap_arp_func;

	acc_t *ap_sendpkt;
	acc_t *ap_sendlist;
	acc_t *ap_reclist;
	event_t ap_event;
} arp_port_t;

#define APF_EMPTY	0x00
#define APF_ARP_RD_IP	0x01
#define APF_ARP_RD_SP	0x02
#define APF_ARP_WR_IP	0x04
#define APF_ARP_WR_SP	0x08
#define APF_INADDR_SET	0x10
#define APF_SUSPEND	0x20

#define APS_INITIAL	1
#define	APS_GETADDR	2
#define	APS_ARPSTART	3
#define	APS_ARPPROTO	4
#define	APS_ARPMAIN	5
#define	APS_ERROR	6

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
#define ACF_PERM	1
#define ACF_PUB		2

#define ACS_UNUSED	0
#define ACS_INCOMPLETE	1
#define ACS_VALID	2
#define ACS_UNREACHABLE	3

static struct arp_hash_ent
{
	arp_cache_t *ahe_row[ARP_HASH_WIDTH];
} arp_hash[ARP_HASH_NR];

static arp_port_t *arp_port_table;
static	arp_cache_t *arp_cache;
static int arp_cache_nr;

static acc_t *arp_getdata ARGS(( int fd, size_t offset,
	size_t count, int for_ioctl ));
static int arp_putdata ARGS(( int fd, size_t offset,
	acc_t *data, int for_ioctl ));
static void arp_main ARGS(( arp_port_t *arp_port ));
static void arp_timeout ARGS(( int ref, timer_t *timer ));
static void setup_write ARGS(( arp_port_t *arp_port ));
static void setup_read ARGS(( arp_port_t *arp_port ));
static void do_reclist ARGS(( event_t *ev, ev_arg_t ev_arg ));
static void process_arp_pkt ARGS(( arp_port_t *arp_port, acc_t *data ));
static void client_reply ARGS(( arp_port_t *arp_port,
	ipaddr_t ipaddr, ether_addr_t *ethaddr ));
static arp_cache_t *find_cache_ent ARGS(( arp_port_t *arp_port,
	ipaddr_t ipaddr ));
static arp_cache_t *alloc_cache_ent ARGS(( int flags ));
static void arp_buffree ARGS(( int priority ));
#ifdef BUF_CONSISTENCY_CHECK
static void arp_bufcheck ARGS(( void ));
#endif

void arp_prep()
{
	arp_port_table= alloc(eth_conf_nr * sizeof(arp_port_table[0]));

	arp_cache_nr= ARP_CACHE_NR;
	if (arp_cache_nr < (eth_conf_nr+1)*AP_REQ_NR)
	{
		arp_cache_nr= (eth_conf_nr+1)*AP_REQ_NR;
		printf("arp: using %d cache entries instead of %d\n",
			arp_cache_nr, ARP_CACHE_NR);
	}
	arp_cache= alloc(arp_cache_nr * sizeof(arp_cache[0]));
}

void arp_init()
{
	arp_port_t *arp_port;
	arp_cache_t *cache;
	int i;

	assert (BUF_S >= sizeof(struct nwio_ethstat));
	assert (BUF_S >= sizeof(struct nwio_ethopt));
	assert (BUF_S >= sizeof(arp46_t));

	for (i=0, arp_port= arp_port_table; i<eth_conf_nr; i++, arp_port++)
	{
		arp_port->ap_state= APS_ERROR;	/* Mark all ports as
						 * unavailable */
	}

	cache= arp_cache;
	for (i=0; i<arp_cache_nr; i++, cache++)
	{
		cache->ac_state= ACS_UNUSED;
		cache->ac_flags= ACF_EMPTY;
		cache->ac_expire= 0;
		cache->ac_lastuse= 0;
	}

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(arp_buffree);
#else
	bf_logon(arp_buffree, arp_bufcheck);
#endif
}

static void arp_main(arp_port)
arp_port_t *arp_port;
{
	int result;

	switch (arp_port->ap_state)
	{
	case APS_INITIAL:
		arp_port->ap_eth_fd= eth_open(arp_port->ap_eth_port,
			arp_port->ap_eth_port, arp_getdata, arp_putdata,
			0 /* no put_pkt */, 0 /* no select_res */);

		if (arp_port->ap_eth_fd<0)
		{
			DBLOCK(1, printf("arp[%d]: unable to open eth[%d]\n",
				arp_port-arp_port_table,
				arp_port->ap_eth_port));
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
		setup_write(arp_port);
		setup_read(arp_port);
		return;

	default:
		ip_panic((
		 "arp_main(&arp_port_table[%d]) called but ap_state=0x%x\n",
			arp_port->ap_eth_port, arp_port->ap_state ));
	}
}

static acc_t *arp_getdata (fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	arp_port_t *arp_port;
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
			data= arp_port->ap_sendpkt;
			arp_port->ap_sendpkt= NULL;
			assert(data);
			bf_afree(data); data= NULL;

			result= (int)offset;
			if (result<0)
			{
				DIFBLOCK(1, (result != NW_SUSPEND),
					printf(
				"arp[%d]: write error on port %d: error %d\n",
					fd, arp_port->ap_eth_fd, result));

				arp_port->ap_state= APS_ERROR;
				break;
			}
			arp_port->ap_flags &= ~APF_ARP_WR_IP;
			if (arp_port->ap_flags & APF_ARP_WR_SP)
				setup_write(arp_port);
			return NW_OK;
		}
		assert (offset+count <= sizeof(arp46_t));
		data= arp_port->ap_sendpkt;
		assert(data);
		data= bf_cut(data, offset, count);

		return data;
	default:
		printf("arp_getdata(%d, 0x%d, 0x%d) called but ap_state=0x%x\n",
			fd, offset, count, arp_port->ap_state);
		break;
	}
	return 0;
}

static int arp_putdata (fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	arp_port_t *arp_port;
	int result;
	struct nwio_ethstat *ethstat;
	ev_arg_t ev_arg;
	acc_t *tmpacc;

	arp_port= &arp_port_table[fd];

	if (arp_port->ap_flags & APF_ARP_RD_IP)
	{
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				DIFBLOCK(1, (result != NW_SUSPEND), printf(
				"arp[%d]: read error on port %d: error %d\n",
					fd, arp_port->ap_eth_fd, result));

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
		   whole packets.
		   */
		data= bf_packIffLess(data, sizeof(arp46_t));
		if (data->acc_length >= sizeof(arp46_t))
		{
			if (!arp_port->ap_reclist)
			{
				ev_arg.ev_ptr= arp_port;
				ev_enqueue(&arp_port->ap_event, do_reclist,
					ev_arg);
			}
			if (data->acc_linkC != 1)
			{
				tmpacc= bf_dupacc(data);
				bf_afree(data);
				data= tmpacc;
				tmpacc= NULL;
			}
			data->acc_ext_link= arp_port->ap_reclist;
			arp_port->ap_reclist= data;
		}
		else
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
		printf("arp_putdata(%d, 0x%d, 0x%lx) called but ap_state=0x%x\n",
			fd, offset, (unsigned long)data, arp_port->ap_state);
		break;
	}
	return EGENERIC;
}

static void setup_read(arp_port)
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
			printf("arp[%d]: eth_read(..,%d)=%d\n",
			arp_port-arp_port_table, ETH_MAX_PACK_SIZE, result));
	}
}

static void setup_write(arp_port)
arp_port_t *arp_port;
{
	int result;
	acc_t *data;

	for(;;)
	{
		data= arp_port->ap_sendlist;
		if (!data)
			break;
		arp_port->ap_sendlist= data->acc_ext_link;

		if (arp_port->ap_ipaddr == HTONL(0x00000000))
		{
			/* Interface is down */
			printf(
		"arp[%d]: not sending ARP packet, interface is down\n",
				arp_port-arp_port_table);
			bf_afree(data); data= NULL;
			continue;
		}

		assert(!arp_port->ap_sendpkt);
		arp_port->ap_sendpkt= data; data= NULL;
			
		arp_port->ap_flags= (arp_port->ap_flags & ~APF_ARP_WR_SP) |
			APF_ARP_WR_IP;
		result= eth_write(arp_port->ap_eth_fd, sizeof(arp46_t));
		if (result == NW_SUSPEND)
		{
			arp_port->ap_flags |= APF_ARP_WR_SP;
			break;
		}
		if (result<0)
		{
			DIFBLOCK(1, (result != NW_SUSPEND),
				printf("arp[%d]: eth_write(..,%d)=%d\n",
				arp_port-arp_port_table, sizeof(arp46_t),
				result));
			return;
		}
	}
}

static void do_reclist(ev, ev_arg)
event_t *ev;
ev_arg_t ev_arg;
{
	arp_port_t *arp_port;
	acc_t *data;

	arp_port= ev_arg.ev_ptr;
	assert(ev == &arp_port->ap_event);

	while (data= arp_port->ap_reclist, data != NULL)
	{
		arp_port->ap_reclist= data->acc_ext_link;
		process_arp_pkt(arp_port, data);
		bf_afree(data);
	}
}

static void process_arp_pkt (arp_port, data)
arp_port_t *arp_port;
acc_t *data;
{
	int i, entry, do_reply;
	arp46_t *arp;
	u16_t *p;
	arp_cache_t *ce, *cache;
	struct arp_req *reqp;
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
	if (arp_port->ap_ipaddr == HTONL(0x00000000))
	{
		/* Interface is down */
#if DEBUG
		printf("arp[%d]: dropping ARP packet, interface is down\n",
			arp_port-arp_port_table);
#endif
		return;
	}

	ce= find_cache_ent(arp_port, spa);
	cache= NULL;	/* lint */

	do_reply= 0;
	if (arp->a46_op != HTONS(ARP_REQUEST))
		;	/* No need to reply */
	else if (tpa == arp_port->ap_ipaddr)
		do_reply= 1;
	else
	{
		/* Look for a published entry */
		cache= find_cache_ent(arp_port, tpa);
		if (cache)
		{
			if (cache->ac_flags & ACF_PUB)
			{
				/* Published entry */
				do_reply= 1;
			}
			else
			{
				/* Nothing to do */
				cache= NULL;
			}
		}
	}

	if (ce == NULL)
	{
		if (!do_reply)
			return;

		DBLOCK(0x10, printf("arp[%d]: allocating entry for ",
			arp_port-arp_port_table);
			writeIpAddr(spa); printf("\n"));

		ce= alloc_cache_ent(ACF_EMPTY);
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
			/* Find request entry */
			entry= ce-arp_cache;
			for (i= 0, reqp= arp_port->ap_req; i<AP_REQ_NR; 
				i++, reqp++)
			{
				if (reqp->ar_entry == entry)
					break;
			}
			assert(i < AP_REQ_NR);
			clck_untimer(&reqp->ar_timer);
			reqp->ar_entry= -1;
			
			ce->ac_state= ACS_VALID;
			client_reply(arp_port, spa, &arp->a46_sha);
		}
		else
			ce->ac_state= ACS_VALID;
	}

	/* Update fields in the arp cache. */
	if (memcmp(&ce->ac_ethaddr, &arp->a46_sha,
		sizeof(ce->ac_ethaddr)) != 0)
	{
		printf("arp[%d]: ethernet address for IP address ",
			arp_port-arp_port_table);
		writeIpAddr(spa);
		printf(" changed from ");
		writeEtherAddr(&ce->ac_ethaddr);
		printf(" to ");
		writeEtherAddr(&arp->a46_sha);
		printf("\n");
		ce->ac_ethaddr= arp->a46_sha;
	}
	ce->ac_expire= curr_time+ARP_EXP_TIME;

	if (do_reply)
	{
		data= bf_memreq(sizeof(arp46_t));
		arp= (arp46_t *)ptr2acc_data(data);

		/* Clear padding */
		assert(sizeof(arp->a46_data.a46_dummy) % sizeof(*p) == 0);
		for (i= 0, p= (u16_t *)arp->a46_data.a46_dummy;
			i < sizeof(arp->a46_data.a46_dummy)/sizeof(*p);
			i++, p++)
		{
			*p= 0xdead;
		}

		arp->a46_dstaddr= ce->ac_ethaddr;
		arp->a46_hdr= HTONS(ARP_ETHERNET);
		arp->a46_pro= HTONS(ETH_IP_PROTO);
		arp->a46_hln= 6;
		arp->a46_pln= 4;

		arp->a46_op= htons(ARP_REPLY);
		if (tpa == arp_port->ap_ipaddr)
		{
			arp->a46_sha= arp_port->ap_ethaddr;
		}
		else
		{
			assert(cache);
			arp->a46_sha= cache->ac_ethaddr;
		}
		memcpy (arp->a46_spa, &tpa, sizeof(ipaddr_t));
		arp->a46_tha= ce->ac_ethaddr;
		memcpy (arp->a46_tpa, &ce->ac_ipaddr, sizeof(ipaddr_t));

		assert(data->acc_linkC == 1);
		data->acc_ext_link= arp_port->ap_sendlist;
		arp_port->ap_sendlist= data; data= NULL;

		if (!(arp_port->ap_flags & APF_ARP_WR_IP))
			setup_write(arp_port);
	}
}

static void client_reply (arp_port, ipaddr, ethaddr)
arp_port_t *arp_port;
ipaddr_t ipaddr;
ether_addr_t *ethaddr;
{
	(*arp_port->ap_arp_func)(arp_port->ap_ip_port, ipaddr, ethaddr);
}

static arp_cache_t *find_cache_ent (arp_port, ipaddr)
arp_port_t *arp_port;
ipaddr_t ipaddr;
{
	arp_cache_t *ce;
	int i;
	unsigned hash;

	hash= (ipaddr >> 24) ^ (ipaddr >> 16) ^ (ipaddr >> 8) ^ ipaddr;
	hash &= ARP_HASH_MASK;

	ce= arp_hash[hash].ahe_row[0];
	if (ce && ce->ac_ipaddr == ipaddr && ce->ac_port == arp_port &&
		ce->ac_state != ACS_UNUSED)
	{
		return ce;
	}
	for (i= 1; i<ARP_HASH_WIDTH; i++)
	{
		ce= arp_hash[hash].ahe_row[i];
		if (!ce || ce->ac_ipaddr != ipaddr || ce->ac_port != arp_port
			|| ce->ac_state == ACS_UNUSED)
		{
			continue;
		}
		arp_hash[hash].ahe_row[i]= arp_hash[hash].ahe_row[0];
		arp_hash[hash].ahe_row[0]= ce;
		return ce;
	}

	for (i=0, ce= arp_cache; i<arp_cache_nr; i++, ce++)
	{
		if (ce->ac_state != ACS_UNUSED &&
			ce->ac_port == arp_port &&
			ce->ac_ipaddr == ipaddr)
		{
			for (i= ARP_HASH_WIDTH-1; i>0; i--)
			{
				arp_hash[hash].ahe_row[i]=
					arp_hash[hash].ahe_row[i-1];
			}
			assert(i == 0);
			arp_hash[hash].ahe_row[0]= ce;
			return ce;
		}
	}
	return NULL;
}

static arp_cache_t *alloc_cache_ent(flags)
int flags;
{
	arp_cache_t *cache, *old;
	int i;

	old= NULL;
	for (i=0, cache= arp_cache; i<arp_cache_nr; i++, cache++)
	{
		if (cache->ac_state == ACS_UNUSED)
		{
			old= cache;
			break;
		}
		if (cache->ac_state == ACS_INCOMPLETE)
			continue;
		if (cache->ac_flags & ACF_PERM)
			continue;
		if (!old || cache->ac_lastuse < old->ac_lastuse)
			old= cache;
	}
	assert(old);

	if (!flags)
		return old;

	/* Get next permanent entry */
	for (i=0, cache= arp_cache; i<arp_cache_nr; i++, cache++)
	{
		if (cache->ac_state == ACS_UNUSED)
			break;
		if (cache->ac_flags & ACF_PERM)
			continue;
		break;
	}
	if (i >= arp_cache_nr/2)
		return NULL; /* Too many entries */
	if (cache != old)
	{
		assert(old > cache);
		*old= *cache;
		old= cache;
	}

	if (!(flags & ACF_PUB))
		return old;

	/* Get first nonpublished entry */
	for (i=0, cache= arp_cache; i<arp_cache_nr; i++, cache++)
	{
		if (cache->ac_state == ACS_UNUSED)
			break;
		if (cache->ac_flags & ACF_PUB)
			continue;
		break;
	}
	if (cache != old)
	{
		assert(old > cache);
		*old= *cache;
		old= cache;
	}
	return old;
}

void arp_set_ipaddr (eth_port, ipaddr)
int eth_port;
ipaddr_t ipaddr;
{
	arp_port_t *arp_port;

	if (eth_port < 0 || eth_port >= eth_conf_nr)
		return;
	arp_port= &arp_port_table[eth_port];

	arp_port->ap_ipaddr= ipaddr;
	arp_port->ap_flags |= APF_INADDR_SET;
	arp_port->ap_flags &= ~APF_SUSPEND;
	if (arp_port->ap_state == APS_GETADDR)
		arp_main(arp_port);
}

int arp_set_cb(eth_port, ip_port, arp_func)
int eth_port;
int ip_port;
arp_func_t arp_func;
{
	int i;
	arp_port_t *arp_port;

	assert(eth_port >= 0);
	if (eth_port >= eth_conf_nr)
		return ENXIO;

	arp_port= &arp_port_table[eth_port];
	arp_port->ap_eth_port= eth_port;
	arp_port->ap_ip_port= ip_port;
	arp_port->ap_state= APS_INITIAL;
	arp_port->ap_flags= APF_EMPTY;
	arp_port->ap_arp_func= arp_func;
	arp_port->ap_sendpkt= NULL;
	arp_port->ap_sendlist= NULL;
	arp_port->ap_reclist= NULL;
	for (i= 0; i<AP_REQ_NR; i++) {
		arp_port->ap_req[i].ar_entry= -1;
		arp_port->ap_req[i].ar_timer.tim_active= 0;
	}

	ev_init(&arp_port->ap_event);

	arp_main(arp_port);

	return NW_OK;
}

int arp_ip_eth (eth_port, ipaddr, ethaddr)
int eth_port;
ipaddr_t ipaddr;
ether_addr_t *ethaddr;
{
	int i, ref;
	arp_port_t *arp_port;
	struct arp_req *reqp;
	arp_cache_t *ce;
	time_t curr_time;

	assert(eth_port >= 0 && eth_port < eth_conf_nr);
	arp_port= &arp_port_table[eth_port];
	assert(arp_port->ap_state == APS_ARPMAIN ||
		(printf("arp[%d]: ap_state= %d\n", arp_port-arp_port_table,
		arp_port->ap_state), 0));

	curr_time= get_time();

	ce= find_cache_ent (arp_port, ipaddr);
	if (ce && ce->ac_expire < curr_time)
	{
		assert(ce->ac_state != ACS_INCOMPLETE);

		/* Check whether there is enough space for an ARP
		 * request or not.
		 */
		for (i= 0, reqp= arp_port->ap_req; i<AP_REQ_NR; i++, reqp++)
		{
			if (reqp->ar_entry < 0)
				break;
		}
		if (i < AP_REQ_NR)
		{
			/* Okay, expire this entry. */
			ce->ac_state= ACS_UNUSED;
			ce= NULL;
		}
		else
		{
			/* Continue using this entry for a while */
			printf("arp[%d]: Overloaded! Keeping entry for ",
				arp_port-arp_port_table);
			writeIpAddr(ipaddr);
			printf("\n");
			ce->ac_expire= curr_time+ARP_NOTRCH_EXP_TIME;
		}
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
			return EHOSTUNREACH;
		assert(ce->ac_state == ACS_INCOMPLETE);

		return NW_SUSPEND;
	}

	/* Find an empty slot for an ARP request */
	for (i= 0, reqp= arp_port->ap_req; i<AP_REQ_NR; i++, reqp++)
	{
		if (reqp->ar_entry < 0)
			break;
	}
	if (i >= AP_REQ_NR)
	{
		/* We should be able to report that this ARP request
		 * cannot be accepted. At the moment we just return SUSPEND.
		 */
		return NW_SUSPEND;
	}
	ref= (eth_port*AP_REQ_NR + i);

	ce= alloc_cache_ent(ACF_EMPTY);
	ce->ac_flags= 0;
	ce->ac_state= ACS_INCOMPLETE;
	ce->ac_ipaddr= ipaddr;
	ce->ac_port= arp_port;
	ce->ac_expire= curr_time+ARP_EXP_TIME;
	ce->ac_lastuse= curr_time;

	reqp->ar_entry= ce-arp_cache;
	reqp->ar_req_count= -1;

	/* Send the first packet by expiring the timer */
	clck_timer(&reqp->ar_timer, 1, arp_timeout, ref);

	return NW_SUSPEND;
}

int arp_ioctl (eth_port, fd, req, get_userdata, put_userdata)
int eth_port;
int fd;
ioreq_t req;
get_userdata_t get_userdata;
put_userdata_t put_userdata;
{
	arp_port_t *arp_port;
	arp_cache_t *ce, *cache;
	acc_t *data;
	nwio_arp_t *arp_iop;
	int entno, result, ac_flags;
	u32_t flags;
	ipaddr_t ipaddr;
	time_t curr_time;

	assert(eth_port >= 0 && eth_port < eth_conf_nr);
	arp_port= &arp_port_table[eth_port];
	assert(arp_port->ap_state == APS_ARPMAIN ||
		(printf("arp[%d]: ap_state= %d\n", arp_port-arp_port_table,
		arp_port->ap_state), 0));

	switch(req)
	{
	case NWIOARPGIP:
		data= (*get_userdata)(fd, 0, sizeof(*arp_iop), TRUE);
		if (data == NULL)
			return EFAULT;
		data= bf_packIffLess(data, sizeof(*arp_iop));
		arp_iop= (nwio_arp_t *)ptr2acc_data(data);
		ipaddr= arp_iop->nwa_ipaddr;
		ce= NULL;	/* lint */
		for (entno= 0; entno < arp_cache_nr; entno++)
		{
			ce= &arp_cache[entno];
			if (ce->ac_state == ACS_UNUSED ||
				ce->ac_port != arp_port)
			{
				continue;
			}
			if (ce->ac_ipaddr == ipaddr)
				break;
		}
		if (entno == arp_cache_nr)
		{
			/* Also report the address of this interface */
			if (ipaddr != arp_port->ap_ipaddr)
			{
				bf_afree(data);
				return ENOENT;
			}
			arp_iop->nwa_entno= arp_cache_nr;
			arp_iop->nwa_ipaddr= ipaddr;
			arp_iop->nwa_ethaddr= arp_port->ap_ethaddr;
			arp_iop->nwa_flags= NWAF_PERM | NWAF_PUB;
		}
		else
		{
			arp_iop->nwa_entno= entno+1;
			arp_iop->nwa_ipaddr= ce->ac_ipaddr;
			arp_iop->nwa_ethaddr= ce->ac_ethaddr;
			arp_iop->nwa_flags= 0;
			if (ce->ac_state == ACS_INCOMPLETE)
				arp_iop->nwa_flags |= NWAF_INCOMPLETE;
			if (ce->ac_state == ACS_UNREACHABLE)
				arp_iop->nwa_flags |= NWAF_DEAD;
			if (ce->ac_flags & ACF_PERM)
				arp_iop->nwa_flags |= NWAF_PERM;
			if (ce->ac_flags & ACF_PUB)
				arp_iop->nwa_flags |= NWAF_PUB;
		}

		result= (*put_userdata)(fd, 0, data, TRUE);
		return result;

	case NWIOARPGNEXT:
		data= (*get_userdata)(fd, 0, sizeof(*arp_iop), TRUE);
		if (data == NULL)
			return EFAULT;
		data= bf_packIffLess(data, sizeof(*arp_iop));
		arp_iop= (nwio_arp_t *)ptr2acc_data(data);
		entno= arp_iop->nwa_entno;
		if (entno < 0)
			entno= 0;
		ce= NULL;	/* lint */
		for (; entno < arp_cache_nr; entno++)
		{
			ce= &arp_cache[entno];
			if (ce->ac_state == ACS_UNUSED ||
				ce->ac_port != arp_port)
			{
				continue;
			}
			break;
		}
		if (entno == arp_cache_nr)
		{
			bf_afree(data);
			return ENOENT;
		}
		arp_iop->nwa_entno= entno+1;
		arp_iop->nwa_ipaddr= ce->ac_ipaddr;
		arp_iop->nwa_ethaddr= ce->ac_ethaddr;
		arp_iop->nwa_flags= 0;
		if (ce->ac_state == ACS_INCOMPLETE)
			arp_iop->nwa_flags |= NWAF_INCOMPLETE;
		if (ce->ac_state == ACS_UNREACHABLE)
			arp_iop->nwa_flags |= NWAF_DEAD;
		if (ce->ac_flags & ACF_PERM)
			arp_iop->nwa_flags |= NWAF_PERM;
		if (ce->ac_flags & ACF_PUB)
			arp_iop->nwa_flags |= NWAF_PUB;

		result= (*put_userdata)(fd, 0, data, TRUE);
		return result;

	case NWIOARPSIP:
		data= (*get_userdata)(fd, 0, sizeof(*arp_iop), TRUE);
		if (data == NULL)
			return EFAULT;
		data= bf_packIffLess(data, sizeof(*arp_iop));
		arp_iop= (nwio_arp_t *)ptr2acc_data(data);
		ipaddr= arp_iop->nwa_ipaddr;
		if (find_cache_ent(arp_port, ipaddr))
		{
			bf_afree(data);
			return EEXIST;
		}

		flags= arp_iop->nwa_flags;
		ac_flags= ACF_EMPTY;
		if (flags & NWAF_PERM)
			ac_flags |= ACF_PERM;
		if (flags & NWAF_PUB)
			ac_flags |= ACF_PUB|ACF_PERM;

		/* Allocate a cache entry */
		ce= alloc_cache_ent(ac_flags);
		if (ce == NULL)
		{
			bf_afree(data);
			return ENOMEM;
		}

		ce->ac_flags= ac_flags;
		ce->ac_state= ACS_VALID;
		ce->ac_ethaddr= arp_iop->nwa_ethaddr;
		ce->ac_ipaddr= arp_iop->nwa_ipaddr;
		ce->ac_port= arp_port;

		curr_time= get_time();
		ce->ac_expire= curr_time+ARP_EXP_TIME;
		ce->ac_lastuse= curr_time;

		bf_afree(data);
		return 0;

	case NWIOARPDIP:
		data= (*get_userdata)(fd, 0, sizeof(*arp_iop), TRUE);
		if (data == NULL)
			return EFAULT;
		data= bf_packIffLess(data, sizeof(*arp_iop));
		arp_iop= (nwio_arp_t *)ptr2acc_data(data);
		ipaddr= arp_iop->nwa_ipaddr;
		bf_afree(data); data= NULL;
		ce= find_cache_ent(arp_port, ipaddr);
		if (!ce)
			return ENOENT;
		if (ce->ac_state == ACS_INCOMPLETE)
			return EINVAL;

		ac_flags= ce->ac_flags;
		if (ac_flags & ACF_PUB)
		{
			/* Make sure entry is at the end of published
			 * entries.
			 */
			for (entno= 0, cache= arp_cache;
				entno<arp_cache_nr; entno++, cache++)
			{
				if (cache->ac_state == ACS_UNUSED)
					break;
				if (cache->ac_flags & ACF_PUB)
					continue;
				break;
			}
			assert(cache > arp_cache);
			cache--;
			if (cache != ce)
			{
				assert(cache > ce);
				*ce= *cache;
				ce= cache;
			}
		}
		if (ac_flags & ACF_PERM)
		{
			/* Make sure entry is at the end of permanent
			 * entries.
			 */
			for (entno= 0, cache= arp_cache;
				entno<arp_cache_nr; entno++, cache++)
			{
				if (cache->ac_state == ACS_UNUSED)
					break;
				if (cache->ac_flags & ACF_PERM)
					continue;
				break;
			}
			assert(cache > arp_cache);
			cache--;
			if (cache != ce)
			{
				assert(cache > ce);
				*ce= *cache;
				ce= cache;
			}
		}

		/* Clear entry */
		ce->ac_state= ACS_UNUSED;

		return 0;

	default:
		ip_panic(("arp_ioctl: unknown request 0x%lx",
			(unsigned long)req));
	}
	return 0;
}

static void arp_timeout (ref, timer)
int ref;
timer_t *timer;
{
	int i, port, reqind, acind;
	arp_port_t *arp_port;
	arp_cache_t *ce;
	struct arp_req *reqp;
	time_t curr_time;
	acc_t *data;
	arp46_t *arp;
	u16_t *p;

	port= ref / AP_REQ_NR;
	reqind= ref % AP_REQ_NR;

	assert(port >= 0 && port <eth_conf_nr);
	arp_port= &arp_port_table[port];

	reqp= &arp_port->ap_req[reqind];
	assert (timer == &reqp->ar_timer);

	acind= reqp->ar_entry;

	assert(acind >= 0 && acind < arp_cache_nr);
	ce= &arp_cache[acind];

	assert(ce->ac_port == arp_port);
	assert(ce->ac_state == ACS_INCOMPLETE);

	if (++reqp->ar_req_count >= MAX_ARP_RETRIES)
	{
		curr_time= get_time();
		ce->ac_state= ACS_UNREACHABLE;
		ce->ac_expire= curr_time+ ARP_NOTRCH_EXP_TIME;
		ce->ac_lastuse= curr_time;

		clck_untimer(&reqp->ar_timer);
		reqp->ar_entry= -1;
		client_reply(arp_port, ce->ac_ipaddr, NULL);
		return;
	}

	data= bf_memreq(sizeof(arp46_t));
	arp= (arp46_t *)ptr2acc_data(data);

	/* Clear padding */
	assert(sizeof(arp->a46_data.a46_dummy) % sizeof(*p) == 0);
	for (i= 0, p= (u16_t *)arp->a46_data.a46_dummy;
		i < sizeof(arp->a46_data.a46_dummy)/sizeof(*p);
		i++, p++)
	{
		*p= 0xdead;
	}

	arp->a46_dstaddr.ea_addr[0]= 0xff;
	arp->a46_dstaddr.ea_addr[1]= 0xff;
	arp->a46_dstaddr.ea_addr[2]= 0xff;
	arp->a46_dstaddr.ea_addr[3]= 0xff;
	arp->a46_dstaddr.ea_addr[4]= 0xff;
	arp->a46_dstaddr.ea_addr[5]= 0xff;
	arp->a46_hdr= HTONS(ARP_ETHERNET);
	arp->a46_pro= HTONS(ETH_IP_PROTO);
	arp->a46_hln= 6;
	arp->a46_pln= 4;
	arp->a46_op= HTONS(ARP_REQUEST);
	arp->a46_sha= arp_port->ap_ethaddr;
	memcpy (arp->a46_spa, &arp_port->ap_ipaddr, sizeof(ipaddr_t));
	memset(&arp->a46_tha, '\0', sizeof(ether_addr_t));
	memcpy (arp->a46_tpa, &ce->ac_ipaddr, sizeof(ipaddr_t));

	assert(data->acc_linkC == 1);
	data->acc_ext_link= arp_port->ap_sendlist;
	arp_port->ap_sendlist= data; data= NULL;

	if (!(arp_port->ap_flags & APF_ARP_WR_IP))
		setup_write(arp_port);

	clck_timer(&reqp->ar_timer, get_time() + ARP_TIMEOUT,
		arp_timeout, ref);
}

static void arp_buffree(priority)
int priority;
{
	int i;
	acc_t *pack, *next_pack;
	arp_port_t *arp_port;

	for (i= 0, arp_port= arp_port_table; i<eth_conf_nr; i++, arp_port++)
	{
		if (priority == ARP_PRI_REC)
		{
			next_pack= arp_port->ap_reclist;
			while(next_pack && next_pack->acc_ext_link)
			{
				pack= next_pack;
				next_pack= pack->acc_ext_link;
				bf_afree(pack);
			}
			if (next_pack)
			{
				if (ev_in_queue(&arp_port->ap_event))
				{
					DBLOCK(1, printf(
			"not freeing ap_reclist, ap_event enqueued\n"));
				}
				else
				{
					bf_afree(next_pack);
					next_pack= NULL;
				}
			}
			arp_port->ap_reclist= next_pack;
		}
		if (priority == ARP_PRI_SEND)
		{
			next_pack= arp_port->ap_sendlist;
			while(next_pack && next_pack->acc_ext_link)
			{
				pack= next_pack;
				next_pack= pack->acc_ext_link;
				bf_afree(pack);
			}
			if (next_pack)
			{
				if (ev_in_queue(&arp_port->ap_event))
				{
					DBLOCK(1, printf(
			"not freeing ap_sendlist, ap_event enqueued\n"));
				}
				else
				{
					bf_afree(next_pack);
					next_pack= NULL;
				}
			}
			arp_port->ap_sendlist= next_pack;
		}
	}
}

#ifdef BUF_CONSISTENCY_CHECK
static void arp_bufcheck()
{
	int i;
	arp_port_t *arp_port;
	acc_t *pack;

	for (i= 0, arp_port= arp_port_table; i<eth_conf_nr; i++, arp_port++)
	{
		for (pack= arp_port->ap_reclist; pack;
			pack= pack->acc_ext_link)
		{
			bf_check_acc(pack);
		}
		for (pack= arp_port->ap_sendlist; pack;
			pack= pack->acc_ext_link)
		{
			bf_check_acc(pack);
		}
	}
}
#endif /* BUF_CONSISTENCY_CHECK */

/*
 * $PchId: arp.c,v 1.22 2005/06/28 14:15:06 philip Exp $
 */
