/*
inet/mnx_eth.c

Created:	Jan 2, 1992 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "proto.h"
#include "osdep_eth.h"
#include "generic/type.h"

#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/eth.h"
#include "generic/eth_int.h"
#include "generic/sr.h"

#include <minix/syslib.h>

THIS_FILE

FORWARD _PROTOTYPE( void setup_read, (eth_port_t *eth_port) );
FORWARD _PROTOTYPE( void read_int, (eth_port_t *eth_port, int count) );
FORWARD _PROTOTYPE( void write_int, (eth_port_t *eth_port) );
FORWARD _PROTOTYPE( void eth_recvev, (event_t *ev, ev_arg_t ev_arg) );
FORWARD _PROTOTYPE( void eth_sendev, (event_t *ev, ev_arg_t ev_arg) );
FORWARD _PROTOTYPE( eth_port_t *find_port, (message *m) );

PUBLIC void osdep_eth_init()
{
	int i, r, tasknr;
	struct eth_conf *ecp;
	eth_port_t *eth_port;
	message mess, repl_mess;

	for (i= 0, eth_port= eth_port_table, ecp= eth_conf;
		i<eth_conf_nr; i++, eth_port++, ecp++)
	{
		printf("INET: sys_getprocnr() for %s\n", ecp->ec_task);
		r = sys_getprocnr(&tasknr, ecp->ec_task, strlen(ecp->ec_task));
		if (r != OK)
		{
			ip_panic(( "unable to find task %s: %d\n",
				ecp->ec_task, r ));
		}


		eth_port->etp_osdep.etp_port= ecp->ec_port;
		eth_port->etp_osdep.etp_task= tasknr;
		ev_init(&eth_port->etp_osdep.etp_recvev);

		mess.m_type= DL_INIT;
		mess.DL_PORT= eth_port->etp_osdep.etp_port;
		mess.DL_PROC= this_proc;
		mess.DL_MODE= DL_NOMODE;

		r= send(eth_port->etp_osdep.etp_task, &mess);
		if (r<0)
		{
#if !CRAMPED
			printf(
		"osdep_eth_init: unable to send to ethernet task, error= %d\n",
				r);
#endif
			continue;
		}

		if (receive(eth_port->etp_osdep.etp_task, &mess)<0)
			ip_panic(("unable to receive"));

		if (mess.m3_i1 == ENXIO)
		{
#if !CRAMPED
			printf(
		"osdep_eth_init: no ethernet device at task=%d,port=%d\n",
				eth_port->etp_osdep.etp_task,
				eth_port->etp_osdep.etp_port);
#endif
			continue;
		}
		if (mess.m3_i1 != eth_port->etp_osdep.etp_port)
			ip_panic(("osdep_eth_init: DL_INIT error or wrong port: %d\n",
				mess.m3_i1));

		eth_port->etp_ethaddr= *(ether_addr_t *)mess.m3_ca1;

		sr_add_minor(if2minor(ecp->ec_ifno, ETH_DEV_OFF),
			i, eth_open, eth_close, eth_read, 
			eth_write, eth_ioctl, eth_cancel);

		eth_port->etp_flags |= EPF_ENABLED;
		eth_port->etp_wr_pack= 0;
		eth_port->etp_rd_pack= 0;
		setup_read (eth_port);
		eth_port++;
	}
}

PUBLIC void eth_write_port(eth_port, pack)
eth_port_t *eth_port;
acc_t *pack;
{
	eth_port_t *loc_port;
	message mess1, block_msg;
	int i, pack_size;
	acc_t *pack_ptr;
	iovec_t *iovec;
	u8_t *eth_dst_ptr;
	int multicast, r;
	ev_arg_t ev_arg;

	assert(eth_port->etp_wr_pack == NULL);
	eth_port->etp_wr_pack= pack;

	iovec= eth_port->etp_osdep.etp_wr_iovec;
	pack_size= 0;
	for (i=0, pack_ptr= pack; i<IOVEC_NR && pack_ptr; i++,
		pack_ptr= pack_ptr->acc_next)
	{
		iovec[i].iov_addr= (vir_bytes)ptr2acc_data(pack_ptr);
		pack_size += iovec[i].iov_size= pack_ptr->acc_length;
	}
	if (i>= IOVEC_NR)
	{
		pack= bf_pack(pack);		/* packet is too fragmented */
		eth_port->etp_wr_pack= pack;
		pack_size= 0;
		for (i=0, pack_ptr= pack; i<IOVEC_NR && pack_ptr;
			i++, pack_ptr= pack_ptr->acc_next)
		{
			iovec[i].iov_addr= (vir_bytes)ptr2acc_data(pack_ptr);
			pack_size += iovec[i].iov_size= pack_ptr->acc_length;
		}
	}
	assert (i< IOVEC_NR);
	assert (pack_size >= ETH_MIN_PACK_SIZE);

	if (i == 1)
	{
		/* simple packets can be sent using DL_WRITE instead of 
		 * DL_WRITEV.
		 */
		mess1.DL_COUNT= iovec[0].iov_size;
		mess1.DL_ADDR= (char *)iovec[0].iov_addr;
		mess1.m_type= DL_WRITE;
	}
	else
	{
		mess1.DL_COUNT= i;
		mess1.DL_ADDR= (char *)iovec;
		mess1.m_type= DL_WRITEV;
	}
	mess1.DL_PORT= eth_port->etp_osdep.etp_port;
	mess1.DL_PROC= this_proc;
	mess1.DL_MODE= DL_NOMODE;

	for (;;)
	{
		r= send (eth_port->etp_osdep.etp_task, &mess1);
		if (r != ELOCKED)
			break;

		/* ethernet task is sending to this task, I hope */
		r= receive(eth_port->etp_osdep.etp_task, &block_msg);
		if (r < 0)
			ip_panic(("unable to receive"));

		loc_port= eth_port;
		if (loc_port->etp_osdep.etp_port != block_msg.DL_PORT)
		{
			loc_port= find_port(&block_msg);
		}
		assert(block_msg.DL_STAT & (DL_PACK_SEND|DL_PACK_RECV));
		if (block_msg.DL_STAT & DL_PACK_SEND)
		{
			assert(loc_port != eth_port);
			loc_port->etp_osdep.etp_sendrepl= block_msg;
			ev_arg.ev_ptr= loc_port;
			ev_enqueue(&loc_port->etp_sendev, eth_sendev, ev_arg);
		}
		if (block_msg.DL_STAT & DL_PACK_RECV)
		{
			loc_port->etp_osdep.etp_recvrepl= block_msg;
			ev_arg.ev_ptr= loc_port;
			ev_enqueue(&loc_port->etp_osdep.etp_recvev,
				eth_recvev, ev_arg);
		}
	}

	if (r < 0)
		ip_panic(("unable to send"));

	r= receive(eth_port->etp_osdep.etp_task, &mess1);
	if (r < 0)
		ip_panic(("unable to receive"));

	assert(mess1.m_type == DL_TASK_REPLY &&
		mess1.DL_PORT == mess1.DL_PORT &&
		mess1.DL_PROC == this_proc);
	assert((mess1.DL_STAT >> 16) == OK);

	if (mess1.DL_STAT & DL_PACK_RECV)
	{
		eth_port->etp_osdep.etp_recvrepl= mess1;
		ev_arg.ev_ptr= eth_port;
		ev_enqueue(&eth_port->etp_osdep.etp_recvev, eth_recvev,
			ev_arg);
	}
	if (!(mess1.DL_STAT & DL_PACK_SEND))
	{
		/* Packet is not yet sent. */
		return;
	}

	/* If the port is in promiscuous mode or the packet is
	 * broadcasted/multicasted, enqueue the reply packet.
	 */
	eth_dst_ptr= (u8_t *)ptr2acc_data(pack);
	multicast= (*eth_dst_ptr & 1);	/* low order bit indicates multicast */
	if (multicast || (eth_port->etp_osdep.etp_recvconf & NWEO_EN_PROMISC))
	{
		eth_port->etp_osdep.etp_sendrepl= mess1;
		ev_arg.ev_ptr= eth_port;
		ev_enqueue(&eth_port->etp_sendev, eth_sendev, ev_arg);

		/* Pretend that we didn't get a reply. */
		return;
	}

	/* packet is sent */
	bf_afree(eth_port->etp_wr_pack);
	eth_port->etp_wr_pack= NULL;
}

PUBLIC void eth_rec(m)
message *m;
{
	int i;
	eth_port_t *loc_port;
	int stat;

	assert(m->m_type == DL_TASK_REPLY);

	set_time (m->DL_CLCK);

	for (i=0, loc_port= eth_port_table; i<eth_conf_nr; i++, loc_port++)
	{
		if (loc_port->etp_osdep.etp_port == m->DL_PORT &&
			loc_port->etp_osdep.etp_task == m->m_source)
			break;
	}
	if (i == eth_conf_nr)
	{
		ip_panic(("message from unknown source: %d:%d",
			m->m_source, m->DL_PORT));
	}

	stat= m->DL_STAT & 0xffff;

	assert(stat & (DL_PACK_SEND|DL_PACK_RECV));
	if (stat & DL_PACK_SEND)
		write_int(loc_port);
	if (stat & DL_PACK_RECV)
		read_int(loc_port, m->DL_COUNT);
}

#ifndef notdef
PUBLIC int eth_get_stat(eth_port, eth_stat)
eth_port_t *eth_port;
eth_stat_t *eth_stat;
{
	acc_t *acc;
	int result;
	message mess, mlocked;

	mess.m_type= DL_GETSTAT;
	mess.DL_PORT= eth_port->etp_osdep.etp_port;
	mess.DL_PROC= this_proc;
	mess.DL_ADDR= (char *)eth_stat;

	for (;;)
	{
		result= send(eth_port->etp_osdep.etp_task, &mess);
		if (result != ELOCKED)
			break;
		result= receive(eth_port->etp_osdep.etp_task, &mlocked);
		assert(result == OK);

		compare(mlocked.m_type, ==, DL_TASK_REPLY);
		eth_rec(&mlocked);
	}
	assert(result == OK);

	result= receive(eth_port->etp_osdep.etp_task, &mess);
	assert(result == OK);
	assert(mess.m_type == DL_TASK_REPLY);

	result= mess.DL_STAT >> 16;
assert (result == 0);

	if (mess.DL_STAT)
	{
#if DEBUG
 { where(); printf("calling eth_rec()\n"); }
#endif
		eth_rec(&mess);
	}
	return OK;
}
#endif

#ifndef notdef
PUBLIC void eth_set_rec_conf (eth_port, flags)
eth_port_t *eth_port;
u32_t flags;
{
	int result;
	unsigned dl_flags;
	message mess, repl_mess;

	dl_flags= DL_NOMODE;
	if (flags & NWEO_EN_BROAD)
		dl_flags |= DL_BROAD_REQ;
	if (flags & NWEO_EN_MULTI)
		dl_flags |= DL_MULTI_REQ;
	if (flags & NWEO_EN_PROMISC)
		dl_flags |= DL_PROMISC_REQ;

	mess.m_type= DL_INIT;
	mess.DL_PORT= eth_port->etp_osdep.etp_port;
	mess.DL_PROC= this_proc;
	mess.DL_MODE= dl_flags;

	do
	{
		result= send (eth_port->etp_osdep.etp_task, &mess);
		if (result == ELOCKED)
		/* Ethernet task is sending to this task, I hope */
		{
			if (receive (eth_port->etp_osdep.etp_task,
				&repl_mess)< 0)
			{
				ip_panic(("unable to receive"));
			}

			compare(repl_mess.m_type, ==, DL_TASK_REPLY);
			eth_rec(&repl_mess);
		}
	} while (result == ELOCKED);
	
	if (result < 0)
		ip_panic(("unable to send(%d)", result));

	if (receive (eth_port->etp_osdep.etp_task, &repl_mess) < 0)
		ip_panic(("unable to receive"));

	assert (repl_mess.m_type == DL_INIT_REPLY);
	if (repl_mess.m3_i1 != eth_port->etp_osdep.etp_port)
	{
		ip_panic(("got reply for wrong port"));
	}
	eth_port->etp_osdep.etp_recvconf= flags;
}
#endif

PRIVATE void write_int(eth_port)
eth_port_t *eth_port;
{
	acc_t *pack;
	int multicast;
	u8_t *eth_dst_ptr;

	pack= eth_port->etp_wr_pack;
	eth_port->etp_wr_pack= NULL;

	eth_dst_ptr= (u8_t *)ptr2acc_data(pack);
	multicast= (*eth_dst_ptr & 1);	/* low order bit indicates multicast */
	if (multicast || (eth_port->etp_osdep.etp_recvconf & NWEO_EN_PROMISC))
		eth_arrive(eth_port, pack, bf_bufsize(pack));
	else
		bf_afree(pack);

	eth_restart_write(eth_port);
}

PRIVATE void read_int(eth_port, count)
eth_port_t *eth_port;
int count;
{
	acc_t *pack, *cut_pack;

	pack= eth_port->etp_rd_pack;
	eth_port->etp_rd_pack= NULL;

	cut_pack= bf_cut(pack, 0, count);
	bf_afree(pack);

	eth_arrive(eth_port, cut_pack, count);
	
	eth_port->etp_flags &= ~(EPF_READ_IP|EPF_READ_SP);
	setup_read(eth_port);
}

PRIVATE void setup_read(eth_port)
eth_port_t *eth_port;
{
	eth_port_t *loc_port;
	acc_t *pack, *pack_ptr;
	message mess1, block_msg;
	iovec_t *iovec;
	ev_arg_t ev_arg;
	int i, r;

	assert(!(eth_port->etp_flags & (EPF_READ_IP|EPF_READ_SP)));

	do
	{
		assert (!eth_port->etp_rd_pack);

		iovec= eth_port->etp_osdep.etp_rd_iovec;
		pack= bf_memreq (ETH_MAX_PACK_SIZE);

		for (i=0, pack_ptr= pack; i<RD_IOVEC && pack_ptr;
			i++, pack_ptr= pack_ptr->acc_next)
		{
			iovec[i].iov_addr= (vir_bytes)ptr2acc_data(pack_ptr);
			iovec[i].iov_size= (vir_bytes)pack_ptr->acc_length;
		}
		assert (!pack_ptr);

		mess1.m_type= DL_READV;
		mess1.DL_PORT= eth_port->etp_osdep.etp_port;
		mess1.DL_PROC= this_proc;
		mess1.DL_COUNT= i;
		mess1.DL_ADDR= (char *)iovec;

		for (;;)
		{
			r= send (eth_port->etp_osdep.etp_task, &mess1);
			if (r != ELOCKED)
				break;

			/* ethernet task is sending to this task, I hope */
			r= receive(eth_port->etp_osdep.etp_task, &block_msg);
			if (r < 0)
				ip_panic(("unable to receive"));

			loc_port= eth_port;
			if (loc_port->etp_osdep.etp_port != block_msg.DL_PORT)
			{
				loc_port= find_port(&block_msg);
			}
			assert(block_msg.DL_STAT &
				(DL_PACK_SEND|DL_PACK_RECV));
			if (block_msg.DL_STAT & DL_PACK_SEND)
			{
				loc_port->etp_osdep.etp_sendrepl= block_msg;
				ev_arg.ev_ptr= loc_port;
				ev_enqueue(&loc_port->etp_sendev, eth_sendev,
					ev_arg);
			}
			if (block_msg.DL_STAT & DL_PACK_RECV)
			{
				assert(loc_port != eth_port);
				loc_port->etp_osdep.etp_recvrepl= block_msg;
				ev_arg.ev_ptr= loc_port;
				ev_enqueue(&loc_port->etp_osdep.etp_recvev,
					eth_recvev, ev_arg);
			}
		}

		if (r < 0)
			ip_panic(("unable to send"));

		r= receive (eth_port->etp_osdep.etp_task, &mess1);
		if (r < 0)
			ip_panic(("unable to receive"));

		assert (mess1.m_type == DL_TASK_REPLY &&
			mess1.DL_PORT == mess1.DL_PORT &&
			mess1.DL_PROC == this_proc);
		compare((mess1.DL_STAT >> 16), ==, OK);

		if (mess1.DL_STAT & DL_PACK_RECV)
		{
			/* packet received */
			pack_ptr= bf_cut(pack, 0, mess1.DL_COUNT);
			bf_afree(pack);

			eth_arrive(eth_port, pack_ptr, mess1.DL_COUNT);
		}
		else
		{
			/* no packet received */
			eth_port->etp_rd_pack= pack;
			eth_port->etp_flags |= EPF_READ_IP;
		}

		if (mess1.DL_STAT & DL_PACK_SEND)
		{
			eth_port->etp_osdep.etp_sendrepl= mess1;
			ev_arg.ev_ptr= eth_port;
			ev_enqueue(&eth_port->etp_sendev, eth_sendev, ev_arg);
		}
	} while (!(eth_port->etp_flags & EPF_READ_IP));
	eth_port->etp_flags |= EPF_READ_SP;
}

PRIVATE void eth_recvev(ev, ev_arg)
event_t *ev;
ev_arg_t ev_arg;
{
	eth_port_t *eth_port;
	message *m_ptr;

	eth_port= ev_arg.ev_ptr;
	assert(ev == &eth_port->etp_osdep.etp_recvev);
	m_ptr= &eth_port->etp_osdep.etp_recvrepl;

	assert(m_ptr->m_type == DL_TASK_REPLY);
	assert(eth_port->etp_osdep.etp_port == m_ptr->DL_PORT);

	assert(m_ptr->DL_STAT & DL_PACK_RECV);
	m_ptr->DL_STAT &= ~DL_PACK_RECV;

	read_int(eth_port, m_ptr->DL_COUNT);
}

PRIVATE void eth_sendev(ev, ev_arg)
event_t *ev;
ev_arg_t ev_arg;
{
	eth_port_t *eth_port;
	message *m_ptr;

	eth_port= ev_arg.ev_ptr;
	assert(ev == &eth_port->etp_sendev);
	m_ptr= &eth_port->etp_osdep.etp_sendrepl;

	assert (m_ptr->m_type == DL_TASK_REPLY);
	assert(eth_port->etp_osdep.etp_port == m_ptr->DL_PORT);

	assert(m_ptr->DL_STAT & DL_PACK_SEND);
	m_ptr->DL_STAT &= ~DL_PACK_SEND;

	/* packet is sent */
	write_int(eth_port);
}

PRIVATE eth_port_t *find_port(m)
message *m;
{
	eth_port_t *loc_port;
	int i;

	for (i=0, loc_port= eth_port_table; i<eth_conf_nr; i++, loc_port++)
	{
		if (loc_port->etp_osdep.etp_port == m->DL_PORT)
			break;
	}
	assert (i<eth_conf_nr);
	return loc_port;
}

/*
 * $PchId: mnx_eth.c,v 1.8 1995/11/21 06:41:57 philip Exp $
 */
