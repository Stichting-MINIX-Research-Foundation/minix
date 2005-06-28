/*
ip.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "event.h"
#include "type.h"

#include "arp.h"
#include "assert.h"
#include "clock.h"
#include "eth.h"
#include "icmp.h"
#include "icmp_lib.h"
#include "io.h"
#include "ip.h"
#include "ip_int.h"
#include "ipr.h"
#include "sr.h"

THIS_FILE

FORWARD void ip_close ARGS(( int fd ));
FORWARD int ip_cancel ARGS(( int fd, int which_operation ));
FORWARD int ip_select ARGS(( int fd, unsigned operations ));

FORWARD void ip_buffree ARGS(( int priority ));
#ifdef BUF_CONSISTENCY_CHECK
FORWARD void ip_bufcheck ARGS(( void ));
#endif
FORWARD void ip_bad_callback ARGS(( struct ip_port *ip_port ));

PUBLIC ip_port_t *ip_port_table;
PUBLIC ip_fd_t ip_fd_table[IP_FD_NR];
PUBLIC ip_ass_t ip_ass_table[IP_ASS_NR];

PUBLIC void ip_prep()
{
	ip_port_table= alloc(ip_conf_nr * sizeof(ip_port_table[0]));
	icmp_prep();
}

PUBLIC void ip_init()
{
	int i, j, result;
	ip_ass_t *ip_ass;
	ip_fd_t *ip_fd;
	ip_port_t *ip_port;
	struct ip_conf *icp;

	assert (BUF_S >= sizeof(struct nwio_ethopt));
	assert (BUF_S >= IP_MAX_HDR_SIZE + ETH_HDR_SIZE);
	assert (BUF_S >= sizeof(nwio_ipopt_t));
	assert (BUF_S >= sizeof(nwio_route_t));

	for (i=0, ip_ass= ip_ass_table; i<IP_ASS_NR; i++, ip_ass++)
	{
		ip_ass->ia_frags= 0;
		ip_ass->ia_first_time= 0;
		ip_ass->ia_port= 0;
	}

	for (i=0, ip_fd= ip_fd_table; i<IP_FD_NR; i++, ip_fd++)
	{
		ip_fd->if_flags= IFF_EMPTY;
		ip_fd->if_rdbuf_head= 0;
	}

	for (i=0, ip_port= ip_port_table, icp= ip_conf;
		i<ip_conf_nr; i++, ip_port++, icp++)
	{
		ip_port->ip_port= i;
		ip_port->ip_flags= IPF_EMPTY;
		ip_port->ip_dev_main= (ip_dev_t)ip_bad_callback;
		ip_port->ip_dev_set_ipaddr= (ip_dev_t)ip_bad_callback;
		ip_port->ip_dev_send= (ip_dev_send_t)ip_bad_callback;
		ip_port->ip_dl_type= icp->ic_devtype;
		ip_port->ip_mtu= IP_DEF_MTU;
		ip_port->ip_mtu_max= IP_MAX_PACKSIZE;

		switch(ip_port->ip_dl_type)
		{
		case IPDL_ETH:
			ip_port->ip_dl.dl_eth.de_port= icp->ic_port;
			result= ipeth_init(ip_port);
			if (result == -1)
				continue;
			assert(result == NW_OK);
			break;
		case IPDL_PSIP:
			ip_port->ip_dl.dl_ps.ps_port= icp->ic_port;
			result= ipps_init(ip_port);
			if (result == -1)
				continue;
			assert(result == NW_OK);
			break;
		default:
			ip_panic(( "unknown ip_dl_type %d", 
							ip_port->ip_dl_type ));
			break;
		}
		ip_port->ip_loopb_head= NULL;
		ip_port->ip_loopb_tail= NULL;
		ev_init(&ip_port->ip_loopb_event);
		ip_port->ip_routeq_head= NULL;
		ip_port->ip_routeq_tail= NULL;
		ev_init(&ip_port->ip_routeq_event);
		ip_port->ip_flags |= IPF_CONFIGURED;
		ip_port->ip_proto_any= NULL;
		for (j= 0; j<IP_PROTO_HASH_NR; j++)
			ip_port->ip_proto[j]= NULL;
	}

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(ip_buffree);
#else
	bf_logon(ip_buffree, ip_bufcheck);
#endif

	icmp_init();
	ipr_init();

	for (i=0, ip_port= ip_port_table; i<ip_conf_nr; i++, ip_port++)
	{
		if (!(ip_port->ip_flags & IPF_CONFIGURED))
			continue;
		ip_port->ip_frame_id= (u16_t)get_time();

		sr_add_minor(if2minor(ip_conf[i].ic_ifno, IP_DEV_OFF),
			i, ip_open, ip_close, ip_read,
			ip_write, ip_ioctl, ip_cancel, ip_select);

		(*ip_port->ip_dev_main)(ip_port);
	}
}

PRIVATE int ip_cancel (fd, which_operation)
int fd;
int which_operation;
{
	ip_fd_t *ip_fd;
	acc_t *repl_res;
	int result;

	ip_fd= &ip_fd_table[fd];

	switch (which_operation)
	{
	case SR_CANCEL_IOCTL:
		assert (ip_fd->if_flags & IFF_IOCTL_IP);
		ip_fd->if_flags &= ~IFF_IOCTL_IP;
		repl_res= (*ip_fd->if_get_userdata)(ip_fd->if_srfd, 
			(size_t)EINTR, (size_t)0, TRUE);
		assert (!repl_res);
		break;
	case SR_CANCEL_READ:
		assert (ip_fd->if_flags & IFF_READ_IP);
		ip_fd->if_flags &= ~IFF_READ_IP;
		result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 
			(size_t)EINTR, (acc_t *)0, FALSE);
		assert (!result);
		break;
#if 0
	case SR_CANCEL_WRITE:
		assert(0);
		assert (ip_fd->if_flags & IFF_WRITE_MASK);
		ip_fd->if_flags &= ~IFF_WRITE_MASK;
		repl_res= (*ip_fd->if_get_userdata)(ip_fd->if_srfd, 
			(size_t)EINTR, (size_t)0, FALSE);
		assert (!repl_res);
		break;
#endif
	default:
		ip_panic(( "unknown cancel request" ));
		break;
	}
	return NW_OK;
}

PRIVATE int ip_select(fd, operations)
int fd;
unsigned operations;
{
	printf("ip_select: not implemented\n");
	return 0;
}

PUBLIC int ip_open (port, srfd, get_userdata, put_userdata, put_pkt,
	select_res)
int port;
int srfd;
get_userdata_t get_userdata;
put_userdata_t put_userdata;
put_pkt_t put_pkt;
select_res_t select_res;
{
	int i;
	ip_fd_t *ip_fd;
	ip_port_t *ip_port;

	ip_port= &ip_port_table[port];
	if (!(ip_port->ip_flags & IPF_CONFIGURED))
		return ENXIO;

	for (i=0; i<IP_FD_NR && (ip_fd_table[i].if_flags & IFF_INUSE);
		i++);

	if (i>=IP_FD_NR)
	{
		DBLOCK(1, printf("out of fds\n"));
		return EAGAIN;
	}

	ip_fd= &ip_fd_table[i];

	ip_fd->if_flags= IFF_INUSE;

	ip_fd->if_ipopt.nwio_flags= NWIO_DEFAULT;
	ip_fd->if_ipopt.nwio_tos= 0;
	ip_fd->if_ipopt.nwio_df= FALSE;
	ip_fd->if_ipopt.nwio_ttl= 255;
	ip_fd->if_ipopt.nwio_hdropt.iho_opt_siz= 0;

	ip_fd->if_port= ip_port;
	ip_fd->if_srfd= srfd;
	assert(ip_fd->if_rdbuf_head == NULL);
	ip_fd->if_get_userdata= get_userdata;
	ip_fd->if_put_userdata= put_userdata;
	ip_fd->if_put_pkt= put_pkt;

	return i;
}

PRIVATE void ip_close (fd)
int fd;
{
	ip_fd_t *ip_fd;
	acc_t *pack;

	ip_fd= &ip_fd_table[fd];

	assert ((ip_fd->if_flags & IFF_INUSE) &&
		!(ip_fd->if_flags & IFF_BUSY));

	if (ip_fd->if_flags & IFF_OPTSET)
		ip_unhash_proto(ip_fd);
	while (ip_fd->if_rdbuf_head)
	{
		pack= ip_fd->if_rdbuf_head;
		ip_fd->if_rdbuf_head= pack->acc_ext_link;
		bf_afree(pack);
	}
	ip_fd->if_flags= IFF_EMPTY;
}

PRIVATE void ip_buffree(priority)
int priority;
{
	int i;
	ip_port_t *ip_port;
	ip_fd_t *ip_fd;
	ip_ass_t *ip_ass;
	acc_t *pack, *next_pack;

	for (i= 0, ip_port= ip_port_table; i<ip_conf_nr; i++, ip_port++)
	{
		if (ip_port->ip_dl_type == IPDL_ETH)
		{
			/* Can't free de_frame.
			 * bf_check_acc(ip_port->ip_dl.dl_eth.de_frame);
			 */
			if (priority == IP_PRI_PORTBUFS)
			{
				next_pack= ip_port->ip_dl.dl_eth.de_arp_head;
				while(next_pack != NULL)
				{
					pack= next_pack;
					next_pack= pack->acc_ext_link;
					bf_afree(pack);
				}
				ip_port->ip_dl.dl_eth.de_arp_head= next_pack;

				next_pack= ip_port->ip_dl.dl_eth.de_q_head;
				while(next_pack != NULL)
				{
					pack= next_pack;
					next_pack= pack->acc_ext_link;
					bf_afree(pack);
				}
				ip_port->ip_dl.dl_eth.de_q_head= next_pack;
			}
		}
		else if (ip_port->ip_dl_type == IPDL_PSIP)
		{
			if (priority == IP_PRI_PORTBUFS)
			{
				next_pack= ip_port->ip_dl.dl_ps.ps_send_head;
				while (next_pack != NULL)
				{
					pack= next_pack;
					next_pack= pack->acc_ext_link;
					bf_afree(pack);
				}
				ip_port->ip_dl.dl_ps.ps_send_head= next_pack;
			}
		}
		if (priority == IP_PRI_PORTBUFS)
		{
			next_pack= ip_port->ip_loopb_head;
			while(next_pack && next_pack->acc_ext_link)
			{
				pack= next_pack;
				next_pack= pack->acc_ext_link;
				bf_afree(pack);
			}
			if (next_pack)
			{
				if (ev_in_queue(&ip_port->ip_loopb_event))
				{
#if DEBUG
					printf(
"not freeing ip_loopb_head, ip_loopb_event enqueued\n");
#endif
				}
				else
				{
					bf_afree(next_pack);
					next_pack= NULL;
				}
			}
			ip_port->ip_loopb_head= next_pack;

			next_pack= ip_port->ip_routeq_head;
			while(next_pack && next_pack->acc_ext_link)
			{
				pack= next_pack;
				next_pack= pack->acc_ext_link;
				bf_afree(pack);
			}
			if (next_pack)
			{
				if (ev_in_queue(&ip_port->ip_routeq_event))
				{
#if DEBUG
					printf(
"not freeing ip_loopb_head, ip_routeq_event enqueued\n");
#endif
				}
				else
				{
					bf_afree(next_pack);
					next_pack= NULL;
				}
			}
			ip_port->ip_routeq_head= next_pack;
		}
	}
	if (priority == IP_PRI_FDBUFS_EXTRA)
	{
		for (i= 0, ip_fd= ip_fd_table; i<IP_FD_NR; i++, ip_fd++)
		{
			while (ip_fd->if_rdbuf_head &&
				ip_fd->if_rdbuf_head->acc_ext_link)
			{
				pack= ip_fd->if_rdbuf_head;
				ip_fd->if_rdbuf_head= pack->acc_ext_link;
				bf_afree(pack);
			}
		}
	}
	if (priority == IP_PRI_FDBUFS)
	{
		for (i= 0, ip_fd= ip_fd_table; i<IP_FD_NR; i++, ip_fd++)
		{
			while (ip_fd->if_rdbuf_head)
			{
				pack= ip_fd->if_rdbuf_head;
				ip_fd->if_rdbuf_head= pack->acc_ext_link;
				bf_afree(pack);
			}
		}
	}
	if (priority == IP_PRI_ASSBUFS)
	{
		for (i= 0, ip_ass= ip_ass_table; i<IP_ASS_NR; i++, ip_ass++)
		{
			next_pack= ip_ass->ia_frags;
			while(ip_ass->ia_frags != NULL)
			{
				pack= ip_ass->ia_frags;
				ip_ass->ia_frags= pack->acc_ext_link;
				bf_afree(pack);
			}
			ip_ass->ia_first_time= 0;
		}
	}
}

#ifdef BUF_CONSISTENCY_CHECK
PRIVATE void ip_bufcheck()
{
	int i;
	ip_port_t *ip_port;
	ip_fd_t *ip_fd;
	ip_ass_t *ip_ass;
	acc_t *pack;

	for (i= 0, ip_port= ip_port_table; i<ip_conf_nr; i++, ip_port++)
	{
		if (ip_port->ip_dl_type == IPDL_ETH)
		{
			bf_check_acc(ip_port->ip_dl.dl_eth.de_frame);
			for (pack= ip_port->ip_dl.dl_eth.de_q_head; pack;
				pack= pack->acc_ext_link)
			{
				bf_check_acc(pack);
			}
			for (pack= ip_port->ip_dl.dl_eth.de_arp_head; pack;
				pack= pack->acc_ext_link)
			{
				bf_check_acc(pack);
			}
		}
		else if (ip_port->ip_dl_type == IPDL_PSIP)
		{
			for (pack= ip_port->ip_dl.dl_ps.ps_send_head; pack;
				pack= pack->acc_ext_link)
			{
				bf_check_acc(pack);
			}
		}
		for (pack= ip_port->ip_loopb_head; pack;
			pack= pack->acc_ext_link)
		{
			bf_check_acc(pack);
		}
		for (pack= ip_port->ip_routeq_head; pack;
			pack= pack->acc_ext_link)
		{
			bf_check_acc(pack);
		}
	}
	for (i= 0, ip_fd= ip_fd_table; i<IP_FD_NR; i++, ip_fd++)
	{
		for (pack= ip_fd->if_rdbuf_head; pack;
			pack= pack->acc_ext_link)
		{
			bf_check_acc(pack);
		}
	}
	for (i= 0, ip_ass= ip_ass_table; i<IP_ASS_NR; i++, ip_ass++)
	{
		for (pack= ip_ass->ia_frags; pack; pack= pack->acc_ext_link)
			bf_check_acc(pack);
	}
}
#endif /* BUF_CONSISTENCY_CHECK */

PRIVATE void ip_bad_callback(ip_port)
struct ip_port *ip_port;
{
	ip_panic(( "no callback filled in for port %d", ip_port->ip_port ));
}

/*
 * $PchId: ip.c,v 1.19 2005/06/28 14:17:40 philip Exp $
 */
