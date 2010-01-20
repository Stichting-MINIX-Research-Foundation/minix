/*
inet/mnx_eth.c

Created:	Jan 2, 1992 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include <minix/ds.h>
#include <minix/safecopies.h>
#include "proto.h"
#include "osdep_eth.h"
#include "generic/type.h"

#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/eth.h"
#include "generic/eth_int.h"
#include "generic/sr.h"

THIS_FILE

static int recv_debug= 0;

FORWARD _PROTOTYPE( void setup_read, (eth_port_t *eth_port) );
FORWARD _PROTOTYPE( void read_int, (eth_port_t *eth_port, int count) );
FORWARD _PROTOTYPE( void eth_issue_send, (eth_port_t *eth_port) );
FORWARD _PROTOTYPE( void write_int, (eth_port_t *eth_port) );
FORWARD _PROTOTYPE( void eth_recvev, (event_t *ev, ev_arg_t ev_arg) );
FORWARD _PROTOTYPE( void eth_sendev, (event_t *ev, ev_arg_t ev_arg) );
FORWARD _PROTOTYPE( eth_port_t *find_port, (message *m) );
FORWARD _PROTOTYPE( void eth_restart, (eth_port_t *eth_port, int tasknr) );
FORWARD _PROTOTYPE( void send_getstat, (eth_port_t *eth_port) );

PUBLIC void osdep_eth_init()
{
	int i, j, r, rport;
	u32_t tasknr;
	struct eth_conf *ecp;
	eth_port_t *eth_port, *rep;
	message mess;
	cp_grant_id_t gid;

	/* First initialize normal ethernet interfaces */
	for (i= 0, ecp= eth_conf, eth_port= eth_port_table;
		i<eth_conf_nr; i++, ecp++, eth_port++)
	{
		/* Set all grants to invalid */
		for (j= 0; j<IOVEC_NR; j++)
			eth_port->etp_osdep.etp_wr_iovec[j].iov_grant= -1;
		eth_port->etp_osdep.etp_wr_vec_grant= -1;
		for (j= 0; j<RD_IOVEC; j++)
			eth_port->etp_osdep.etp_rd_iovec[j].iov_grant= -1;
		eth_port->etp_osdep.etp_rd_vec_grant= -1;

		eth_port->etp_osdep.etp_state= OEPS_INIT;
		eth_port->etp_osdep.etp_flags= OEPF_EMPTY;
		eth_port->etp_osdep.etp_stat_gid= -1;
		eth_port->etp_osdep.etp_stat_buf= NULL;

		if (eth_is_vlan(ecp))
			continue;

		/* Allocate grants */
		for (j= 0; j<IOVEC_NR; j++)
		{
			if (cpf_getgrants(&gid, 1) != 1)
			{
				ip_panic((
			"osdep_eth_init: cpf_getgrants failed: %d\n",
					errno));
			}
			eth_port->etp_osdep.etp_wr_iovec[j].iov_grant= gid;
		}
		if (cpf_getgrants(&gid, 1) != 1)
		{
			ip_panic((
		"osdep_eth_init: cpf_getgrants failed: %d\n",
				errno));
		}
		eth_port->etp_osdep.etp_wr_vec_grant= gid;
		for (j= 0; j<RD_IOVEC; j++)
		{
			if (cpf_getgrants(&gid, 1) != 1)
			{
				ip_panic((
			"osdep_eth_init: cpf_getgrants failed: %d\n",
					errno));
			}
			eth_port->etp_osdep.etp_rd_iovec[j].iov_grant= gid;
		}
		if (cpf_getgrants(&gid, 1) != 1)
		{
			ip_panic((
		"osdep_eth_init: cpf_getgrants failed: %d\n",
				errno));
		}
		eth_port->etp_osdep.etp_rd_vec_grant= gid;

		r= ds_retrieve_label_num(ecp->ec_task, &tasknr);
		if (r != OK && r != ESRCH)
		{
			printf("inet: ds_retrieve_label_num failed for '%s': %d\n",
				ecp->ec_task, r);
		}
		if (r != OK)
		{
			/* Eventually, we expect ethernet drivers to be
			 * started after INET. So we always end up here. And
			 * the findproc can be removed.
			 */
#if 0
			printf("eth%d: unable to find task %s: %d\n",
				i, ecp->ec_task, r);
#endif
			tasknr= ANY;
		}

 		eth_port->etp_osdep.etp_port= ecp->ec_port;
		eth_port->etp_osdep.etp_task= tasknr;
		eth_port->etp_osdep.etp_recvconf= 0;
		eth_port->etp_osdep.etp_send_ev= 0;
		ev_init(&eth_port->etp_osdep.etp_recvev);

		mess.m_type= DL_CONF;
		mess.DL_PORT= eth_port->etp_osdep.etp_port;
		mess.DL_PROC= this_proc;
		mess.DL_MODE= DL_NOMODE;

		if (tasknr == ANY)
			r= ENXIO;
		else
		{
			assert(eth_port->etp_osdep.etp_state == OEPS_INIT);
			r= asynsend(eth_port->etp_osdep.etp_task, &mess);
			if (r == OK)
				eth_port->etp_osdep.etp_state= OEPS_CONF_SENT;
			else
			{
				printf(
		"osdep_eth_init: unable to send to ethernet task, error= %d\n",
					r);
			}
		}

		sr_add_minor(if2minor(ecp->ec_ifno, ETH_DEV_OFF),
			i, eth_open, eth_close, eth_read, 
			eth_write, eth_ioctl, eth_cancel, eth_select);

		eth_port->etp_flags |= EPF_ENABLED;
		eth_port->etp_vlan= 0;
		eth_port->etp_vlan_port= NULL;
		eth_port->etp_wr_pack= 0;
		eth_port->etp_rd_pack= 0;
	}

	/* And now come the VLANs */
	for (i= 0, ecp= eth_conf, eth_port= eth_port_table;
		i<eth_conf_nr; i++, ecp++, eth_port++)
	{
		if (!eth_is_vlan(ecp))
			continue;

 		eth_port->etp_osdep.etp_port= ecp->ec_port;
		eth_port->etp_osdep.etp_task= ANY;
		ev_init(&eth_port->etp_osdep.etp_recvev);

		rport= eth_port->etp_osdep.etp_port;
		assert(rport >= 0 && rport < eth_conf_nr);
		rep= &eth_port_table[rport];
		if (!(rep->etp_flags & EPF_ENABLED))
		{
			printf(
			"eth%d: underlying ethernet device %d not enabled",
				i, rport);
			continue;
		}
		if (rep->etp_vlan != 0)
		{
			printf(
			"eth%d: underlying ethernet device %d is a VLAN",
				i, rport);
			continue;
		}
		
		if (rep->etp_flags & EPF_GOT_ADDR)
		{
			eth_port->etp_ethaddr= rep->etp_ethaddr;
			printf("osdep_eth_init: setting EPF_GOT_ADDR\n");
			eth_port->etp_flags |= EPF_GOT_ADDR;
		}

		sr_add_minor(if2minor(ecp->ec_ifno, ETH_DEV_OFF),
			i, eth_open, eth_close, eth_read, 
			eth_write, eth_ioctl, eth_cancel, eth_select);

		eth_port->etp_flags |= EPF_ENABLED;
		eth_port->etp_vlan= ecp->ec_vlan;
		eth_port->etp_vlan_port= rep;
		assert(eth_port->etp_vlan != 0);
		eth_port->etp_wr_pack= 0;
		eth_port->etp_rd_pack= 0;
		eth_reg_vlan(rep, eth_port);
	}
}

PUBLIC void eth_write_port(eth_port, pack)
eth_port_t *eth_port;
acc_t *pack;
{
	assert(!no_ethWritePort);
	assert(!eth_port->etp_vlan);

	assert(eth_port->etp_wr_pack == NULL);
	eth_port->etp_wr_pack= pack;

	if (eth_port->etp_osdep.etp_state != OEPS_IDLE)
	{
		eth_port->etp_osdep.etp_flags |= OEPF_NEED_SEND;
		return;
	}


	eth_issue_send(eth_port);
}

#if 0
PRIVATE int notification_count;
#endif

PUBLIC void eth_rec(message *m)
{
	int i, r, m_type, stat;
	eth_port_t *loc_port, *vlan_port;
	char *drivername;
	struct eth_conf *ecp;

	m_type= m->m_type;
	if (m_type == DL_NAME_REPLY)
	{
		drivername= m->m3_ca1;
#if 0
		printf("eth_rec: got name: %s\n", drivername);

		notification_count= 0;
#endif

		/* Re-init ethernet interfaces */
		for (i= 0, ecp= eth_conf, loc_port= eth_port_table;
			i<eth_conf_nr; i++, ecp++, loc_port++)
		{
			if (eth_is_vlan(ecp))
				continue;

			if (strcmp(ecp->ec_task, drivername) != 0)
			{
				/* Wrong driver */
				continue;
			}
			eth_restart(loc_port, m->m_source);
		}
		return;
	}

	assert(m_type == DL_CONF_REPLY || m_type == DL_TASK_REPLY ||
		m_type == DL_STAT_REPLY);

	for (i=0, loc_port= eth_port_table; i<eth_conf_nr; i++, loc_port++)
	{
		if (loc_port->etp_osdep.etp_port == m->DL_PORT &&
			loc_port->etp_osdep.etp_task == m->m_source)
			break;
	}
	if (i >= eth_conf_nr)
	{
		printf("eth_rec: bad port %d in message type 0x%x from %d\n",
			m->DL_PORT, m_type, m->m_source);
		return;
	}

	if (loc_port->etp_osdep.etp_state == OEPS_CONF_SENT)
	{
		if (m_type == DL_TASK_REPLY)
		{
			stat= m->DL_STAT & 0xffff;

			if (stat & DL_PACK_SEND)
				write_int(loc_port);
			if (stat & DL_PACK_RECV)
				read_int(loc_port, m->DL_COUNT);
			return;
		}

		if (m_type != DL_CONF_REPLY)
		{
			printf(
	"eth_rec: got bad message type 0x%x from %d in CONF state\n",
				m_type, m->m_source);
			return;
		}

		r= m->m3_i1;
		if (r == ENXIO)
		{
			printf(
	"eth_rec(conf_reply): no ethernet device at task=%d,port=%d\n",
				loc_port->etp_osdep.etp_task, 
				loc_port->etp_osdep.etp_port);
			return;
		}
		if (r < 0)
		{
			ip_panic(("eth_rec: DL_INIT returned error %d\n", r));
			return;
		}
	
		loc_port->etp_osdep.etp_flags &= ~OEPF_NEED_CONF;
		loc_port->etp_osdep.etp_state= OEPS_IDLE;
		loc_port->etp_flags |= EPF_ENABLED;

		loc_port->etp_ethaddr= *(ether_addr_t *)m->m3_ca1;
		if (!(loc_port->etp_flags & EPF_GOT_ADDR))
		{
			loc_port->etp_flags |= EPF_GOT_ADDR;
#if 0
			printf("eth_rec: calling eth_restart_ioctl\n");
#endif
			eth_restart_ioctl(loc_port);

			/* Also update any VLANs on this device */
			for (i=0, vlan_port= eth_port_table; i<eth_conf_nr;
				i++, vlan_port++)
			{
				if (!(vlan_port->etp_flags & EPF_ENABLED))
					continue;
				if (vlan_port->etp_vlan_port != loc_port)
					continue;
				 
				vlan_port->etp_ethaddr= loc_port->etp_ethaddr;
				vlan_port->etp_flags |= EPF_GOT_ADDR;
				eth_restart_ioctl(vlan_port);
			}
		}
		if (!(loc_port->etp_flags & EPF_READ_IP))
			setup_read (loc_port);

#if 0
		if (loc_port->etp_osdep.etp_flags & OEPF_NEED_SEND)
		{
			printf("eth_rec(conf): OEPF_NEED_SEND is set\n");
		}
		if (loc_port->etp_osdep.etp_flags & OEPF_NEED_RECV)
		{
			printf("eth_rec(conf): OEPF_NEED_RECV is set\n");
		}
		if (loc_port->etp_osdep.etp_flags & OEPF_NEED_STAT)
		{
			printf("eth_rec(conf): OEPF_NEED_STAT is set\n");
		}
#endif

		return;
	}
	if (loc_port->etp_osdep.etp_state == OEPS_GETSTAT_SENT)
	{
		if (m_type != DL_STAT_REPLY)
		{
			printf(
	"eth_rec: got bad message type 0x%x from %d in GETSTAT state\n",
				m_type, m->m_source);
			return;
		}

		r= m->DL_STAT;
		if (r != OK)
		{
			ip_warning(("eth_rec: DL_STAT returned error %d\n",
				r));
			return;
		}
	
		loc_port->etp_osdep.etp_state= OEPS_IDLE;
		loc_port->etp_osdep.etp_flags &= ~OEPF_NEED_STAT;

		assert(loc_port->etp_osdep.etp_stat_gid != -1);
		cpf_revoke(loc_port->etp_osdep.etp_stat_gid);
		loc_port->etp_osdep.etp_stat_gid= -1;
		loc_port->etp_osdep.etp_stat_buf= NULL;
		
		/* Finish ioctl */
		assert(loc_port->etp_flags & EPF_GOT_ADDR);
		eth_restart_ioctl(loc_port);

#if 0
		if (loc_port->etp_osdep.etp_flags & OEPF_NEED_SEND)
		{
			printf("eth_rec(stat): OEPF_NEED_SEND is set\n");
		}
		if (loc_port->etp_osdep.etp_flags & OEPF_NEED_RECV)
		{
			printf("eth_rec(stat): OEPF_NEED_RECV is set\n");
		}
		if (loc_port->etp_osdep.etp_flags & OEPF_NEED_CONF)
		{
			printf("eth_rec(stat): OEPF_NEED_CONF is set\n");
		}
#endif

#if 0
		if (loc_port->etp_osdep.etp_state == OEPS_IDLE &&
			(loc_port->etp_osdep.etp_flags & OEPF_NEED_CONF))
		{
			eth_set_rec_conf(loc_port,
				loc_port->etp_osdep.etp_recvconf);
		}
#endif
		return;
	}
	assert(loc_port->etp_osdep.etp_state == OEPS_IDLE  ||
		loc_port->etp_osdep.etp_state == OEPS_RECV_SENT ||
		loc_port->etp_osdep.etp_state == OEPS_SEND_SENT ||
		(printf("etp_state = %d\n", loc_port->etp_osdep.etp_state), 0));
	loc_port->etp_osdep.etp_state= OEPS_IDLE;

#if 0 /* Ethernet driver is not trusted */
	set_time (m->DL_CLCK);
#endif

	stat= m->DL_STAT & 0xffff;

#if 0
	if (!(stat & (DL_PACK_SEND|DL_PACK_RECV)))
		printf("eth_rec: neither DL_PACK_SEND nor DL_PACK_RECV\n");
#endif
	if (stat & DL_PACK_SEND)
		write_int(loc_port);
	if (stat & DL_PACK_RECV)
	{
		if (recv_debug)
		{
			printf("eth_rec: eth%d got DL_PACK_RECV\n",
				m->DL_PORT);
		}
		read_int(loc_port, m->DL_COUNT);
	}

	if (loc_port->etp_osdep.etp_state == OEPS_IDLE &&
		loc_port->etp_osdep.etp_flags & OEPF_NEED_SEND)
	{
		loc_port->etp_osdep.etp_flags &= ~OEPF_NEED_SEND;
		if (loc_port->etp_wr_pack)
			eth_issue_send(loc_port);
	}
	if (loc_port->etp_osdep.etp_state == OEPS_IDLE &&
		(loc_port->etp_osdep.etp_flags & OEPF_NEED_RECV))
	{
		loc_port->etp_osdep.etp_flags &= ~OEPF_NEED_RECV;
		if (!(loc_port->etp_flags & EPF_READ_IP))
			setup_read (loc_port);
	}
	if (loc_port->etp_osdep.etp_flags & OEPF_NEED_CONF)
	{
		printf("eth_rec: OEPF_NEED_CONF is set\n");
	}
	if (loc_port->etp_osdep.etp_state == OEPS_IDLE &&
		(loc_port->etp_osdep.etp_flags & OEPF_NEED_STAT))
	{
		send_getstat(loc_port);
	}
}

PUBLIC void eth_check_drivers(message *m)
{
	int r, tasknr;

	tasknr= m->m_source;
#if 0
	if (notification_count < 100)
	{
		notification_count++;
		printf("eth_check_drivers: got a notification #%d from %d\n",
			notification_count, tasknr);
	}
#endif
		
	m->m_type= DL_GETNAME;
	r= asynsend(tasknr, m);
	if (r != OK)
	{
		printf("eth_check_drivers: asynsend to %d failed: %d\n",
			tasknr, r);
		return;
	}
}

PUBLIC int eth_get_stat(eth_port, eth_stat)
eth_port_t *eth_port;
eth_stat_t *eth_stat;
{
	cp_grant_id_t gid;

	assert(!eth_port->etp_vlan);

	if (eth_port->etp_osdep.etp_flags & OEPF_NEED_STAT)
		ip_panic(( "eth_get_stat: getstat already in progress" ));

	gid= cpf_grant_direct(eth_port->etp_osdep.etp_task,
		(vir_bytes)eth_stat, sizeof(*eth_stat), CPF_WRITE);
	if (gid == -1)
	{
		ip_panic(( "eth_get_stat: cpf_grant_direct failed: %d\n",
			errno));
	}
	assert(eth_port->etp_osdep.etp_stat_gid == -1);
	eth_port->etp_osdep.etp_stat_gid= gid;
	eth_port->etp_osdep.etp_stat_buf= eth_stat;

	if (eth_port->etp_osdep.etp_state != OEPS_IDLE)
	{
		eth_port->etp_osdep.etp_flags |= OEPF_NEED_STAT;
		return SUSPEND;
	}

	send_getstat(eth_port);

	return SUSPEND;
}

PUBLIC void eth_set_rec_conf (eth_port, flags)
eth_port_t *eth_port;
u32_t flags;
{
	int r;
	unsigned dl_flags, mask;
	message mess;

	assert(!eth_port->etp_vlan);

	if (!(eth_port->etp_flags & EPF_GOT_ADDR))
	{
		/* We have never seen the device. */
#if 0
		printf("eth_set_rec_conf: waiting for device to appear\n");
#endif
		return;
	}

	if (eth_port->etp_osdep.etp_state != OEPS_IDLE)
	{
		printf(
		"eth_set_rec_conf: setting OEPF_NEED_CONF, state = %d\n",
			eth_port->etp_osdep.etp_state);
		eth_port->etp_osdep.etp_flags |= OEPF_NEED_CONF;
		return;
	}

	mask = NWEO_EN_BROAD | NWEO_EN_MULTI | NWEO_EN_PROMISC;
	if ((eth_port->etp_osdep.etp_recvconf & mask) == (flags & mask))
	{
		/* No change for the driver, so don't send an update */
		return;
	}

	eth_port->etp_osdep.etp_recvconf= flags;
	dl_flags= DL_NOMODE;
	if (flags & NWEO_EN_BROAD)
		dl_flags |= DL_BROAD_REQ;
	if (flags & NWEO_EN_MULTI)
		dl_flags |= DL_MULTI_REQ;
	if (flags & NWEO_EN_PROMISC)
		dl_flags |= DL_PROMISC_REQ;

	mess.m_type= DL_CONF;
	mess.DL_PORT= eth_port->etp_osdep.etp_port;
	mess.DL_PROC= this_proc;
	mess.DL_MODE= dl_flags;

	assert(eth_port->etp_osdep.etp_state == OEPS_IDLE);
	r= asynsend(eth_port->etp_osdep.etp_task, &mess);
	eth_port->etp_osdep.etp_state= OEPS_CONF_SENT;

	if (r < 0)
	{
		printf("eth_set_rec_conf: asynsend to %d failed: %d\n",
			eth_port->etp_osdep.etp_task, r);
		return;
	}
}

PRIVATE void eth_issue_send(eth_port)
eth_port_t *eth_port;
{
	int i, r, pack_size;
	acc_t *pack, *pack_ptr;
	iovec_s_t *iovec;
	message m;

	iovec= eth_port->etp_osdep.etp_wr_iovec;
	pack= eth_port->etp_wr_pack;
	pack_size= 0;
	for (i=0, pack_ptr= pack; i<IOVEC_NR && pack_ptr; i++,
		pack_ptr= pack_ptr->acc_next)
	{
		r= cpf_setgrant_direct(iovec[i].iov_grant,
			eth_port->etp_osdep.etp_task,
			(vir_bytes)ptr2acc_data(pack_ptr),
			(vir_bytes)pack_ptr->acc_length,
			CPF_READ);
		if (r != 0)
		{
			ip_panic((
		"eth_write_port: cpf_setgrant_direct failed: %d\n",
				errno));
		}
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
			r= cpf_setgrant_direct(iovec[i].iov_grant,
				eth_port->etp_osdep.etp_task,
				(vir_bytes)ptr2acc_data(pack_ptr),
				(vir_bytes)pack_ptr->acc_length,
				CPF_READ);
			if (r != 0)
			{
				ip_panic((
			"eth_write_port: cpf_setgrant_direct failed: %d\n",
					errno));
			}
			pack_size += iovec[i].iov_size= pack_ptr->acc_length;
		}
	}
	assert (i< IOVEC_NR);
	assert (pack_size >= ETH_MIN_PACK_SIZE);

	r= cpf_setgrant_direct(eth_port->etp_osdep.etp_wr_vec_grant,
		eth_port->etp_osdep.etp_task,
		(vir_bytes)iovec,
		(vir_bytes)(i * sizeof(iovec[0])),
		CPF_READ);
	if (r != 0)
	{
		ip_panic((
	"eth_write_port: cpf_setgrant_direct failed: %d\n",
			errno));
	}
	m.DL_COUNT= i;
	m.DL_GRANT= eth_port->etp_osdep.etp_wr_vec_grant;
	m.m_type= DL_WRITEV_S;

	m.DL_PORT= eth_port->etp_osdep.etp_port;
	m.DL_PROC= this_proc;
	m.DL_MODE= DL_NOMODE;

	assert(eth_port->etp_osdep.etp_state == OEPS_IDLE);
	r= asynsend(eth_port->etp_osdep.etp_task, &m);

	if (r < 0)
	{
		printf("eth_issue_send: send to %d failed: %d\n",
			eth_port->etp_osdep.etp_task, r);
		return;
	}
	eth_port->etp_osdep.etp_state= OEPS_SEND_SENT;
}

PRIVATE void write_int(eth_port)
eth_port_t *eth_port;
{
	acc_t *pack;
	int multicast;
	u8_t *eth_dst_ptr;

	pack= eth_port->etp_wr_pack;
	if (pack == NULL)
	{
		printf("write_int: strange no packet on eth port %d\n",
			eth_port-eth_port_table);
		eth_restart_write(eth_port);
		return;
	}

	eth_port->etp_wr_pack= NULL;

	eth_dst_ptr= (u8_t *)ptr2acc_data(pack);
	multicast= (*eth_dst_ptr & 1);	/* low order bit indicates multicast */
	if (multicast || (eth_port->etp_osdep.etp_recvconf & NWEO_EN_PROMISC))
	{
		assert(!no_ethWritePort);
		no_ethWritePort= 1;
		eth_arrive(eth_port, pack, bf_bufsize(pack));
		assert(no_ethWritePort);
		no_ethWritePort= 0;
	}
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

	if (count < ETH_MIN_PACK_SIZE)
	{
		printf("mnx_eth`read_int: packet size too small (%d)\n",
			count);
		bf_afree(pack);
	}
	else if (count > ETH_MAX_PACK_SIZE_TAGGED)
	{
		printf("mnx_eth`read_int: packet size too big (%d)\n",
			count);
		bf_afree(pack);
	}
	else
	{
		cut_pack= bf_cut(pack, 0, count);
		bf_afree(pack);

		assert(!no_ethWritePort);
		no_ethWritePort= 1;
		eth_arrive(eth_port, cut_pack, count);
		assert(no_ethWritePort);
		no_ethWritePort= 0;
	}
	
	eth_port->etp_flags &= ~(EPF_READ_IP|EPF_READ_SP);
	setup_read(eth_port);
}

PRIVATE void setup_read(eth_port)
eth_port_t *eth_port;
{
	acc_t *pack, *pack_ptr;
	message mess1;
	iovec_s_t *iovec;
	int i, r;

	assert(!eth_port->etp_vlan);
	assert(!(eth_port->etp_flags & (EPF_READ_IP|EPF_READ_SP)));

	if (eth_port->etp_osdep.etp_state != OEPS_IDLE)
	{
		eth_port->etp_osdep.etp_flags |= OEPF_NEED_RECV;

		return;
	}

	assert (!eth_port->etp_rd_pack);

	iovec= eth_port->etp_osdep.etp_rd_iovec;
	pack= bf_memreq (ETH_MAX_PACK_SIZE_TAGGED);

	for (i=0, pack_ptr= pack; i<RD_IOVEC && pack_ptr;
		i++, pack_ptr= pack_ptr->acc_next)
	{
		r= cpf_setgrant_direct(iovec[i].iov_grant,
			eth_port->etp_osdep.etp_task,
			(vir_bytes)ptr2acc_data(pack_ptr),
			(vir_bytes)pack_ptr->acc_length,
			CPF_WRITE);
		if (r != 0)
		{
			ip_panic((
		"mnx_eth`setup_read: cpf_setgrant_direct failed: %d\n",
				errno));
		}
		iovec[i].iov_size= (vir_bytes)pack_ptr->acc_length;
	}
	assert (!pack_ptr);

	r= cpf_setgrant_direct(eth_port->etp_osdep.etp_rd_vec_grant,
		eth_port->etp_osdep.etp_task,
		(vir_bytes)iovec,
		(vir_bytes)(i * sizeof(iovec[0])),
		CPF_READ);
	if (r != 0)
	{
		ip_panic((
	"mnx_eth`setup_read: cpf_setgrant_direct failed: %d\n",
			errno));
	}

	mess1.m_type= DL_READV_S;
	mess1.DL_PORT= eth_port->etp_osdep.etp_port;
	mess1.DL_PROC= this_proc;
	mess1.DL_COUNT= i;
	mess1.DL_GRANT= eth_port->etp_osdep.etp_rd_vec_grant;

	assert(eth_port->etp_osdep.etp_state == OEPS_IDLE);

	r= asynsend(eth_port->etp_osdep.etp_task, &mess1);
	eth_port->etp_osdep.etp_state= OEPS_RECV_SENT;

	if (r < 0)
	{
		printf(
		"mnx_eth`setup_read: asynsend to %d failed: %d\n",
			eth_port->etp_osdep.etp_task, r);
	}
	eth_port->etp_rd_pack= pack;
	eth_port->etp_flags |= EPF_READ_IP;
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
	assert(eth_port->etp_osdep.etp_port == m_ptr->DL_PORT &&
		eth_port->etp_osdep.etp_task == m_ptr->m_source);

	assert(m_ptr->DL_STAT & DL_PACK_RECV);
	m_ptr->DL_STAT &= ~DL_PACK_RECV;

	if (recv_debug)
	{
		printf("eth_recvev: eth%d got DL_PACK_RECV\n", m_ptr->DL_PORT);
	}

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
	assert(eth_port->etp_osdep.etp_port == m_ptr->DL_PORT &&
		eth_port->etp_osdep.etp_task == m_ptr->m_source);

	assert(m_ptr->DL_STAT & DL_PACK_SEND);
	m_ptr->DL_STAT &= ~DL_PACK_SEND;
	assert(eth_port->etp_osdep.etp_send_ev);
	eth_port->etp_osdep.etp_send_ev= 0;

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
		if (loc_port->etp_osdep.etp_port == m->DL_PORT &&
			loc_port->etp_osdep.etp_task == m->m_source)
			break;
	}
	assert (i<eth_conf_nr);
	return loc_port;
}

static void eth_restart(eth_port, tasknr)
eth_port_t *eth_port;
int tasknr;
{
	int r;
	unsigned flags, dl_flags;
	cp_grant_id_t gid;
	message mess;

	if (eth_port->etp_osdep.etp_state != OEPS_INIT) {
		printf("eth_restart: restarting eth%d, task %d, port %d\n",
			eth_port-eth_port_table, tasknr,
			eth_port->etp_osdep.etp_port);
	}

	if (eth_port->etp_osdep.etp_task == tasknr)
	{
		printf(
		"eth_restart: task number did not change. Aborting restart\n");
		return;
	}
	eth_port->etp_osdep.etp_task= tasknr;

	switch(eth_port->etp_osdep.etp_state)
	{
	case OEPS_INIT:
	case OEPS_CONF_SENT:
	case OEPS_RECV_SENT:
	case OEPS_SEND_SENT:
		/* We can safely ignore the pending CONF, RECV, and SEND
		 * requests. If this is the first time that we see this
		 * driver at all, that's fine too.
		 */
		eth_port->etp_osdep.etp_state= OEPS_IDLE;
		break;
	case OEPS_GETSTAT_SENT:
		/* Set the OEPF_NEED_STAT to trigger a new request */
		eth_port->etp_osdep.etp_flags |= OEPF_NEED_STAT;
		eth_port->etp_osdep.etp_state= OEPS_IDLE;
		break;
	}

	/* If there is a pending GETSTAT request then we have to create a
	 * new grant.
	 */
	if (eth_port->etp_osdep.etp_flags & OEPF_NEED_STAT)
	{
		assert(eth_port->etp_osdep.etp_stat_gid != -1);
		cpf_revoke(eth_port->etp_osdep.etp_stat_gid);

		gid= cpf_grant_direct(eth_port->etp_osdep.etp_task,
		(vir_bytes)eth_port->etp_osdep.etp_stat_buf,
		sizeof(*eth_port->etp_osdep.etp_stat_buf), CPF_WRITE);
		if (gid == -1)
		{
			ip_panic((
				"eth_restart: cpf_grant_direct failed: %d\n",
				errno));
		}
		eth_port->etp_osdep.etp_stat_gid= gid;
	}

	flags= eth_port->etp_osdep.etp_recvconf;
	dl_flags= DL_NOMODE;
	if (flags & NWEO_EN_BROAD)
		dl_flags |= DL_BROAD_REQ;
	if (flags & NWEO_EN_MULTI)
		dl_flags |= DL_MULTI_REQ;
	if (flags & NWEO_EN_PROMISC)
		dl_flags |= DL_PROMISC_REQ;
	mess.m_type= DL_CONF;
	mess.DL_PORT= eth_port->etp_osdep.etp_port;
	mess.DL_PROC= this_proc;
	mess.DL_MODE= dl_flags;

	compare(eth_port->etp_osdep.etp_state, ==, OEPS_IDLE);
	r= asynsend(eth_port->etp_osdep.etp_task, &mess);
	if (r<0)
	{
		printf(
	"eth_restart: send to ethernet task %d failed: %d\n",
			eth_port->etp_osdep.etp_task, r);
		return;
	}
	eth_port->etp_osdep.etp_state= OEPS_CONF_SENT;

	if (eth_port->etp_wr_pack)
	{
		bf_afree(eth_port->etp_wr_pack);
		eth_port->etp_wr_pack= NULL;
		eth_restart_write(eth_port);
	}
	if (eth_port->etp_rd_pack)
	{
		bf_afree(eth_port->etp_rd_pack);
		eth_port->etp_rd_pack= NULL;
		eth_port->etp_flags &= ~(EPF_READ_IP|EPF_READ_SP);
	}

}

PRIVATE void send_getstat(eth_port)
eth_port_t *eth_port;
{
	int r;
	message mess;

	mess.m_type= DL_GETSTAT_S;
	mess.DL_PORT= eth_port->etp_osdep.etp_port;
	mess.DL_PROC= this_proc;
	mess.DL_GRANT= eth_port->etp_osdep.etp_stat_gid;

	assert(eth_port->etp_osdep.etp_state == OEPS_IDLE);

	r= asynsend(eth_port->etp_osdep.etp_task, &mess);
	eth_port->etp_osdep.etp_state= OEPS_GETSTAT_SENT;

	if (r != OK)
		ip_panic(( "eth_get_stat: asynsend failed: %d", r));
}

/*
 * $PchId: mnx_eth.c,v 1.16 2005/06/28 14:24:37 philip Exp $
 */
