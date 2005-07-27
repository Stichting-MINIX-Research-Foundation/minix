/*
udp.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "type.h"

#include "assert.h"
#include "buf.h"
#include "clock.h"
#include "icmp_lib.h"
#include "io.h"
#include "ip.h"
#include "sr.h"
#include "udp.h"
#include "udp_int.h"

THIS_FILE

FORWARD void read_ip_packets ARGS(( udp_port_t *udp_port ));
FORWARD void udp_buffree ARGS(( int priority ));
#ifdef BUF_CONSISTENCY_CHECK
FORWARD void udp_bufcheck ARGS(( void ));
#endif
FORWARD void udp_main ARGS(( udp_port_t *udp_port ));
FORWARD int udp_select ARGS(( int fd, unsigned operations ));
FORWARD acc_t *udp_get_data ARGS(( int fd, size_t offset, size_t count, 
	int for_ioctl ));
FORWARD int udp_put_data ARGS(( int fd, size_t offset, acc_t *data, 	
	int for_ioctl ));
FORWARD int udp_peek ARGS(( udp_fd_t * ));
FORWARD int udp_sel_read ARGS(( udp_fd_t * ));
FORWARD void udp_restart_write_port ARGS(( udp_port_t *udp_port ));
FORWARD void udp_ip_arrived ARGS(( int port, acc_t *pack, size_t pack_size ));
FORWARD void reply_thr_put ARGS(( udp_fd_t *udp_fd, int reply,
	int for_ioctl ));
FORWARD void reply_thr_get ARGS(( udp_fd_t *udp_fd, int reply,
	int for_ioctl ));
FORWARD int udp_setopt ARGS(( udp_fd_t *udp_fd ));
FORWARD udpport_t find_unused_port ARGS(( int fd ));
FORWARD int is_unused_port ARGS(( Udpport_t port ));
FORWARD int udp_packet2user ARGS(( udp_fd_t *udp_fd ));
FORWARD void restart_write_fd ARGS(( udp_fd_t *udp_fd ));
FORWARD u16_t pack_oneCsum ARGS(( acc_t *pack ));
FORWARD void udp_rd_enqueue ARGS(( udp_fd_t *udp_fd, acc_t *pack,
							clock_t exp_tim ));
FORWARD void hash_fd ARGS(( udp_fd_t *udp_fd ));
FORWARD void unhash_fd ARGS(( udp_fd_t *udp_fd ));

PUBLIC udp_port_t *udp_port_table;
PUBLIC udp_fd_t udp_fd_table[UDP_FD_NR];

PUBLIC void udp_prep()
{
	udp_port_table= alloc(udp_conf_nr * sizeof(udp_port_table[0]));
}

PUBLIC void udp_init()
{
	udp_fd_t *udp_fd;
	udp_port_t *udp_port;
	int i, j, ifno;

	assert (BUF_S >= sizeof(struct nwio_ipopt));
	assert (BUF_S >= sizeof(struct nwio_ipconf));
	assert (BUF_S >= sizeof(struct nwio_udpopt));
	assert (BUF_S >= sizeof(struct udp_io_hdr));
	assert (UDP_HDR_SIZE == sizeof(udp_hdr_t));
	assert (UDP_IO_HDR_SIZE == sizeof(udp_io_hdr_t));

	for (i= 0, udp_fd= udp_fd_table; i<UDP_FD_NR; i++, udp_fd++)
	{
		udp_fd->uf_flags= UFF_EMPTY;
		udp_fd->uf_rdbuf_head= NULL;
	}

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(udp_buffree);
#else
	bf_logon(udp_buffree, udp_bufcheck);
#endif

	for (i= 0, udp_port= udp_port_table; i<udp_conf_nr; i++, udp_port++)
	{
		udp_port->up_ipdev= udp_conf[i].uc_port;

		udp_port->up_flags= UPF_EMPTY;
		udp_port->up_state= UPS_EMPTY;
		udp_port->up_next_fd= udp_fd_table;
		udp_port->up_write_fd= NULL;
		udp_port->up_wr_pack= NULL;
		udp_port->up_port_any= NULL;
		for (j= 0; j<UDP_PORT_HASH_NR; j++)
			udp_port->up_port_hash[j]= NULL;

		ifno= ip_conf[udp_port->up_ipdev].ic_ifno;
		sr_add_minor(if2minor(ifno, UDP_DEV_OFF),
			i, udp_open, udp_close, udp_read,
			udp_write, udp_ioctl, udp_cancel, udp_select);

		udp_main(udp_port);
	}
}

PUBLIC int udp_open (port, srfd, get_userdata, put_userdata, put_pkt,
	select_res)
int port;
int srfd;
get_userdata_t get_userdata;
put_userdata_t put_userdata;
put_pkt_t put_pkt;
select_res_t select_res;
{
	int i;
	udp_fd_t *udp_fd;

	for (i= 0; i<UDP_FD_NR && (udp_fd_table[i].uf_flags & UFF_INUSE);
		i++);

	if (i>= UDP_FD_NR)
	{
		DBLOCK(1, printf("out of fds\n"));
		return EAGAIN;
	}

	udp_fd= &udp_fd_table[i];

	udp_fd->uf_flags= UFF_INUSE;
	udp_fd->uf_port= &udp_port_table[port];
	udp_fd->uf_srfd= srfd;
	udp_fd->uf_udpopt.nwuo_flags= UDP_DEF_OPT;
	udp_fd->uf_get_userdata= get_userdata;
	udp_fd->uf_put_userdata= put_userdata;
	udp_fd->uf_select_res= select_res;
	assert(udp_fd->uf_rdbuf_head == NULL);
	udp_fd->uf_port_next= NULL;

	return i;

}

PUBLIC int udp_ioctl (fd, req)
int fd;
ioreq_t req;
{
	udp_fd_t *udp_fd;
	udp_port_t *udp_port;
	nwio_udpopt_t *udp_opt;
	acc_t *opt_acc;
	int result;

	udp_fd= &udp_fd_table[fd];

assert (udp_fd->uf_flags & UFF_INUSE);

	udp_port= udp_fd->uf_port;
	udp_fd->uf_flags |= UFF_IOCTL_IP;
	udp_fd->uf_ioreq= req;

	if (udp_port->up_state != UPS_MAIN)
		return NW_SUSPEND;

	switch(req)
	{
	case NWIOSUDPOPT:
		result= udp_setopt(udp_fd);
		break;
	case NWIOGUDPOPT:
		opt_acc= bf_memreq(sizeof(*udp_opt));
assert (opt_acc->acc_length == sizeof(*udp_opt));
		udp_opt= (nwio_udpopt_t *)ptr2acc_data(opt_acc);

		*udp_opt= udp_fd->uf_udpopt;
		udp_opt->nwuo_locaddr= udp_fd->uf_port->up_ipaddr;
		result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd, 0, opt_acc,
			TRUE);
		if (result == NW_OK)
			reply_thr_put(udp_fd, NW_OK, TRUE);
		break;
	case NWIOUDPPEEK:
		result= udp_peek(udp_fd);
		break;
	default:
		reply_thr_get(udp_fd, EBADIOCTL, TRUE);
		result= NW_OK;
		break;
	}
	if (result != NW_SUSPEND)
		udp_fd->uf_flags &= ~UFF_IOCTL_IP;
	return result;
}

PUBLIC int udp_read (fd, count)
int fd;
size_t count;
{
	udp_fd_t *udp_fd;
	acc_t *tmp_acc, *next_acc;

	udp_fd= &udp_fd_table[fd];
	if (!(udp_fd->uf_flags & UFF_OPTSET))
	{
		reply_thr_put(udp_fd, EBADMODE, FALSE);
		return NW_OK;
	}

	udp_fd->uf_rd_count= count;

	if (udp_fd->uf_rdbuf_head)
	{
		if (get_time() <= udp_fd->uf_exp_tim)
			return udp_packet2user (udp_fd);
		tmp_acc= udp_fd->uf_rdbuf_head;
		while (tmp_acc)
		{
			next_acc= tmp_acc->acc_ext_link;
			bf_afree(tmp_acc);
			tmp_acc= next_acc;
		}
		udp_fd->uf_rdbuf_head= NULL;
	}
	udp_fd->uf_flags |= UFF_READ_IP;
	return NW_SUSPEND;
}

PRIVATE void udp_main(udp_port)
udp_port_t *udp_port;
{
	udp_fd_t *udp_fd;
	int result, i;

	switch (udp_port->up_state)
	{
	case UPS_EMPTY:
		udp_port->up_state= UPS_SETPROTO;

		udp_port->up_ipfd= ip_open(udp_port->up_ipdev, 
			udp_port->up_ipdev, udp_get_data, udp_put_data,
			udp_ip_arrived, 0 /* no select_res */);
		if (udp_port->up_ipfd < 0)
		{
			udp_port->up_state= UPS_ERROR;
			DBLOCK(1, printf("%s, %d: unable to open ip port\n",
				__FILE__, __LINE__));
			return;
		}

		result= ip_ioctl(udp_port->up_ipfd, NWIOSIPOPT);
		if (result == NW_SUSPEND)
			udp_port->up_flags |= UPF_SUSPEND;
		if (result<0)
		{
			return;
		}
		if (udp_port->up_state != UPS_GETCONF)
			return;
		/* drops through */
	case UPS_GETCONF:
		udp_port->up_flags &= ~UPF_SUSPEND;

		result= ip_ioctl(udp_port->up_ipfd, NWIOGIPCONF);
		if (result == NW_SUSPEND)
			udp_port->up_flags |= UPF_SUSPEND;
		if (result<0)
		{
			return;
		}
		if (udp_port->up_state != UPS_MAIN)
			return;
		/* drops through */
	case UPS_MAIN:
		udp_port->up_flags &= ~UPF_SUSPEND;

		for (i= 0, udp_fd= udp_fd_table; i<UDP_FD_NR; i++, udp_fd++)
		{
			if (!(udp_fd->uf_flags & UFF_INUSE))
				continue;
			if (udp_fd->uf_port != udp_port)
				continue;
			if (udp_fd->uf_flags & UFF_IOCTL_IP)
				udp_ioctl(i, udp_fd->uf_ioreq);
		}
		read_ip_packets(udp_port);
		return;
	default:
		DBLOCK(1, printf("udp_port_table[%d].up_state= %d\n",
			udp_port->up_ipdev, udp_port->up_state));
		ip_panic(( "unknown state" ));
		break;
	}
}

PRIVATE int udp_select(fd, operations)
int fd;
unsigned operations;
{
	int i;
	unsigned resops;
	udp_fd_t *udp_fd;

	udp_fd= &udp_fd_table[fd];
	assert (udp_fd->uf_flags & UFF_INUSE);

	resops= 0;

	if (operations & SR_SELECT_READ)
	{
		if (udp_sel_read(udp_fd))
			resops |= SR_SELECT_READ;
		else if (!(operations & SR_SELECT_POLL))
			udp_fd->uf_flags |= UFF_SEL_READ;
	}
	if (operations & SR_SELECT_WRITE)
	{
		/* Should handle special case when the interface is down */
		resops |= SR_SELECT_WRITE;
	}
	if (operations & SR_SELECT_EXCEPTION)
	{
		printf("udp_select: not implemented for exceptions\n");
	}
	return resops;
}

PRIVATE acc_t *udp_get_data (port, offset, count, for_ioctl)
int port;
size_t offset;
size_t count;
int for_ioctl;
{
	udp_port_t *udp_port;
	udp_fd_t *udp_fd;
	int result;

	udp_port= &udp_port_table[port];

	switch(udp_port->up_state)
	{
	case UPS_SETPROTO:
assert (for_ioctl);
		if (!count)
		{
			result= (int)offset;
			if (result<0)
			{
				udp_port->up_state= UPS_ERROR;
				break;
			}
			udp_port->up_state= UPS_GETCONF;
			if (udp_port->up_flags & UPF_SUSPEND)
				udp_main(udp_port);
			return NULL;
		}
		else
		{
			struct nwio_ipopt *ipopt;
			acc_t *acc;

assert (!offset);
assert (count == sizeof(*ipopt));

			acc= bf_memreq(sizeof(*ipopt));
			ipopt= (struct nwio_ipopt *)ptr2acc_data(acc);
			ipopt->nwio_flags= NWIO_COPY | NWIO_EN_LOC | 
				NWIO_EN_BROAD | NWIO_REMANY | NWIO_PROTOSPEC |
				NWIO_HDR_O_ANY | NWIO_RWDATALL;
			ipopt->nwio_proto= IPPROTO_UDP;
			return acc;
		}
	case UPS_MAIN:
assert (!for_ioctl);
assert (udp_port->up_flags & UPF_WRITE_IP);
		if (!count)
		{
			result= (int)offset;
assert (udp_port->up_wr_pack);
			bf_afree(udp_port->up_wr_pack);
			udp_port->up_wr_pack= 0;
			if (udp_port->up_flags & UPF_WRITE_SP)
			{
				if (udp_port->up_write_fd)
				{
					udp_fd= udp_port->up_write_fd;
					udp_port->up_write_fd= NULL;
					udp_fd->uf_flags &= ~UFF_WRITE_IP;
					reply_thr_get(udp_fd, result, FALSE);
				}
				udp_port->up_flags &= ~(UPF_WRITE_SP | 
					UPF_WRITE_IP);
				if (udp_port->up_flags & UPF_MORE2WRITE)
				{
					udp_restart_write_port(udp_port);
				}
			}
			else
				udp_port->up_flags &= ~UPF_WRITE_IP;
		}
		else
		{
			return bf_cut (udp_port->up_wr_pack, offset, count);
		}
		break;
	default:
		printf("udp_get_data(%d, 0x%x, 0x%x) called but up_state= 0x%x\n",
			port, offset, count, udp_port->up_state);
		break;
	}
	return NULL;
}

PRIVATE int udp_put_data (fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	udp_port_t *udp_port;
	int result;

	udp_port= &udp_port_table[fd];

	switch (udp_port->up_state)
	{
	case UPS_GETCONF:
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				udp_port->up_state= UPS_ERROR;
				return NW_OK;
			}
			udp_port->up_state= UPS_MAIN;
			if (udp_port->up_flags & UPF_SUSPEND)
				udp_main(udp_port);
		}
		else
		{
			struct nwio_ipconf *ipconf;

			data= bf_packIffLess(data, sizeof(*ipconf));
			ipconf= (struct nwio_ipconf *)ptr2acc_data(data);
assert (ipconf->nwic_flags & NWIC_IPADDR_SET);
			udp_port->up_ipaddr= ipconf->nwic_ipaddr;
			bf_afree(data);
		}
		break;
	case UPS_MAIN:
		assert(0);

		assert (udp_port->up_flags & UPF_READ_IP);
		if (!data)
		{
			result= (int)offset;
			compare (result, >=, 0);
			if (udp_port->up_flags & UPF_READ_SP)
			{
				udp_port->up_flags &= ~(UPF_READ_SP|
					UPF_READ_IP);
				read_ip_packets(udp_port);
			}
			else
				udp_port->up_flags &= ~UPF_READ_IP;
		}
		else
		{
assert (!offset);	/* This isn't a valid assertion but ip sends only
			 * whole datagrams up */
			udp_ip_arrived(fd, data, bf_bufsize(data));
		}
		break;
	default:
		ip_panic((
		"udp_put_data(%d, 0x%x, %p) called but up_state= 0x%x\n",
					fd, offset, data, udp_port->up_state ));
	}
	return NW_OK;
}

PRIVATE int udp_setopt(udp_fd)
udp_fd_t *udp_fd;
{
	udp_fd_t *fd_ptr;
	nwio_udpopt_t oldopt, newopt;
	acc_t *data;
	unsigned int new_en_flags, new_di_flags, old_en_flags, old_di_flags,
		all_flags, flags;
	unsigned long new_flags;
	int i;

	data= (*udp_fd->uf_get_userdata)(udp_fd->uf_srfd, 0, 
		sizeof(nwio_udpopt_t), TRUE);

	if (!data)
		return EFAULT;

	data= bf_packIffLess(data, sizeof(nwio_udpopt_t));
assert (data->acc_length == sizeof(nwio_udpopt_t));

	newopt= *(nwio_udpopt_t *)ptr2acc_data(data);
	bf_afree(data);
	oldopt= udp_fd->uf_udpopt;

	old_en_flags= oldopt.nwuo_flags & 0xffff;
	old_di_flags= (oldopt.nwuo_flags >> 16) & 0xffff;

	new_en_flags= newopt.nwuo_flags & 0xffff;
	new_di_flags= (newopt.nwuo_flags >> 16) & 0xffff;

	if (new_en_flags & new_di_flags)
	{
		DBLOCK(1, printf("returning EBADMODE\n"));

		reply_thr_get(udp_fd, EBADMODE, TRUE);
		return NW_OK;
	}

	/* NWUO_ACC_MASK */
	if (new_di_flags & NWUO_ACC_MASK)
	{
		DBLOCK(1, printf("returning EBADMODE\n"));

		reply_thr_get(udp_fd, EBADMODE, TRUE);
		return NW_OK;
		/* access modes can't be disabled */
	}

	if (!(new_en_flags & NWUO_ACC_MASK))
		new_en_flags |= (old_en_flags & NWUO_ACC_MASK);

	/* NWUO_LOCPORT_MASK */
	if (new_di_flags & NWUO_LOCPORT_MASK)
	{
		DBLOCK(1, printf("returning EBADMODE\n"));

		reply_thr_get(udp_fd, EBADMODE, TRUE);
		return NW_OK;
		/* the loc ports can't be disabled */
	}
	if (!(new_en_flags & NWUO_LOCPORT_MASK))
	{
		new_en_flags |= (old_en_flags & NWUO_LOCPORT_MASK);
		newopt.nwuo_locport= oldopt.nwuo_locport;
	}
	else if ((new_en_flags & NWUO_LOCPORT_MASK) == NWUO_LP_SEL)
	{
		newopt.nwuo_locport= find_unused_port(udp_fd-udp_fd_table);
	}
	else if ((new_en_flags & NWUO_LOCPORT_MASK) == NWUO_LP_SET)
	{
		if (!newopt.nwuo_locport)
		{
			DBLOCK(1, printf("returning EBADMODE\n"));

			reply_thr_get(udp_fd, EBADMODE, TRUE);
			return NW_OK;
		}
	}

	/* NWUO_LOCADDR_MASK */
	if (!((new_en_flags | new_di_flags) & NWUO_LOCADDR_MASK))
	{
		new_en_flags |= (old_en_flags & NWUO_LOCADDR_MASK);
		new_di_flags |= (old_di_flags & NWUO_LOCADDR_MASK);
	}

	/* NWUO_BROAD_MASK */
	if (!((new_en_flags | new_di_flags) & NWUO_BROAD_MASK))
	{
		new_en_flags |= (old_en_flags & NWUO_BROAD_MASK);
		new_di_flags |= (old_di_flags & NWUO_BROAD_MASK);
	}

	/* NWUO_REMPORT_MASK */
	if (!((new_en_flags | new_di_flags) & NWUO_REMPORT_MASK))
	{
		new_en_flags |= (old_en_flags & NWUO_REMPORT_MASK);
		new_di_flags |= (old_di_flags & NWUO_REMPORT_MASK);
		newopt.nwuo_remport= oldopt.nwuo_remport;
	}
	
	/* NWUO_REMADDR_MASK */
	if (!((new_en_flags | new_di_flags) & NWUO_REMADDR_MASK))
	{
		new_en_flags |= (old_en_flags & NWUO_REMADDR_MASK);
		new_di_flags |= (old_di_flags & NWUO_REMADDR_MASK);
		newopt.nwuo_remaddr= oldopt.nwuo_remaddr;
	}

	/* NWUO_RW_MASK */
	if (!((new_en_flags | new_di_flags) & NWUO_RW_MASK))
	{
		new_en_flags |= (old_en_flags & NWUO_RW_MASK);
		new_di_flags |= (old_di_flags & NWUO_RW_MASK);
	}

	/* NWUO_IPOPT_MASK */
	if (!((new_en_flags | new_di_flags) & NWUO_IPOPT_MASK))
	{
		new_en_flags |= (old_en_flags & NWUO_IPOPT_MASK);
		new_di_flags |= (old_di_flags & NWUO_IPOPT_MASK);
	}

	new_flags= ((unsigned long)new_di_flags << 16) | new_en_flags;
	if ((new_flags & NWUO_RWDATONLY) && 
		((new_flags & NWUO_LOCPORT_MASK) == NWUO_LP_ANY || 
		(new_flags & (NWUO_RP_ANY|NWUO_RA_ANY|NWUO_EN_IPOPT))))
	{
		DBLOCK(1, printf("returning EBADMODE\n"));

		reply_thr_get(udp_fd, EBADMODE, TRUE);
		return NW_OK;
	}

	/* Check the access modes */
	if ((new_flags & NWUO_LOCPORT_MASK) == NWUO_LP_SEL ||
		(new_flags & NWUO_LOCPORT_MASK) == NWUO_LP_SET)
	{
		for (i= 0, fd_ptr= udp_fd_table; i<UDP_FD_NR; i++, fd_ptr++)
		{
			if (fd_ptr == udp_fd)
				continue;
			if (!(fd_ptr->uf_flags & UFF_INUSE))
				continue;
			if (fd_ptr->uf_port != udp_fd->uf_port)
				continue;
			flags= fd_ptr->uf_udpopt.nwuo_flags;
			if ((flags & NWUO_LOCPORT_MASK) != NWUO_LP_SEL &&
				(flags & NWUO_LOCPORT_MASK) != NWUO_LP_SET)
				continue;
			if (fd_ptr->uf_udpopt.nwuo_locport !=
				newopt.nwuo_locport)
			{
				continue;
			}
			if ((flags & NWUO_ACC_MASK) != 
				(new_flags & NWUO_ACC_MASK))
			{
				DBLOCK(1, printf(
			"address inuse: new fd= %d, old_fd= %d, port= %u\n",
					udp_fd-udp_fd_table,
					fd_ptr-udp_fd_table,
					newopt.nwuo_locport));

				reply_thr_get(udp_fd, EADDRINUSE, TRUE);
				return NW_OK;
			}
		}
	}

	if (udp_fd->uf_flags & UFF_OPTSET)
		unhash_fd(udp_fd);

	newopt.nwuo_flags= new_flags;
	udp_fd->uf_udpopt= newopt;

	all_flags= new_en_flags | new_di_flags;
	if ((all_flags & NWUO_ACC_MASK) && (all_flags & NWUO_LOCPORT_MASK) &&
		(all_flags & NWUO_LOCADDR_MASK) &&
		(all_flags & NWUO_BROAD_MASK) &&
		(all_flags & NWUO_REMPORT_MASK) &&
		(all_flags & NWUO_REMADDR_MASK) &&
		(all_flags & NWUO_RW_MASK) &&
		(all_flags & NWUO_IPOPT_MASK))
		udp_fd->uf_flags |= UFF_OPTSET;
	else
	{
		udp_fd->uf_flags &= ~UFF_OPTSET;
	}

	if (udp_fd->uf_flags & UFF_OPTSET)
		hash_fd(udp_fd);

	reply_thr_get(udp_fd, NW_OK, TRUE);
	return NW_OK;
}

PRIVATE udpport_t find_unused_port(fd)
int fd;
{
	udpport_t port, nw_port;

	for (port= 0x8000+fd; port < 0xffff-UDP_FD_NR; port+= UDP_FD_NR)
	{
		nw_port= htons(port);
		if (is_unused_port(nw_port))
			return nw_port;
	}
	for (port= 0x8000; port < 0xffff; port++)
	{
		nw_port= htons(port);
		if (is_unused_port(nw_port))
			return nw_port;
	}
	ip_panic(( "unable to find unused port (shouldn't occur)" ));
	return 0;
}

/*
reply_thr_put
*/

PRIVATE void reply_thr_put(udp_fd, reply, for_ioctl)
udp_fd_t *udp_fd;
int reply;
int for_ioctl;
{
	int result;

	result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd, reply,
		(acc_t *)0, for_ioctl);
	assert(result == NW_OK);
}

/*
reply_thr_get
*/

PRIVATE void reply_thr_get(udp_fd, reply, for_ioctl)
udp_fd_t *udp_fd;
int reply;
int for_ioctl;
{
	acc_t *result;
	result= (*udp_fd->uf_get_userdata)(udp_fd->uf_srfd, reply,
		(size_t)0, for_ioctl);
	assert (!result);
}

PRIVATE int is_unused_port(port)
udpport_t port;
{
	int i;
	udp_fd_t *udp_fd;

	for (i= 0, udp_fd= udp_fd_table; i<UDP_FD_NR; i++,
		udp_fd++)
	{
		if (!(udp_fd->uf_flags & UFF_OPTSET))
			continue;
		if (udp_fd->uf_udpopt.nwuo_locport == port)
			return FALSE;
	}
	return TRUE;
}

PRIVATE void read_ip_packets(udp_port)
udp_port_t *udp_port;
{
	int result;

	do
	{
		udp_port->up_flags |= UPF_READ_IP;
		result= ip_read(udp_port->up_ipfd, UDP_MAX_DATAGRAM);
		if (result == NW_SUSPEND)
		{
			udp_port->up_flags |= UPF_READ_SP;
			return;
		}
assert(result == NW_OK);
		udp_port->up_flags &= ~UPF_READ_IP;
	} while(!(udp_port->up_flags & UPF_READ_IP));
}


PRIVATE int udp_peek (udp_fd)
udp_fd_t *udp_fd;
{
	acc_t *pack, *tmp_acc, *next_acc;
	int result;

	if (!(udp_fd->uf_flags & UFF_OPTSET))
	{
		udp_fd->uf_flags &= ~UFF_IOCTL_IP;
		reply_thr_put(udp_fd, EBADMODE, TRUE);
		return NW_OK;
	}

	if (udp_fd->uf_rdbuf_head)
	{
		if (get_time() <= udp_fd->uf_exp_tim)
		{
			pack= bf_cut(udp_fd->uf_rdbuf_head, 0,
				sizeof(udp_io_hdr_t));
			result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd,
				(size_t)0, pack, TRUE);

			udp_fd->uf_flags &= ~UFF_IOCTL_IP;
			result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd,
				result, (acc_t *)0, TRUE);
			assert (result == 0);
			return result;
		}
		tmp_acc= udp_fd->uf_rdbuf_head;
		while (tmp_acc)
		{
			next_acc= tmp_acc->acc_ext_link;
			bf_afree(tmp_acc);
			tmp_acc= next_acc;
		}
		udp_fd->uf_rdbuf_head= NULL;
	}
	udp_fd->uf_flags |= UFF_PEEK_IP;
	return NW_SUSPEND;
}

PRIVATE int udp_sel_read (udp_fd)
udp_fd_t *udp_fd;
{
	acc_t *pack, *tmp_acc, *next_acc;
	int result;

	if (!(udp_fd->uf_flags & UFF_OPTSET))
		return 1;	/* Read will not block */

	if (udp_fd->uf_rdbuf_head)
	{
		if (get_time() <= udp_fd->uf_exp_tim)
			return 1;
		
		tmp_acc= udp_fd->uf_rdbuf_head;
		while (tmp_acc)
		{
			next_acc= tmp_acc->acc_ext_link;
			bf_afree(tmp_acc);
			tmp_acc= next_acc;
		}
		udp_fd->uf_rdbuf_head= NULL;
	}
	return 0;
}

PRIVATE int udp_packet2user (udp_fd)
udp_fd_t *udp_fd;
{
	acc_t *pack, *tmp_pack;
	udp_io_hdr_t *hdr;
	int result, hdr_len;
	size_t size, transf_size;

	pack= udp_fd->uf_rdbuf_head;
	udp_fd->uf_rdbuf_head= pack->acc_ext_link;

	size= bf_bufsize (pack);

	if (udp_fd->uf_udpopt.nwuo_flags & NWUO_RWDATONLY)
	{

		pack= bf_packIffLess (pack, UDP_IO_HDR_SIZE);
		assert (pack->acc_length >= UDP_IO_HDR_SIZE);

		hdr= (udp_io_hdr_t *)ptr2acc_data(pack);
#if CONF_UDP_IO_NW_BYTE_ORDER
		hdr_len= UDP_IO_HDR_SIZE+NTOHS(hdr->uih_ip_opt_len);
#else
		hdr_len= UDP_IO_HDR_SIZE+hdr->uih_ip_opt_len;
#endif

		assert (size>= hdr_len);
		size -= hdr_len;
		tmp_pack= bf_cut(pack, hdr_len, size);
		bf_afree(pack);
		pack= tmp_pack;
	}

	if (size>udp_fd->uf_rd_count)
	{
		tmp_pack= bf_cut (pack, 0, udp_fd->uf_rd_count);
		bf_afree(pack);
		pack= tmp_pack;
		transf_size= udp_fd->uf_rd_count;
	}
	else
		transf_size= size;

	result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd,
		(size_t)0, pack, FALSE);

	if (result >= 0)
		if (size > transf_size)
			result= EPACKSIZE;
		else
			result= transf_size;

	udp_fd->uf_flags &= ~UFF_READ_IP;
	result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd, result,
			(acc_t *)0, FALSE);
	assert (result == 0);

	return result;
}

PRIVATE void udp_ip_arrived(port, pack, pack_size)
int port;
acc_t *pack;
size_t pack_size;
{
	udp_port_t *udp_port;
	udp_fd_t *udp_fd, *share_fd;
	acc_t *ip_hdr_acc, *udp_acc, *ipopt_pack, *no_ipopt_pack, *tmp_acc;
	ip_hdr_t *ip_hdr;
	udp_hdr_t *udp_hdr;
	udp_io_hdr_t *udp_io_hdr;
	size_t ip_hdr_size, udp_size, data_size, opt_size;
	ipaddr_t src_addr, dst_addr, ipaddr;
	udpport_t src_port, dst_port;
	u8_t u16[2];
	u16_t chksum;
	unsigned long dst_type, flags;
	clock_t exp_tim;
	int i, delivered, hash;

	udp_port= &udp_port_table[port];

	ip_hdr_acc= bf_cut(pack, 0, IP_MIN_HDR_SIZE);
	ip_hdr_acc= bf_packIffLess(ip_hdr_acc, IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(ip_hdr_acc);
	ip_hdr_size= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	if (ip_hdr_size != IP_MIN_HDR_SIZE)
	{
		bf_afree(ip_hdr_acc);
		ip_hdr_acc= bf_cut(pack, 0, ip_hdr_size);
		ip_hdr_acc= bf_packIffLess(ip_hdr_acc, ip_hdr_size);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(ip_hdr_acc);
	}

	pack_size -= ip_hdr_size;
	if (pack_size < UDP_HDR_SIZE)
	{
		if (pack_size == 0 && ip_hdr->ih_proto == 0)
		{
			/* IP layer reports new IP address */
			ipaddr= ip_hdr->ih_src;
			udp_port->up_ipaddr= ipaddr;
			DBLOCK(1, printf("udp_ip_arrived: using address ");
				writeIpAddr(ipaddr); printf("\n"));
		}
		else
			DBLOCK(1, printf("packet too small\n"));

		bf_afree(ip_hdr_acc);
		bf_afree(pack);
		return;
	}

	udp_acc= bf_delhead(pack, ip_hdr_size);
	pack= NULL;


	udp_acc= bf_packIffLess(udp_acc, UDP_HDR_SIZE);
	udp_hdr= (udp_hdr_t *)ptr2acc_data(udp_acc);
	udp_size= ntohs(udp_hdr->uh_length);
	if (udp_size > pack_size)
	{
		DBLOCK(1, printf("packet too large\n"));

		bf_afree(ip_hdr_acc);
		bf_afree(udp_acc);
		return;
	}

	src_addr= ip_hdr->ih_src;
	dst_addr= ip_hdr->ih_dst;

	if (udp_hdr->uh_chksum)
	{
		u16[0]= 0;
		u16[1]= ip_hdr->ih_proto;
		chksum= pack_oneCsum(udp_acc);
		chksum= oneC_sum(chksum, (u16_t *)&src_addr, sizeof(ipaddr_t));
		chksum= oneC_sum(chksum, (u16_t *)&dst_addr, sizeof(ipaddr_t));
		chksum= oneC_sum(chksum, (u16_t *)u16, sizeof(u16));
		chksum= oneC_sum(chksum, (u16_t *)&udp_hdr->uh_length, 
			sizeof(udp_hdr->uh_length));
		if (~chksum & 0xffff)
		{
			DBLOCK(1, printf("checksum error in udp packet\n");
				printf("src ip_addr= ");
				writeIpAddr(src_addr);
				printf(" dst ip_addr= ");
				writeIpAddr(dst_addr);
				printf("\n");
				printf("packet chksum= 0x%x, sum= 0x%x\n",
					udp_hdr->uh_chksum, chksum));

			bf_afree(ip_hdr_acc);
			bf_afree(udp_acc);
			return;
		}
	}

	exp_tim= get_time() + UDP_READ_EXP_TIME;
	src_port= udp_hdr->uh_src_port;
	dst_port= udp_hdr->uh_dst_port;

	/* Send an ICMP port unreachable if the packet could not be
	 * delivered.
	 */
	delivered= 0;

	if (dst_addr == udp_port->up_ipaddr)
		dst_type= NWUO_EN_LOC;
	else
	{
		dst_type= NWUO_EN_BROAD;

		/* Don't send ICMP error packets for broadcast packets */
		delivered= 1;
	}

	DBLOCK(0x20, printf("udp: got packet from ");
		writeIpAddr(src_addr);
		printf(".%u to ", ntohs(src_port));
		writeIpAddr(dst_addr);
		printf(".%u\n", ntohs(dst_port)));

	no_ipopt_pack= bf_memreq(UDP_IO_HDR_SIZE);
	udp_io_hdr= (udp_io_hdr_t *)ptr2acc_data(no_ipopt_pack);
	udp_io_hdr->uih_src_addr= src_addr;
	udp_io_hdr->uih_dst_addr= dst_addr;
	udp_io_hdr->uih_src_port= src_port;
	udp_io_hdr->uih_dst_port= dst_port;
	data_size = udp_size-UDP_HDR_SIZE;
#if CONF_UDP_IO_NW_BYTE_ORDER
	udp_io_hdr->uih_ip_opt_len= HTONS(0);
	udp_io_hdr->uih_data_len= htons(data_size);
#else
	udp_io_hdr->uih_ip_opt_len= 0;
	udp_io_hdr->uih_data_len= data_size;
#endif
	no_ipopt_pack->acc_next= bf_cut(udp_acc, UDP_HDR_SIZE, data_size);

	if (ip_hdr_size == IP_MIN_HDR_SIZE)
	{
		ipopt_pack= no_ipopt_pack;
		ipopt_pack->acc_linkC++;
	}
	else
	{
		ipopt_pack= bf_memreq(UDP_IO_HDR_SIZE);
		*(udp_io_hdr_t *)ptr2acc_data(ipopt_pack)= *udp_io_hdr;
		udp_io_hdr= (udp_io_hdr_t *)ptr2acc_data(ipopt_pack);
		opt_size = ip_hdr_size-IP_MIN_HDR_SIZE;
#if CONF_UDP_IO_NW_BYTE_ORDER
		udp_io_hdr->uih_ip_opt_len= htons(opt_size);
#else
		udp_io_hdr->uih_ip_opt_len= opt_size;
#endif
		tmp_acc= bf_cut(ip_hdr_acc, (size_t)IP_MIN_HDR_SIZE, opt_size);
		assert(tmp_acc->acc_linkC == 1);
		assert(tmp_acc->acc_next == NULL);
		ipopt_pack->acc_next= tmp_acc;

		tmp_acc->acc_next= no_ipopt_pack->acc_next;
		if (tmp_acc->acc_next)
			tmp_acc->acc_next->acc_linkC++;
	}

	hash= dst_port;
	hash ^= (hash >> 8);
	hash &= (UDP_PORT_HASH_NR-1);

	for (i= 0; i<2; i++)
	{
		share_fd= NULL;

		udp_fd= (i == 0) ? udp_port->up_port_any :
			udp_port->up_port_hash[hash];
		for (; udp_fd; udp_fd= udp_fd->uf_port_next)
		{
			if (i && udp_fd->uf_udpopt.nwuo_locport != dst_port)
				continue;
		
			assert(udp_fd->uf_flags & UFF_INUSE);
			assert(udp_fd->uf_flags & UFF_OPTSET);
		
			if (udp_fd->uf_port != udp_port)
				continue;

			flags= udp_fd->uf_udpopt.nwuo_flags;
			if (!(flags & dst_type))
				continue;

			if ((flags & NWUO_RP_SET) && 
				udp_fd->uf_udpopt.nwuo_remport != src_port)
			{
				continue;
			}

			if ((flags & NWUO_RA_SET) && 
				udp_fd->uf_udpopt.nwuo_remaddr != src_addr)
			{
				continue;
			}

			if (i)
			{
				/* Packet is considdered to be delivered */
				delivered= 1;
			}

			if ((flags & NWUO_ACC_MASK) == NWUO_SHARED &&
				(!share_fd || !udp_fd->uf_rdbuf_head))
			{
				share_fd= udp_fd;
				continue;
			}

			if (flags & NWUO_EN_IPOPT)
				pack= ipopt_pack;
			else
				pack= no_ipopt_pack;

			pack->acc_linkC++;
			udp_rd_enqueue(udp_fd, pack, exp_tim);
			if (udp_fd->uf_flags & UFF_READ_IP)
				udp_packet2user(udp_fd);
		}

		if (share_fd)
		{
			flags= share_fd->uf_udpopt.nwuo_flags;
			if (flags & NWUO_EN_IPOPT)
				pack= ipopt_pack;
			else
				pack= no_ipopt_pack;

			pack->acc_linkC++;
			udp_rd_enqueue(share_fd, pack, exp_tim);
			if (share_fd->uf_flags & UFF_READ_IP)
				udp_packet2user(share_fd);
		}
	}

	if (ipopt_pack)
		bf_afree(ipopt_pack);
	if (no_ipopt_pack)
		bf_afree(no_ipopt_pack);

	if (!delivered)
	{
		DBLOCK(0x2, printf("udp: could not deliver packet from ");
			writeIpAddr(src_addr);
			printf(".%u to ", ntohs(src_port));
			writeIpAddr(dst_addr);
			printf(".%u\n", ntohs(dst_port)));

		pack= bf_append(ip_hdr_acc, udp_acc);
		ip_hdr_acc= NULL;
		udp_acc= NULL;
		icmp_snd_unreachable(udp_port->up_ipdev, pack,
			ICMP_PORT_UNRCH);
		return;
	}

	assert (ip_hdr_acc);
	bf_afree(ip_hdr_acc);
	assert (udp_acc);
	bf_afree(udp_acc);
}

PUBLIC void udp_close(fd)
int fd;
{
	udp_fd_t *udp_fd;
	acc_t *tmp_acc, *next_acc;

	udp_fd= &udp_fd_table[fd];

	assert (udp_fd->uf_flags & UFF_INUSE);

	if (udp_fd->uf_flags & UFF_OPTSET)
		unhash_fd(udp_fd);

	udp_fd->uf_flags= UFF_EMPTY;
	tmp_acc= udp_fd->uf_rdbuf_head;
	while (tmp_acc)
	{
		next_acc= tmp_acc->acc_ext_link;
		bf_afree(tmp_acc);
		tmp_acc= next_acc;
	}
	udp_fd->uf_rdbuf_head= NULL;
}

PUBLIC int udp_write(fd, count)
int fd;
size_t count;
{
	udp_fd_t *udp_fd;
	udp_port_t *udp_port;

	udp_fd= &udp_fd_table[fd];
	udp_port= udp_fd->uf_port;

	if (!(udp_fd->uf_flags & UFF_OPTSET))
	{
		reply_thr_get (udp_fd, EBADMODE, FALSE);
		return NW_OK;
	}

assert (!(udp_fd->uf_flags & UFF_WRITE_IP));

	udp_fd->uf_wr_count= count;

	udp_fd->uf_flags |= UFF_WRITE_IP;

	restart_write_fd(udp_fd);

	if (udp_fd->uf_flags & UFF_WRITE_IP)
	{
		DBLOCK(1, printf("replying NW_SUSPEND\n"));

		return NW_SUSPEND;
	}
	else
	{
		return NW_OK;
	}
}

PRIVATE void restart_write_fd(udp_fd)
udp_fd_t *udp_fd;
{
	udp_port_t *udp_port;
	acc_t *pack, *ip_hdr_pack, *udp_hdr_pack, *ip_opt_pack, *user_data;
	udp_hdr_t *udp_hdr;
	udp_io_hdr_t *udp_io_hdr;
	ip_hdr_t *ip_hdr;
	size_t ip_opt_size, user_data_size;
	unsigned long flags;
	u16_t chksum;
	u8_t u16[2];
	int result;

	udp_port= udp_fd->uf_port;

	if (udp_port->up_flags & UPF_WRITE_IP)
	{
		udp_port->up_flags |= UPF_MORE2WRITE;
		return;
	}

assert (udp_fd->uf_flags & UFF_WRITE_IP);
	udp_fd->uf_flags &= ~UFF_WRITE_IP;

assert (!udp_port->up_wr_pack);

	pack= (*udp_fd->uf_get_userdata)(udp_fd->uf_srfd, 0,
		udp_fd->uf_wr_count, FALSE);
	if (!pack)
	{
		udp_fd->uf_flags &= ~UFF_WRITE_IP;
		reply_thr_get (udp_fd, EFAULT, FALSE);
		return;
	}

	flags= udp_fd->uf_udpopt.nwuo_flags;

	ip_hdr_pack= bf_memreq(IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(ip_hdr_pack);

	udp_hdr_pack= bf_memreq(UDP_HDR_SIZE);
	udp_hdr= (udp_hdr_t *)ptr2acc_data(udp_hdr_pack);

	if (flags & NWUO_RWDATALL)
	{
		pack= bf_packIffLess(pack, UDP_IO_HDR_SIZE);
		udp_io_hdr= (udp_io_hdr_t *)ptr2acc_data(pack);
#if CONF_UDP_IO_NW_BYTE_ORDER
		ip_opt_size= ntohs(udp_io_hdr->uih_ip_opt_len);
#else
		ip_opt_size= udp_io_hdr->uih_ip_opt_len;
#endif
		if (UDP_IO_HDR_SIZE+ip_opt_size>udp_fd->uf_wr_count)
		{
			bf_afree(ip_hdr_pack);
			bf_afree(udp_hdr_pack);
			bf_afree(pack);
			reply_thr_get (udp_fd, EINVAL, FALSE);
			return;
		}
		if (ip_opt_size & 3)
		{
			bf_afree(ip_hdr_pack);
			bf_afree(udp_hdr_pack);
			bf_afree(pack);
			reply_thr_get (udp_fd, EFAULT, FALSE);
			return;
		}
		if (ip_opt_size)
			ip_opt_pack= bf_cut(pack, UDP_IO_HDR_SIZE, ip_opt_size);
		else
			ip_opt_pack= 0;
		user_data_size= udp_fd->uf_wr_count-UDP_IO_HDR_SIZE-
			ip_opt_size;
		user_data= bf_cut(pack, UDP_IO_HDR_SIZE+ip_opt_size, 
			user_data_size);
		bf_afree(pack);
	}
	else
	{
		udp_io_hdr= 0;
		ip_opt_size= 0;
		user_data_size= udp_fd->uf_wr_count;
		ip_opt_pack= 0;
		user_data= pack;
	}

	ip_hdr->ih_vers_ihl= (IP_MIN_HDR_SIZE+ip_opt_size) >> 2;
	ip_hdr->ih_tos= UDP_TOS;
	ip_hdr->ih_flags_fragoff= HTONS(UDP_IP_FLAGS);
	ip_hdr->ih_ttl= IP_DEF_TTL;
	ip_hdr->ih_proto= IPPROTO_UDP;
	if (flags & NWUO_RA_SET)
	{
		ip_hdr->ih_dst= udp_fd->uf_udpopt.nwuo_remaddr;
	}
	else
	{
assert (udp_io_hdr);
		ip_hdr->ih_dst= udp_io_hdr->uih_dst_addr;
	}

	if ((flags & NWUO_LOCPORT_MASK) != NWUO_LP_ANY)
		udp_hdr->uh_src_port= udp_fd->uf_udpopt.nwuo_locport;
	else
	{
assert (udp_io_hdr);
		udp_hdr->uh_src_port= udp_io_hdr->uih_src_port;
	}

	if (flags & NWUO_RP_SET)
		udp_hdr->uh_dst_port= udp_fd->uf_udpopt.nwuo_remport;
	else
	{
assert (udp_io_hdr);
		udp_hdr->uh_dst_port= udp_io_hdr->uih_dst_port;
	}

	udp_hdr->uh_length= htons(UDP_HDR_SIZE+user_data_size);
	udp_hdr->uh_chksum= 0;

	udp_hdr_pack->acc_next= user_data;
	chksum= pack_oneCsum(udp_hdr_pack);
	chksum= oneC_sum(chksum, (u16_t *)&udp_fd->uf_port->up_ipaddr,
		sizeof(ipaddr_t));
	chksum= oneC_sum(chksum, (u16_t *)&ip_hdr->ih_dst, sizeof(ipaddr_t));
	u16[0]= 0;
	u16[1]= IPPROTO_UDP;
	chksum= oneC_sum(chksum, (u16_t *)u16, sizeof(u16));
	chksum= oneC_sum(chksum, (u16_t *)&udp_hdr->uh_length, sizeof(u16_t));
	if (~chksum)
		chksum= ~chksum;
	udp_hdr->uh_chksum= chksum;
	
	if (ip_opt_pack)
	{
		ip_opt_pack= bf_packIffLess(ip_opt_pack, ip_opt_size);
		ip_opt_pack->acc_next= udp_hdr_pack;
		udp_hdr_pack= ip_opt_pack;
	}
	ip_hdr_pack->acc_next= udp_hdr_pack;

assert (!udp_port->up_wr_pack);
assert (!(udp_port->up_flags & UPF_WRITE_IP));

	udp_port->up_wr_pack= ip_hdr_pack;
	udp_port->up_flags |= UPF_WRITE_IP;
	result= ip_write(udp_port->up_ipfd, bf_bufsize(ip_hdr_pack));
	if (result == NW_SUSPEND)
	{
		udp_port->up_flags |= UPF_WRITE_SP;
		udp_fd->uf_flags |= UFF_WRITE_IP;
		udp_port->up_write_fd= udp_fd;
	}
	else if (result<0)
		reply_thr_get(udp_fd, result, FALSE);
	else
		reply_thr_get (udp_fd, udp_fd->uf_wr_count, FALSE);
}

PRIVATE u16_t pack_oneCsum(pack)
acc_t *pack;
{
	u16_t prev;
	int odd_byte;
	char *data_ptr;
	int length;
	char byte_buf[2];

	assert (pack);

	prev= 0;

	odd_byte= FALSE;
	for (; pack; pack= pack->acc_next)
	{
		
		data_ptr= ptr2acc_data(pack);
		length= pack->acc_length;

		if (!length)
			continue;
		if (odd_byte)
		{
			byte_buf[1]= *data_ptr;
			prev= oneC_sum(prev, (u16_t *)byte_buf, 2);
			data_ptr++;
			length--;
			odd_byte= FALSE;
		}
		if (length & 1)
		{
			odd_byte= TRUE;
			length--;
			byte_buf[0]= data_ptr[length];
		}
		if (!length)
			continue;
		prev= oneC_sum (prev, (u16_t *)data_ptr, length);
	}
	if (odd_byte)
	{
		byte_buf[1]= 0;
		prev= oneC_sum (prev, (u16_t *)byte_buf, 1);
	}
	return prev;
}

PRIVATE void udp_restart_write_port(udp_port )
udp_port_t *udp_port;
{
	udp_fd_t *udp_fd;
	int i;

assert (!udp_port->up_wr_pack);
assert (!(udp_port->up_flags & (UPF_WRITE_IP|UPF_WRITE_SP)));

	while (udp_port->up_flags & UPF_MORE2WRITE)
	{
		udp_port->up_flags &= ~UPF_MORE2WRITE;

		for (i= 0, udp_fd= udp_port->up_next_fd; i<UDP_FD_NR;
			i++, udp_fd++)
		{
			if (udp_fd == &udp_fd_table[UDP_FD_NR])
				udp_fd= udp_fd_table;

			if (!(udp_fd->uf_flags & UFF_INUSE))
				continue;
			if (!(udp_fd->uf_flags & UFF_WRITE_IP))
				continue;
			if (udp_fd->uf_port != udp_port)
				continue;
			restart_write_fd(udp_fd);
			if (udp_port->up_flags & UPF_WRITE_IP)
			{
				udp_port->up_next_fd= udp_fd+1;
				udp_port->up_flags |= UPF_MORE2WRITE;
				return;
			}
		}
	}
}

PUBLIC int udp_cancel(fd, which_operation)
int fd;
int which_operation;
{
	udp_fd_t *udp_fd;

	DBLOCK(0x10, printf("udp_cancel(%d, %d)\n", fd, which_operation));

	udp_fd= &udp_fd_table[fd];

	switch (which_operation)
	{
	case SR_CANCEL_READ:
assert (udp_fd->uf_flags & UFF_READ_IP);
		udp_fd->uf_flags &= ~UFF_READ_IP;
		reply_thr_put(udp_fd, EINTR, FALSE);
		break;
	case SR_CANCEL_WRITE:
assert (udp_fd->uf_flags & UFF_WRITE_IP);
		udp_fd->uf_flags &= ~UFF_WRITE_IP;
		if (udp_fd->uf_port->up_write_fd == udp_fd)
			udp_fd->uf_port->up_write_fd= NULL;
		reply_thr_get(udp_fd, EINTR, FALSE);
		break;
	case SR_CANCEL_IOCTL:
assert (udp_fd->uf_flags & UFF_IOCTL_IP);
		udp_fd->uf_flags &= ~UFF_IOCTL_IP;
		udp_fd->uf_flags &= ~UFF_PEEK_IP;
		reply_thr_get(udp_fd, EINTR, TRUE);
		break;
	default:
		ip_panic(( "got unknown cancel request" ));
	}
	return NW_OK;
}

PRIVATE void udp_buffree (priority)
int priority;
{
	int i;
	udp_fd_t *udp_fd;
	acc_t *tmp_acc;

	if (priority ==  UDP_PRI_FDBUFS_EXTRA)
	{
		for (i=0, udp_fd= udp_fd_table; i<UDP_FD_NR; i++, udp_fd++)
		{
			while (udp_fd->uf_rdbuf_head &&
				udp_fd->uf_rdbuf_head->acc_ext_link)
			{
				tmp_acc= udp_fd->uf_rdbuf_head;
				udp_fd->uf_rdbuf_head= tmp_acc->acc_ext_link;
				bf_afree(tmp_acc);
			}
		}
	}

	if (priority  == UDP_PRI_FDBUFS)
	{
		for (i=0, udp_fd= udp_fd_table; i<UDP_FD_NR; i++, udp_fd++)
		{
			while (udp_fd->uf_rdbuf_head)
			{
				tmp_acc= udp_fd->uf_rdbuf_head;
				udp_fd->uf_rdbuf_head= tmp_acc->acc_ext_link;
				bf_afree(tmp_acc);
			}
		}
	}
}

PRIVATE void udp_rd_enqueue(udp_fd, pack, exp_tim)
udp_fd_t *udp_fd;
acc_t *pack;
clock_t exp_tim;
{
	acc_t *tmp_acc;
	int result;

	if (pack->acc_linkC != 1)
	{
		tmp_acc= bf_dupacc(pack);
		bf_afree(pack);
		pack= tmp_acc;
	}
	pack->acc_ext_link= NULL;
	if (udp_fd->uf_rdbuf_head == NULL)
	{
		udp_fd->uf_exp_tim= exp_tim;
		udp_fd->uf_rdbuf_head= pack;
	}
	else
		udp_fd->uf_rdbuf_tail->acc_ext_link= pack;
	udp_fd->uf_rdbuf_tail= pack;

	if (udp_fd->uf_flags & UFF_PEEK_IP)
	{
		pack= bf_cut(udp_fd->uf_rdbuf_head, 0,
			sizeof(udp_io_hdr_t));
		result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd,
			(size_t)0, pack, TRUE);

		udp_fd->uf_flags &= ~UFF_IOCTL_IP;
		udp_fd->uf_flags &= ~UFF_PEEK_IP;
		result= (*udp_fd->uf_put_userdata)(udp_fd->uf_srfd,
			result, (acc_t *)0, TRUE);
		assert (result == 0);
	}

	if (udp_fd->uf_flags & UFF_SEL_READ)
	{
		udp_fd->uf_flags &= ~UFF_SEL_READ;
		if (udp_fd->uf_select_res)
			udp_fd->uf_select_res(udp_fd->uf_srfd, SR_SELECT_READ);
		else
			printf("udp_rd_enqueue: no select_res\n");
	}
}

PRIVATE void hash_fd(udp_fd)
udp_fd_t *udp_fd;
{
	udp_port_t *udp_port;
	int hash;

	udp_port= udp_fd->uf_port;
	if ((udp_fd->uf_udpopt.nwuo_flags & NWUO_LOCPORT_MASK) ==
		NWUO_LP_ANY)
	{
		udp_fd->uf_port_next= udp_port->up_port_any;
		udp_port->up_port_any= udp_fd;
	}
	else
	{
		hash= udp_fd->uf_udpopt.nwuo_locport;
		hash ^= (hash >> 8);
		hash &= (UDP_PORT_HASH_NR-1);

		udp_fd->uf_port_next= udp_port->up_port_hash[hash];
		udp_port->up_port_hash[hash]= udp_fd;
	}
}

PRIVATE void unhash_fd(udp_fd)
udp_fd_t *udp_fd;
{
	udp_port_t *udp_port;
	udp_fd_t *prev, *curr, **udp_fd_p;
	int hash;

	udp_port= udp_fd->uf_port;
	if ((udp_fd->uf_udpopt.nwuo_flags & NWUO_LOCPORT_MASK) ==
		NWUO_LP_ANY)
	{
		udp_fd_p= &udp_port->up_port_any;
	}
	else
	{
		hash= udp_fd->uf_udpopt.nwuo_locport;
		hash ^= (hash >> 8);
		hash &= (UDP_PORT_HASH_NR-1);

		udp_fd_p= &udp_port->up_port_hash[hash];
	}
	for (prev= NULL, curr= *udp_fd_p; curr;
		prev= curr, curr= curr->uf_port_next)
	{
		if (curr == udp_fd)
			break;
	}
	assert(curr);
	if (prev)
		prev->uf_port_next= curr->uf_port_next;
	else
		*udp_fd_p= curr->uf_port_next;
}

#ifdef BUF_CONSISTENCY_CHECK
PRIVATE void udp_bufcheck()
{
	int i;
	udp_port_t *udp_port;
	udp_fd_t *udp_fd;
	acc_t *tmp_acc;

	for (i= 0, udp_port= udp_port_table; i<udp_conf_nr; i++, udp_port++)
	{
		if (udp_port->up_wr_pack)
			bf_check_acc(udp_port->up_wr_pack);
	}

	for (i= 0, udp_fd= udp_fd_table; i<UDP_FD_NR; i++, udp_fd++)
	{
		for (tmp_acc= udp_fd->uf_rdbuf_head; tmp_acc; 
			tmp_acc= tmp_acc->acc_ext_link)
		{
			bf_check_acc(tmp_acc);
		}
	}
}
#endif

/*
 * $PchId: udp.c,v 1.25 2005/06/28 14:14:44 philip Exp $
 */
