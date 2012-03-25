/*
generic/ip_eth.c

Ethernet specific part of the IP implementation

Created:	Apr 22, 1993 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "type.h"
#include "arp.h"
#include "assert.h"
#include "buf.h"
#include "clock.h"
#include "eth.h"
#include "event.h"
#include "icmp_lib.h"
#include "io.h"
#include "ip.h"
#include "ip_int.h"

THIS_FILE

typedef struct xmit_hdr
{
	time_t xh_time;
	ipaddr_t xh_ipaddr;
} xmit_hdr_t;

static ether_addr_t broadcast_ethaddr=
{
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
};
static ether_addr_t ipmulticast_ethaddr=
{
	{ 0x01, 0x00, 0x5e, 0x00, 0x00, 0x00 }
};

static void do_eth_read ARGS(( ip_port_t *port ));
static acc_t *get_eth_data ARGS(( int fd, size_t offset,
	size_t count, int for_ioctl ));
static int put_eth_data ARGS(( int fd, size_t offset,
	acc_t *data, int for_ioctl ));
static void ipeth_main ARGS(( ip_port_t *port ));
static void ipeth_set_ipaddr ARGS(( ip_port_t *port ));
static void ipeth_restart_send ARGS(( ip_port_t *ip_port ));
static int ipeth_send ARGS(( struct ip_port *ip_port, ipaddr_t dest, 
	acc_t *pack, int type ));
static void ipeth_arp_reply ARGS(( int ip_port_nr, ipaddr_t ipaddr,
	ether_addr_t *dst_ether_ptr ));
static int ipeth_update_ttl ARGS(( time_t enq_time, time_t now,
	acc_t *eth_pack ));
static void ip_eth_arrived ARGS(( int port, acc_t *pack,
	size_t pack_size ));


int ipeth_init(ip_port)
ip_port_t *ip_port;
{
	assert(BUF_S >= sizeof(xmit_hdr_t));
	assert(BUF_S >= sizeof(eth_hdr_t));

	ip_port->ip_dl.dl_eth.de_fd= eth_open(ip_port->
		ip_dl.dl_eth.de_port, ip_port->ip_port,
		get_eth_data, put_eth_data, ip_eth_arrived,
		0 /* no select_res */);
	if (ip_port->ip_dl.dl_eth.de_fd < 0)
	{
		DBLOCK(1, printf("ip.c: unable to open eth port\n"));
		return -1;
	}
	ip_port->ip_dl.dl_eth.de_state= IES_EMPTY;
	ip_port->ip_dl.dl_eth.de_flags= IEF_EMPTY;
	ip_port->ip_dl.dl_eth.de_q_head= NULL;
	ip_port->ip_dl.dl_eth.de_q_tail= NULL;
	ip_port->ip_dl.dl_eth.de_arp_head= NULL;
	ip_port->ip_dl.dl_eth.de_arp_tail= NULL;
	ip_port->ip_dev_main= ipeth_main;
	ip_port->ip_dev_set_ipaddr= ipeth_set_ipaddr;
	ip_port->ip_dev_send= ipeth_send;
	ip_port->ip_mtu= ETH_MAX_PACK_SIZE-ETH_HDR_SIZE;
	ip_port->ip_mtu_max= ip_port->ip_mtu;
	return 0;
}

static void ipeth_main(ip_port)
ip_port_t *ip_port;
{
	int result;

	switch (ip_port->ip_dl.dl_eth.de_state)
	{
	case IES_EMPTY:
		ip_port->ip_dl.dl_eth.de_state= IES_SETPROTO;

		result= eth_ioctl(ip_port->ip_dl.dl_eth.de_fd, NWIOSETHOPT);
		if (result == NW_SUSPEND)
			ip_port->ip_dl.dl_eth.de_flags |= IEF_SUSPEND;
		if (result<0)
		{
			DBLOCK(1, printf("eth_ioctl(..,0x%lx)=%d\n",
				(unsigned long)NWIOSETHOPT, result));
			return;
		}
		if (ip_port->ip_dl.dl_eth.de_state != IES_SETPROTO)
			return;
		/* drops through */
	case IES_SETPROTO:
		result= arp_set_cb(ip_port->ip_dl.dl_eth.de_port,
			ip_port->ip_port,
			ipeth_arp_reply);
		if (result != NW_OK)
		{
			printf("ipeth_main: arp_set_cb failed: %d\n",
				result);
			return;
		}

		/* Wait until the interface is configured up. */
		ip_port->ip_dl.dl_eth.de_state= IES_GETIPADDR;
		if (!(ip_port->ip_flags & IPF_IPADDRSET))
		{
			ip_port->ip_dl.dl_eth.de_flags |= IEF_SUSPEND;
			return;
		}

		/* fall through */
	case IES_GETIPADDR:
		ip_port->ip_dl.dl_eth.de_state= IES_MAIN;
		do_eth_read(ip_port);
		return;
	default:
		ip_panic(( "unknown state: %d",
			ip_port->ip_dl.dl_eth.de_state));
	}
}

static acc_t *get_eth_data (fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	ip_port_t *ip_port;
	acc_t *data;
	int result;

	ip_port= &ip_port_table[fd];

	switch (ip_port->ip_dl.dl_eth.de_state)
	{
	case IES_SETPROTO:
		if (!count)
		{
			result= (int)offset;
			if (result<0)
			{
				ip_port->ip_dl.dl_eth.de_state= IES_ERROR;
				break;
			}
			if (ip_port->ip_dl.dl_eth.de_flags & IEF_SUSPEND)
				ipeth_main(ip_port);
			return NW_OK;
		}
		assert ((!offset) && (count == sizeof(struct nwio_ethopt)));
		{
			struct nwio_ethopt *ethopt;
			acc_t *acc;

			acc= bf_memreq(sizeof(*ethopt));
			ethopt= (struct nwio_ethopt *)ptr2acc_data(acc);
			ethopt->nweo_flags= NWEO_COPY|NWEO_EN_BROAD|
				NWEO_EN_MULTI|NWEO_TYPESPEC;
			ethopt->nweo_type= HTONS(ETH_IP_PROTO);
			return acc;
		}

	case IES_MAIN:
		if (!count)
		{
			result= (int)offset;
			if (result<0)
				ip_warning(( "error on write: %d\n", result ));
			bf_afree (ip_port->ip_dl.dl_eth.de_frame);
			ip_port->ip_dl.dl_eth.de_frame= 0;

			if (ip_port->ip_dl.dl_eth.de_flags & IEF_WRITE_SP)
			{
				ip_port->ip_dl.dl_eth.de_flags &=
					~IEF_WRITE_SP;
				ipeth_restart_send(ip_port);
			}
			return NW_OK;
		}
		data= bf_cut (ip_port->ip_dl.dl_eth.de_frame, offset, count);
		assert (data);
		return data;
	default:
		printf(
		"get_eth_data(%d, 0x%d, 0x%d) called but ip_state=0x%x\n",
			fd, offset, count, ip_port->ip_dl.dl_eth.de_state);
		break;
	}
	return 0;
}

static int put_eth_data (port, offset, data, for_ioctl)
int port;
size_t offset;
acc_t *data;
int for_ioctl;
{
	ip_port_t *ip_port;
	int result;

	ip_port= &ip_port_table[port];

	assert(0);

	if (ip_port->ip_dl.dl_eth.de_flags & IEF_READ_IP)
	{
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				DBLOCK(1, printf(
				"ip.c: put_eth_data(..,%d,..)\n", result));
				return NW_OK;
			}
			if (ip_port->ip_dl.dl_eth.de_flags & IEF_READ_SP)
			{
				ip_port->ip_dl.dl_eth.de_flags &= 
						~(IEF_READ_IP|IEF_READ_SP);
				do_eth_read(ip_port);
			}
			else
				ip_port->ip_dl.dl_eth.de_flags &= ~IEF_READ_IP;
			return NW_OK;
		}
		assert (!offset);
		/* Warning: the above assertion is illegal; puts and
		   gets of data can be brokenup in any piece the server
		   likes. However we assume that the server is eth.c
		   and it transfers only whole packets. */
		ip_eth_arrived(port, data, bf_bufsize(data));
		return NW_OK;
	}
	printf("ip_port->ip_dl.dl_eth.de_state= 0x%x",
		ip_port->ip_dl.dl_eth.de_state);
	ip_panic (( "strange status" ));
	return -1;
}

static void ipeth_set_ipaddr(ip_port)
ip_port_t *ip_port;
{
	arp_set_ipaddr (ip_port->ip_dl.dl_eth.de_port, ip_port->ip_ipaddr);
	if (ip_port->ip_dl.dl_eth.de_state == IES_GETIPADDR)
		ipeth_main(ip_port);
}

static int ipeth_send(ip_port, dest, pack, type)
struct ip_port *ip_port;
ipaddr_t dest;
acc_t *pack;
int type;
{
	int i, r;
	acc_t *eth_pack, *tail;
	size_t pack_size;
	eth_hdr_t *eth_hdr;
	xmit_hdr_t *xmit_hdr;
	ipaddr_t tmpaddr;
	time_t t;
	u32_t *p;

	/* Start optimistic: the arp will succeed without blocking and the
	 * ethernet packet can be sent without blocking also. Start with
	 * the allocation of the ethernet header.
	 */
	eth_pack= bf_memreq(sizeof(*eth_hdr));
	assert(eth_pack->acc_next == NULL);
	eth_pack->acc_next= pack;
	pack_size= bf_bufsize(eth_pack);
	if (pack_size<ETH_MIN_PACK_SIZE)
	{
		tail= bf_memreq(ETH_MIN_PACK_SIZE-pack_size);

		/* Clear padding */
		for (i= (ETH_MIN_PACK_SIZE-pack_size)/sizeof(*p),
			p= (u32_t *)ptr2acc_data(tail);
			i >= 0; i--, p++)
		{
			*p= 0xdeadbeef;
		}

		eth_pack= bf_append(eth_pack, tail);
	}
	eth_hdr= (eth_hdr_t *)ptr2acc_data(eth_pack);

	/* Lookup the ethernet address */
	if (type != IP_LT_NORMAL)
	{
		if (type == IP_LT_BROADCAST)
			eth_hdr->eh_dst= broadcast_ethaddr;
		else
		{
			tmpaddr= ntohl(dest);
			eth_hdr->eh_dst= ipmulticast_ethaddr;
			eth_hdr->eh_dst.ea_addr[5]= tmpaddr & 0xff;
			eth_hdr->eh_dst.ea_addr[4]= (tmpaddr >> 8) & 0xff;
			eth_hdr->eh_dst.ea_addr[3]= (tmpaddr >> 16) & 0x7f;
		}
	}
	else
	{
		if ((dest ^ ip_port->ip_ipaddr) & ip_port->ip_subnetmask)
		{
			ip_panic(( "invalid destination" ));
		}

		assert(dest != ip_port->ip_ipaddr);

		r= arp_ip_eth(ip_port->ip_dl.dl_eth.de_port,
			dest, &eth_hdr->eh_dst);
		if (r == NW_SUSPEND)
		{
			/* Unfortunately, the arp takes some time, use
			 * the ethernet header to store the next hop
			 * ip address and the current time.
			 */
			xmit_hdr= (xmit_hdr_t *)eth_hdr;
			xmit_hdr->xh_time= get_time();
			xmit_hdr->xh_ipaddr= dest;
			eth_pack->acc_ext_link= NULL;
			if (ip_port->ip_dl.dl_eth.de_arp_head == NULL)
				ip_port->ip_dl.dl_eth.de_arp_head= eth_pack;
			else
			{
				ip_port->ip_dl.dl_eth.de_arp_tail->
					acc_ext_link= eth_pack;
			}
			ip_port->ip_dl.dl_eth.de_arp_tail= eth_pack;
			return NW_OK;
		}
		if (r == EHOSTUNREACH)
		{
			bf_afree(eth_pack);
			return r;
		}
		assert(r == NW_OK);
	}

	/* If we have no write in progress, we can try to send the ethernet
	 * packet using eth_send. If the IP packet is larger than mtu,
	 * enqueue the packet and let ipeth_restart_send deal with it. 
	 */
	pack_size= bf_bufsize(eth_pack);
	if (ip_port->ip_dl.dl_eth.de_frame == NULL && pack_size <=
		ip_port->ip_mtu + sizeof(*eth_hdr))
	{
		r= eth_send(ip_port->ip_dl.dl_eth.de_fd,
			eth_pack, pack_size);
		if (r == NW_OK)
			return NW_OK;

		/* A non-blocking send is not possible, start a regular
		 * send.
		 */
		assert(r == NW_WOULDBLOCK);
		ip_port->ip_dl.dl_eth.de_frame= eth_pack;
		r= eth_write(ip_port->ip_dl.dl_eth.de_fd, pack_size);
		if (r == NW_SUSPEND)
		{
			assert(!(ip_port->ip_dl.dl_eth.de_flags &
				IEF_WRITE_SP));
			ip_port->ip_dl.dl_eth.de_flags |= IEF_WRITE_SP;
		}
		assert(r == NW_OK || r == NW_SUSPEND);
		return NW_OK;
	}

	/* Enqueue the packet, and store the current time, in the
	 * space for the ethernet source address.
	 */
	t= get_time();
	assert(sizeof(t) <= sizeof(eth_hdr->eh_src));
	memcpy(&eth_hdr->eh_src, &t, sizeof(t));

	eth_pack->acc_ext_link= NULL;
	if (ip_port->ip_dl.dl_eth.de_q_head == NULL)
		ip_port->ip_dl.dl_eth.de_q_head= eth_pack;
	else
	{
		ip_port->ip_dl.dl_eth.de_q_tail->acc_ext_link= eth_pack;
	}
	ip_port->ip_dl.dl_eth.de_q_tail= eth_pack;
	if (ip_port->ip_dl.dl_eth.de_frame == NULL)
		ipeth_restart_send(ip_port);
	return NW_OK;
}

static void ipeth_restart_send(ip_port)
ip_port_t *ip_port;
{
	time_t now, enq_time;
	int i, r;
	acc_t *eth_pack, *ip_pack, *next_eth_pack, *next_part, *tail;
	size_t pack_size;
	eth_hdr_t *eth_hdr, *next_eth_hdr;
	u32_t *p;

	now= get_time();

	while (ip_port->ip_dl.dl_eth.de_q_head != NULL)
	{
		eth_pack= ip_port->ip_dl.dl_eth.de_q_head;
		ip_port->ip_dl.dl_eth.de_q_head= eth_pack->acc_ext_link;

		eth_hdr= (eth_hdr_t *)ptr2acc_data(eth_pack);

		pack_size= bf_bufsize(eth_pack);

		if (pack_size > ip_port->ip_mtu+sizeof(*eth_hdr))
		{
			/* Split the IP packet */
			assert(eth_pack->acc_linkC == 1);
			ip_pack= eth_pack->acc_next; eth_pack->acc_next= NULL;
			next_part= ip_pack; ip_pack= NULL;
			ip_pack= ip_split_pack(ip_port, &next_part, 
							ip_port->ip_mtu);
			if (ip_pack == NULL)
			{
				bf_afree(eth_pack);
				continue;
			}

			eth_pack->acc_next= ip_pack; ip_pack= NULL;

			/* Allocate new ethernet header */
			next_eth_pack= bf_memreq(sizeof(*next_eth_hdr));
			next_eth_hdr= (eth_hdr_t *)ptr2acc_data(next_eth_pack);
			*next_eth_hdr= *eth_hdr;
			next_eth_pack->acc_next= next_part;

			next_eth_pack->acc_ext_link= NULL;
			if (ip_port->ip_dl.dl_eth.de_q_head == NULL)
				ip_port->ip_dl.dl_eth.de_q_head= next_eth_pack;
			else
			{
				ip_port->ip_dl.dl_eth.de_q_tail->acc_ext_link= 
								next_eth_pack;
			}
			ip_port->ip_dl.dl_eth.de_q_tail= next_eth_pack;

			pack_size= bf_bufsize(eth_pack);
		}

		memcpy(&enq_time, &eth_hdr->eh_src, sizeof(enq_time));
		if (enq_time + HZ < now)
		{
			r= ipeth_update_ttl(enq_time, now, eth_pack);
			if (r == ETIMEDOUT)
			{	
				ip_pack= bf_delhead(eth_pack, sizeof(*eth_hdr));
				eth_pack= NULL;
				icmp_snd_time_exceeded(ip_port->ip_port,
					ip_pack, ICMP_TTL_EXC);
				continue;
			}
			assert(r == NW_OK);
		}

		if (pack_size<ETH_MIN_PACK_SIZE)
		{
			tail= bf_memreq(ETH_MIN_PACK_SIZE-pack_size);

			/* Clear padding */
			for (i= (ETH_MIN_PACK_SIZE-pack_size)/sizeof(*p),
				p= (u32_t *)ptr2acc_data(tail);
				i >= 0; i--, p++)
			{
				*p= 0xdeadbeef;
			}

			eth_pack= bf_append(eth_pack, tail);
			pack_size= ETH_MIN_PACK_SIZE;
		}

		assert(ip_port->ip_dl.dl_eth.de_frame == NULL);

		r= eth_send(ip_port->ip_dl.dl_eth.de_fd, eth_pack, pack_size);
		if (r == NW_OK)
			continue;

		/* A non-blocking send is not possible, start a regular
		 * send.
		 */
		assert(r == NW_WOULDBLOCK);
		ip_port->ip_dl.dl_eth.de_frame= eth_pack;
		r= eth_write(ip_port->ip_dl.dl_eth.de_fd, pack_size);
		if (r == NW_SUSPEND)
		{
			assert(!(ip_port->ip_dl.dl_eth.de_flags &
				IEF_WRITE_SP));
			ip_port->ip_dl.dl_eth.de_flags |= IEF_WRITE_SP;
			return;
		}
		assert(r == NW_OK);
	}
}


static void ipeth_arp_reply(ip_port_nr, ipaddr, eth_addr)
int ip_port_nr;
ipaddr_t ipaddr;
ether_addr_t *eth_addr;
{
	acc_t *prev, *eth_pack;
	int r;
	xmit_hdr_t *xmit_hdr;
	ip_port_t *ip_port;
	time_t t;
	eth_hdr_t *eth_hdr;
	ether_addr_t tmp_eth_addr;

	assert (ip_port_nr >= 0 && ip_port_nr < ip_conf_nr);
	ip_port= &ip_port_table[ip_port_nr];

	for (;;)
	{
		for (prev= 0, eth_pack= ip_port->ip_dl.dl_eth.de_arp_head;
			eth_pack;
			prev= eth_pack, eth_pack= eth_pack->acc_ext_link)
		{
			xmit_hdr= (xmit_hdr_t *)ptr2acc_data(eth_pack);
			if (xmit_hdr->xh_ipaddr == ipaddr)
				break;
		}

		if (eth_pack == NULL)
		{
			/* No packet found. */
			break;
		}

		/* Delete packet from the queue. */
		if (prev == NULL)
		{
			ip_port->ip_dl.dl_eth.de_arp_head=
				eth_pack->acc_ext_link;
		}
		else
		{
			prev->acc_ext_link= eth_pack->acc_ext_link;
			if (prev->acc_ext_link == NULL)
				ip_port->ip_dl.dl_eth.de_arp_tail= prev;
		}

		if (eth_addr == NULL)
		{
			/* Destination is unreachable, delete packet. */
			bf_afree(eth_pack);
			continue;
		}

		/* Fill in the ethernet address and put the packet on the 
		 * transmit queue.
		 */
		t= xmit_hdr->xh_time;
		eth_hdr= (eth_hdr_t *)ptr2acc_data(eth_pack);
		eth_hdr->eh_dst= *eth_addr;
		memcpy(&eth_hdr->eh_src, &t, sizeof(t));

		eth_pack->acc_ext_link= NULL;
		if (ip_port->ip_dl.dl_eth.de_q_head == NULL)
			ip_port->ip_dl.dl_eth.de_q_head= eth_pack;
		else
		{
			ip_port->ip_dl.dl_eth.de_q_tail->acc_ext_link=
				eth_pack;
		}
		ip_port->ip_dl.dl_eth.de_q_tail= eth_pack;
	}

	/* Try to get some more ARPs in progress. */
	while (ip_port->ip_dl.dl_eth.de_arp_head)
	{
		eth_pack= ip_port->ip_dl.dl_eth.de_arp_head;
		xmit_hdr= (xmit_hdr_t *)ptr2acc_data(eth_pack);
		r= arp_ip_eth(ip_port->ip_dl.dl_eth.de_port,
			xmit_hdr->xh_ipaddr, &tmp_eth_addr);
		if (r == NW_SUSPEND)
			break;				/* Normal case */

		/* Dequeue the packet */
		ip_port->ip_dl.dl_eth.de_arp_head= eth_pack->acc_ext_link;

		if (r == EHOSTUNREACH)
		{
			bf_afree(eth_pack);
			continue;
		}
		assert(r == NW_OK);

		/* Fill in the ethernet address and put the packet on the 
		 * transmit queue.
		 */
		t= xmit_hdr->xh_time;
		eth_hdr= (eth_hdr_t *)ptr2acc_data(eth_pack);
		eth_hdr->eh_dst= tmp_eth_addr;
		memcpy(&eth_hdr->eh_src, &t, sizeof(t));

		eth_pack->acc_ext_link= NULL;
		if (ip_port->ip_dl.dl_eth.de_q_head == NULL)
			ip_port->ip_dl.dl_eth.de_q_head= eth_pack;
		else
		{
			ip_port->ip_dl.dl_eth.de_q_tail->acc_ext_link=
				eth_pack;
		}
		ip_port->ip_dl.dl_eth.de_q_tail= eth_pack;
	}

	/* Restart sending ethernet packets. */
	if (ip_port->ip_dl.dl_eth.de_frame == NULL)
		ipeth_restart_send(ip_port);
}

static int ipeth_update_ttl(enq_time, now, eth_pack)
time_t enq_time;
time_t now;
acc_t *eth_pack;
{
	int ttl_diff;
	ip_hdr_t *ip_hdr;
	u32_t sum;
	u16_t word;
	acc_t *ip_pack;

	ttl_diff= (now-enq_time)/HZ;
	enq_time += ttl_diff*HZ;
	assert(enq_time <= now && enq_time + HZ > now);

	ip_pack= eth_pack->acc_next;
	assert(ip_pack->acc_length >= sizeof(*ip_hdr));
	assert(ip_pack->acc_linkC == 1 &&
		ip_pack->acc_buffer->buf_linkC == 1);

	ip_hdr= (ip_hdr_t *)ptr2acc_data(ip_pack);
	if (ip_hdr->ih_ttl <= ttl_diff)
		return ETIMEDOUT;
	sum= (u16_t)~ip_hdr->ih_hdr_chk;
	word= *(u16_t *)&ip_hdr->ih_ttl;
	if (word > sum)
		sum += 0xffff - word;
	else
		sum -= word;
	ip_hdr->ih_ttl -= ttl_diff;
	word= *(u16_t *)&ip_hdr->ih_ttl;
	sum += word;
	if (sum > 0xffff)
		sum -= 0xffff;
	assert(!(sum & 0xffff0000));
	ip_hdr->ih_hdr_chk= ~sum;
	assert(ip_hdr->ih_ttl > 0);
	return NW_OK;
}

static void do_eth_read(ip_port)
ip_port_t *ip_port;
{
	int result;

	assert(!(ip_port->ip_dl.dl_eth.de_flags & IEF_READ_IP));

	for (;;)
	{
		ip_port->ip_dl.dl_eth.de_flags |= IEF_READ_IP;

		result= eth_read (ip_port->ip_dl.dl_eth.de_fd,
			ETH_MAX_PACK_SIZE);
		if (result == NW_SUSPEND)
		{
			assert(!(ip_port->ip_dl.dl_eth.de_flags & 
								IEF_READ_SP));
			ip_port->ip_dl.dl_eth.de_flags |= IEF_READ_SP;
			return;
		}
		ip_port->ip_dl.dl_eth.de_flags &= ~IEF_READ_IP;
		if (result<0)
		{
			return;
		}
	}
}

static void ip_eth_arrived(port, pack, pack_size)
int port;
acc_t *pack;
size_t pack_size;
{
	int broadcast;
	ip_port_t *ip_port;

	ip_port= &ip_port_table[port];
	broadcast= (*(u8_t *)ptr2acc_data(pack) & 1);

	pack= bf_delhead(pack, ETH_HDR_SIZE);

	if (broadcast)
		ip_arrived_broadcast(ip_port, pack);
	else
		ip_arrived(ip_port, pack);
}

/*
 * $PchId: ip_eth.c,v 1.25 2005/06/28 14:18:10 philip Exp $
 */
