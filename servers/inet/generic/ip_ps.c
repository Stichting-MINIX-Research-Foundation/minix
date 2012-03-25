/*
generic/ip_ps.c

pseudo IP specific part of the IP implementation

Created:	Apr 23, 1993 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "assert.h"
#include "type.h"
#include "buf.h"
#include "event.h"
#include "ip.h"
#include "ip_int.h"
#include "psip.h"

THIS_FILE

static void ipps_main ARGS(( ip_port_t *ip_port ));
static void ipps_set_ipaddr ARGS(( ip_port_t *ip_port ));
static int ipps_send ARGS(( struct ip_port *ip_port, ipaddr_t dest, 
					acc_t *pack, int type ));

int ipps_init(ip_port)
ip_port_t *ip_port;
{
	int result;

	result= psip_enable(ip_port->ip_dl.dl_ps.ps_port, ip_port->ip_port);
	if (result == -1)
		return -1;
	ip_port->ip_dl.dl_ps.ps_send_head= NULL;
	ip_port->ip_dl.dl_ps.ps_send_tail= NULL;
	ip_port->ip_dev_main= ipps_main;
	ip_port->ip_dev_set_ipaddr= ipps_set_ipaddr;
	ip_port->ip_dev_send= ipps_send;
	return result;
}

void ipps_get(ip_port_nr)
int ip_port_nr;
{
	int result;
	ipaddr_t dest;
	acc_t *acc, *pack, *next_part;
	ip_port_t *ip_port;

	assert(ip_port_nr >= 0 && ip_port_nr < ip_conf_nr);
	ip_port= &ip_port_table[ip_port_nr];
	assert(ip_port->ip_dl_type == IPDL_PSIP);

	while (ip_port->ip_dl.dl_ps.ps_send_head != NULL)
	{
		pack= ip_port->ip_dl.dl_ps.ps_send_head;
		ip_port->ip_dl.dl_ps.ps_send_head= pack->acc_ext_link;

		/* Extract nexthop address */
		pack= bf_packIffLess(pack, sizeof(dest));
		dest= *(ipaddr_t *)ptr2acc_data(pack);
		pack= bf_delhead(pack, sizeof(dest));

		if (bf_bufsize(pack) > ip_port->ip_mtu)
		{
			next_part= pack;
			pack= ip_split_pack(ip_port, &next_part, 
				ip_port->ip_mtu);
			if (pack == NULL)
				continue;

			/* Prepend nexthop address */
			acc= bf_memreq(sizeof(dest));
			*(ipaddr_t *)(ptr2acc_data(acc))= dest;
			acc->acc_next= next_part;
			next_part= acc; acc= NULL;

			assert(next_part->acc_linkC == 1);
			next_part->acc_ext_link= NULL;
			if (ip_port->ip_dl.dl_ps.ps_send_head)
			{
				ip_port->ip_dl.dl_ps.ps_send_tail->
					acc_ext_link= next_part;
			}
			else
			{
				ip_port->ip_dl.dl_ps.ps_send_head=
					next_part;
			}
			ip_port->ip_dl.dl_ps.ps_send_tail= next_part;
		}

		result= psip_send(ip_port->ip_dl.dl_ps.ps_port, dest, pack);
		if (result != NW_SUSPEND)
		{
			assert(result == NW_OK);
			continue;
		}

		/* Prepend nexthop address */
		acc= bf_memreq(sizeof(dest));
		*(ipaddr_t *)(ptr2acc_data(acc))= dest;
		acc->acc_next= pack;
		pack= acc; acc= NULL;

		pack->acc_ext_link= ip_port->ip_dl.dl_ps.ps_send_head;
		ip_port->ip_dl.dl_ps.ps_send_head= pack;
		if (pack->acc_ext_link == NULL)
			ip_port->ip_dl.dl_ps.ps_send_tail= pack;
		break;
	}
}

void ipps_put(ip_port_nr, nexthop, pack)
int ip_port_nr;
ipaddr_t nexthop;
acc_t *pack;
{
	ip_port_t *ip_port;

	assert(ip_port_nr >= 0 && ip_port_nr < ip_conf_nr);
	ip_port= &ip_port_table[ip_port_nr];
	assert(ip_port->ip_dl_type == IPDL_PSIP);
	if (nexthop == HTONL(0xffffffff))
		ip_arrived_broadcast(ip_port, pack);
	else
		ip_arrived(ip_port, pack);
}

static void ipps_main(ip_port)
ip_port_t *ip_port;
{
	/* nothing to do */
}

static void ipps_set_ipaddr(ip_port)
ip_port_t *ip_port;
{
}

static int ipps_send(ip_port, dest, pack, type)
struct ip_port *ip_port;
ipaddr_t dest;
acc_t *pack;
int type;
{
	int result;
	acc_t *acc, *next_part;

	if (type != IP_LT_NORMAL)
	{
		ip_arrived_broadcast(ip_port, bf_dupacc(pack));

		/* Map all broadcasts to the on-link broadcast address.
		 * This saves the application from having to to find out
		 * if the destination is a subnet broadcast.
		 */
		dest= HTONL(0xffffffff);
	}

	/* Note that allocating a packet may trigger a cleanup action,
	 * which may cause the send queue to become empty.
	 */
	while (ip_port->ip_dl.dl_ps.ps_send_head != NULL)
	{
		acc= bf_memreq(sizeof(dest));

		if (ip_port->ip_dl.dl_ps.ps_send_head == NULL)
		{
			bf_afree(acc); acc= NULL;
			continue;
		}

		/* Prepend nexthop address */
		*(ipaddr_t *)(ptr2acc_data(acc))= dest;
		acc->acc_next= pack;
		pack= acc; acc= NULL;

		assert(pack->acc_linkC == 1);
		pack->acc_ext_link= NULL;

		ip_port->ip_dl.dl_ps.ps_send_tail->acc_ext_link= pack;
		ip_port->ip_dl.dl_ps.ps_send_tail= pack;

		return NW_OK;
	}

	while (pack)
	{
		if (bf_bufsize(pack) > ip_port->ip_mtu)
		{
			next_part= pack;
			pack= ip_split_pack(ip_port, &next_part, 
				ip_port->ip_mtu);
			if (pack == NULL)
			{
				return NW_OK;
			}

			/* Prepend nexthop address */
			acc= bf_memreq(sizeof(dest));
			*(ipaddr_t *)(ptr2acc_data(acc))= dest;
			acc->acc_next= next_part;
			next_part= acc; acc= NULL;

			assert(next_part->acc_linkC == 1);
			next_part->acc_ext_link= NULL;
			ip_port->ip_dl.dl_ps.ps_send_head= next_part;
			ip_port->ip_dl.dl_ps.ps_send_tail= next_part;
		}
		result= psip_send(ip_port->ip_dl.dl_ps.ps_port, dest, pack);
		if (result == NW_SUSPEND)
		{
			/* Prepend nexthop address */
			acc= bf_memreq(sizeof(dest));
			*(ipaddr_t *)(ptr2acc_data(acc))= dest;
			acc->acc_next= pack;
			pack= acc; acc= NULL;

			assert(pack->acc_linkC == 1);
			pack->acc_ext_link= ip_port->ip_dl.dl_ps.ps_send_head;
			ip_port->ip_dl.dl_ps.ps_send_head= pack;
			if (!pack->acc_ext_link)
				ip_port->ip_dl.dl_ps.ps_send_tail= pack;
			break;
		}
		assert(result == NW_OK);
		pack= ip_port->ip_dl.dl_ps.ps_send_head;
		if (!pack)
			break;
		ip_port->ip_dl.dl_ps.ps_send_head= pack->acc_ext_link;

		/* Extract nexthop address */
		pack= bf_packIffLess(pack, sizeof(dest));
		dest= *(ipaddr_t *)ptr2acc_data(pack);
		pack= bf_delhead(pack, sizeof(dest));
	}

	return NW_OK;
}

#if 0
int ipps_check(ip_port_t *ip_port)
{
	int n, bad;
	acc_t *prev, *curr;

	for (n= 0, prev= NULL, curr= ip_port->ip_dl.dl_ps.ps_send_head_;
		curr; prev= curr, curr= curr->acc_ext_link)
	{
		n++;
	}
	bad= 0;
	if (prev != NULL && prev != ip_port->ip_dl.dl_ps.ps_send_tail_)
	{
		printf("ipps_check, ip[%d]: wrong tail: got %p, expected %p\n",
			ip_port-ip_port_table,
			ip_port->ip_dl.dl_ps.ps_send_tail_, prev);
		bad++;
	}
	if (n != ip_port->ip_dl.dl_ps.ps_send_nr)
	{
		printf("ipps_check, ip[%d]: wrong count: got %d, expected %d\n",
			ip_port-ip_port_table,
			ip_port->ip_dl.dl_ps.ps_send_nr, n);
		bad++;
	}
	return bad == 0;
}
#endif

/*
 * $PchId: ip_ps.c,v 1.15 2003/01/21 15:57:52 philip Exp $
 */
