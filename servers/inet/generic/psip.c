/*
generic/psip.c

Implementation of a pseudo IP device.

Created:	Apr 22, 1993 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "assert.h"
#include "buf.h"
#include "event.h"
#include "type.h"
#include "ip_int.h"
#include "psip.h"
#include "sr.h"

#if ENABLE_PSIP

THIS_FILE

typedef struct psip_port
{
	int pp_flags;
	int pp_ipdev;
	int pp_opencnt;
	struct psip_fd *pp_rd_head;
	struct psip_fd *pp_rd_tail;
	acc_t *pp_promisc_head;
	acc_t *pp_promisc_tail;
} psip_port_t;

#define PPF_EMPTY	0
#define PPF_CONFIGURED	1
#define PPF_ENABLED	2
#define PPF_PROMISC	4

#define PSIP_FD_NR	(1*IP_PORT_MAX)

typedef struct psip_fd
{
	int pf_flags;
	int pf_srfd;
	psip_port_t *pf_port;
	get_userdata_t pf_get_userdata;
	put_userdata_t pf_put_userdata;
	struct psip_fd *pf_rd_next;
	size_t pf_rd_count;
	nwio_psipopt_t pf_psipopt;
} psip_fd_t;

#define PFF_EMPTY	0
#define PFF_INUSE	1
#define PFF_READ_IP	2
#define PFF_PROMISC	4

PRIVATE psip_port_t *psip_port_table;
PRIVATE psip_fd_t psip_fd_table[PSIP_FD_NR];

FORWARD int psip_open ARGS(( int port, int srfd,
	get_userdata_t get_userdata, put_userdata_t put_userdata,
	put_pkt_t pkt_pkt ));
FORWARD int psip_ioctl ARGS(( int fd, ioreq_t req ));
FORWARD int psip_read ARGS(( int fd, size_t count ));
FORWARD int psip_write ARGS(( int fd, size_t count ));
FORWARD void psip_close ARGS(( int fd ));
FORWARD int psip_cancel ARGS(( int fd, int which_operation ));
FORWARD void promisc_restart_read ARGS(( psip_port_t *psip_port ));
FORWARD int psip_setopt ARGS(( psip_fd_t *psip_fd, nwio_psipopt_t *newoptp ));
FORWARD void psip_buffree ARGS(( int priority ));
#ifdef BUF_CONSISTENCY_CHECK
FORWARD void psip_bufcheck ARGS(( void ));
#endif
FORWARD void reply_thr_put ARGS(( psip_fd_t *psip_fd, int reply,
	int for_ioctl ));
FORWARD void reply_thr_get ARGS(( psip_fd_t *psip_fd, int reply,
	int for_ioctl ));

PUBLIC void psip_prep()
{
	psip_port_table= alloc(psip_conf_nr * sizeof(psip_port_table[0]));
}

PUBLIC void psip_init()
{
	int i;
	psip_port_t *psip_port;
	psip_fd_t *psip_fd;

#if ZERO
	for (i=0, psip_port= psip_port_table; i<psip_conf_nr; i++, psip_port++)
		psip_port->pp_flags= PPF_EMPTY;

	for (i=0, psip_fd= psip_fd_table; i<PSIP_FD_NR; i++, psip_fd++)
		psip_fd->pf_flags= PFF_EMPTY;
#endif

	for (i=0, psip_port= psip_port_table; i<psip_conf_nr; i++, psip_port++)
	{
		psip_port->pp_flags |= PPF_CONFIGURED;
#if ZERO
		psip_port->pp_opencnt= 0;
		psip_port->pp_rd_head= NULL;
		psip_port->pp_promisc_head= NULL;
#endif
	}

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(psip_buffree);
#else
	bf_logon(psip_buffree, psip_bufcheck);
#endif
}

PUBLIC int psip_enable(port_nr, ip_port_nr)
int port_nr;
int ip_port_nr;
{
	psip_port_t *psip_port;

	assert(port_nr >= 0 && port_nr < psip_conf_nr);

	psip_port= &psip_port_table[port_nr];
	assert(psip_port->pp_flags & PPF_CONFIGURED);

	psip_port->pp_ipdev= ip_port_nr;
	psip_port->pp_flags |= PPF_ENABLED;

	sr_add_minor(if2minor(psip_conf[port_nr].pc_ifno, PSIP_DEV_OFF),
		port_nr, psip_open, psip_close, psip_read,
		psip_write, psip_ioctl, psip_cancel);

	return NW_OK;
}

PUBLIC int psip_send(port_nr, pack)
int port_nr;
acc_t *pack;
{
	psip_port_t *psip_port;
	psip_fd_t *psip_fd, *mark_fd;
	int i, result, result1;
	size_t buf_size;
	acc_t *hdr_pack;
	psip_io_hdr_t *hdr;

	assert(port_nr >= 0 && port_nr < psip_conf_nr);
	psip_port= &psip_port_table[port_nr];

	if (psip_port->pp_opencnt == 0)
	{
		bf_afree(pack);
		return NW_OK;
	}

	for(;;)
	{
		mark_fd= psip_port->pp_rd_tail;

		for(i= 0; i<PSIP_FD_NR; i++)
		{
			psip_fd= psip_port->pp_rd_head;
			if (!psip_fd)
				return NW_SUSPEND;
			psip_port->pp_rd_head= psip_fd->pf_rd_next;
			if (!(psip_fd->pf_flags & PFF_PROMISC))
				break;
			psip_fd->pf_rd_next= NULL;
			if (psip_port->pp_rd_head == NULL)
				psip_port->pp_rd_head= psip_fd;
			else
				psip_port->pp_rd_tail->pf_rd_next= psip_fd;
			psip_port->pp_rd_tail= psip_fd;
			if (psip_fd == mark_fd)
				return NW_SUSPEND;
		}
		if (i == PSIP_FD_NR)
			ip_panic(( "psip_send: loop" ));

		assert(psip_fd->pf_flags & PFF_READ_IP);
		psip_fd->pf_flags &= ~PFF_READ_IP;

		buf_size= bf_bufsize(pack);
		if (buf_size <= psip_fd->pf_rd_count)
		{
			if (psip_port->pp_flags & PPF_PROMISC)
			{
				/* Deal with promiscuous mode. */
				hdr_pack= bf_memreq(sizeof(*hdr));
				hdr= (psip_io_hdr_t *)ptr2acc_data(hdr_pack);
				memset(hdr, '\0', sizeof(*hdr));
				hdr->pih_flags |= PF_LOC2REM;

				pack->acc_linkC++;
				hdr_pack->acc_next= pack;
				hdr_pack->acc_ext_link= NULL;
				if (psip_port->pp_promisc_head)
				{
					/* Append at the end. */
					psip_port->pp_promisc_tail->
						acc_ext_link= hdr_pack;
					psip_port->pp_promisc_tail= hdr_pack;
				}
				else
				{
					/* First packet. */
					psip_port->pp_promisc_head= hdr_pack;
					psip_port->pp_promisc_tail= hdr_pack;
					if (psip_port->pp_rd_head)
					    promisc_restart_read(psip_port);
				}
			}
			result= (*psip_fd->pf_put_userdata)(psip_fd->pf_srfd, 
				(size_t)0, pack, FALSE);
			if (result == NW_OK)
				result= buf_size;
		}
		else
			result= EPACKSIZE;

		result1= (*psip_fd->pf_put_userdata)(psip_fd->pf_srfd,
				(size_t)result, NULL, FALSE);
		assert(result1 == NW_OK);
		if (result == EPACKSIZE)
			continue;
		return NW_OK;
	}
	return NW_SUSPEND;
}

PRIVATE int psip_open(port, srfd, get_userdata, put_userdata, put_pkt)
int port;
int srfd;
get_userdata_t get_userdata;
put_userdata_t put_userdata;
put_pkt_t put_pkt;
{
	psip_port_t *psip_port;
	psip_fd_t *psip_fd;
	int i;

	assert(port >= 0 && port < psip_conf_nr);
	psip_port= &psip_port_table[port];

	if (!(psip_port->pp_flags & PPF_ENABLED))
		return ENXIO;

	for (i= 0, psip_fd= psip_fd_table; i<PSIP_FD_NR; i++, psip_fd++)
	{
		if (psip_fd->pf_flags & PFF_INUSE)
			continue;
		break;
	}
	if (i == PSIP_FD_NR)
		return ENFILE;
	psip_fd->pf_flags |= PFF_INUSE;
	psip_fd->pf_srfd= srfd;
	psip_fd->pf_port= psip_port;
	psip_fd->pf_get_userdata= get_userdata;
	psip_fd->pf_put_userdata= put_userdata;
	psip_port->pp_opencnt++;

	return i;
}

PRIVATE int psip_ioctl(fd, req)
int fd;
ioreq_t req;
{
	int result;
	psip_fd_t *psip_fd;
	acc_t *data;
	nwio_psipopt_t *psip_opt, *newoptp;

	assert(fd >= 0 && fd < PSIP_FD_NR);
	psip_fd= &psip_fd_table[fd];

	switch(req)
	{
	case NWIOSPSIPOPT:
		data= (*psip_fd->pf_get_userdata)(psip_fd->pf_srfd, 0, 
			sizeof(*psip_opt), TRUE);
		if (!data)
		{
			result= EFAULT;
			break;
		}
		data= bf_packIffLess(data, sizeof(*psip_opt));
		assert (data->acc_length == sizeof(*psip_opt));

		newoptp= (nwio_psipopt_t *)ptr2acc_data(data);
		result= psip_setopt(psip_fd, newoptp);
		bf_afree(data);
		if (result == NW_OK)
		{
			if (psip_fd->pf_psipopt.nwpo_flags & NWPO_EN_PROMISC)
			{
				psip_fd->pf_flags |= PFF_PROMISC;
				psip_fd->pf_port->pp_flags |= PPF_PROMISC;
			}
			else
			{
				psip_fd->pf_flags &= ~PFF_PROMISC;
				/* XXX check port flags */
			}
		}
		reply_thr_get(psip_fd, result, TRUE);
		break;
	case NWIOGPSIPOPT:
		data= bf_memreq(sizeof(*psip_opt));
		psip_opt= (nwio_psipopt_t *)ptr2acc_data(data);

		*psip_opt= psip_fd->pf_psipopt;
		result= (*psip_fd->pf_put_userdata)(psip_fd->pf_srfd, 0,
			data, TRUE);
		if (result == NW_OK)
			reply_thr_put(psip_fd, NW_OK, TRUE);
		break;
	default:
		reply_thr_put(psip_fd, ENOTTY, TRUE);
		break;
	}
	return NW_OK;
}

PRIVATE int psip_read(fd, count)
int fd;
size_t count;
{
	psip_port_t *psip_port;
	psip_fd_t *psip_fd;

	assert(fd >= 0 && fd < PSIP_FD_NR);
	psip_fd= &psip_fd_table[fd];
	psip_port= psip_fd->pf_port;

	psip_fd->pf_rd_count= count;
	if (psip_port->pp_rd_head == NULL)
		psip_port->pp_rd_head= psip_fd;
	else
		psip_port->pp_rd_tail->pf_rd_next= psip_fd;
	psip_fd->pf_rd_next= NULL;
	psip_port->pp_rd_tail= psip_fd;

	psip_fd->pf_flags |= PFF_READ_IP;
	if (psip_fd->pf_flags & PFF_PROMISC)
		promisc_restart_read(psip_port);
	else
		ipps_get(psip_port->pp_ipdev);
	if (psip_fd->pf_flags & PFF_READ_IP)
		return NW_SUSPEND;
	return NW_OK;
}

PRIVATE int psip_write(fd, count)
int fd;
size_t count;
{
	psip_port_t *psip_port;
	psip_fd_t *psip_fd;
	acc_t *pack, *hdr_pack;
	psip_io_hdr_t *hdr;

	assert(fd >= 0 && fd < PSIP_FD_NR);
	psip_fd= &psip_fd_table[fd];
	psip_port= psip_fd->pf_port;

	pack= (*psip_fd->pf_get_userdata)(psip_fd->pf_srfd, (size_t)0,
		count, FALSE);
	if (pack == NULL)
	{
		pack= (*psip_fd->pf_get_userdata)(psip_fd->pf_srfd, 
			(size_t)EFAULT, (size_t)0, FALSE);
		assert(pack == NULL);
		return NW_OK;
	}
	if (psip_port->pp_flags & PPF_PROMISC)
	{
		/* Deal with promiscuous mode. */
		hdr_pack= bf_memreq(sizeof(*hdr));
		hdr= (psip_io_hdr_t *)ptr2acc_data(hdr_pack);
		memset(hdr, '\0', sizeof(*hdr));
		hdr->pih_flags |= PF_REM2LOC;

		pack->acc_linkC++;
		hdr_pack->acc_next= pack;
		hdr_pack->acc_ext_link= NULL;
		if (psip_port->pp_promisc_head)
		{
			/* Append at the end. */
			psip_port->pp_promisc_tail->acc_ext_link= hdr_pack;
		}
		else
		{
			/* First packet. */
			psip_port->pp_promisc_head= hdr_pack;
			if (psip_port->pp_rd_head)
				promisc_restart_read(psip_port);
		}
	}
	ipps_put(psip_port->pp_ipdev, pack);
	pack= (*psip_fd->pf_get_userdata)(psip_fd->pf_srfd, (size_t)count,
		(size_t)0, FALSE);
	assert(pack == NULL);
	return NW_OK;
}

PRIVATE void psip_close(fd)
int fd;
{
	psip_port_t *psip_port;
	psip_fd_t *psip_fd;
	acc_t *acc, *acc_next;
	int i;

	assert(fd >= 0 && fd < PSIP_FD_NR);
	psip_fd= &psip_fd_table[fd];
	psip_port= psip_fd->pf_port;

	assert(psip_port->pp_opencnt >0);
	psip_port->pp_opencnt--;
	psip_fd->pf_flags= PFF_EMPTY;
	ipps_get(psip_port->pp_ipdev);

	/* Check if the port should still be in promiscuous mode. */
	if (psip_port->pp_flags & PPF_PROMISC)
	{
		psip_port->pp_flags &= ~PPF_PROMISC;
		for (i= 0, psip_fd= psip_fd_table; i<PSIP_FD_NR;
			i++, psip_fd++)
		{
			if ((psip_fd->pf_flags & (PFF_INUSE|PFF_PROMISC)) !=
				(PFF_INUSE|PFF_PROMISC))
			{
				continue;
			}
			if (psip_fd->pf_port != psip_port)
				continue;
			psip_port->pp_flags |= PPF_PROMISC;
			break;
		}
		if (!(psip_port->pp_flags & PPF_PROMISC))
		{
			/* Delete queued packets. */
			acc= psip_port->pp_promisc_head;
			psip_port->pp_promisc_head= NULL;
			while (acc)
			{
				acc_next= acc->acc_ext_link;
				bf_afree(acc);
				acc= acc_next;
			}
		}
	}
}

PRIVATE int psip_cancel(fd, which_operation)
int fd;
int which_operation;
{
	psip_port_t *psip_port;
	psip_fd_t *psip_fd, *prev_fd, *tmp_fd;
	int result;

	DBLOCK(1, printf("psip_cancel(%d, %d)\n", fd, which_operation));

	assert(fd >= 0 && fd < PSIP_FD_NR);
	psip_fd= &psip_fd_table[fd];
	psip_port= psip_fd->pf_port;

	switch(which_operation)
	{
#if !CRAMPED
	case SR_CANCEL_IOCTL:
		ip_panic(( "should not be here" ));
#endif
	case SR_CANCEL_READ:
		assert(psip_fd->pf_flags & PFF_READ_IP);
		for (prev_fd= NULL, tmp_fd= psip_port->pp_rd_head; tmp_fd;
				prev_fd= tmp_fd, tmp_fd= tmp_fd->pf_rd_next)
		{
			if (tmp_fd == psip_fd)
				break;
		}
#if !CRAMPED
		if (tmp_fd == NULL)
			ip_panic(( "unable to find to request to cancel" ));
#endif
		if (prev_fd == NULL)
			psip_port->pp_rd_head= psip_fd->pf_rd_next;
		else
			prev_fd->pf_rd_next= psip_fd->pf_rd_next;
		if (psip_fd->pf_rd_next == NULL)
			psip_port->pp_rd_tail= prev_fd;
		psip_fd->pf_flags &= ~PFF_READ_IP;
		result= (*psip_fd->pf_put_userdata)(psip_fd->pf_srfd,
						(size_t)EINTR, NULL, FALSE);
		assert(result == NW_OK);
		break;
#if !CRAMPED
	case SR_CANCEL_WRITE:
		ip_panic(( "should not be here" ));
	default:
		ip_panic(( "invalid operation for cancel" ));
#endif
	}
	return NW_OK;
}

PRIVATE void promisc_restart_read(psip_port)
psip_port_t *psip_port;
{
	psip_fd_t *psip_fd, *mark_fd;
	acc_t *pack;
	size_t buf_size;
	int i, result, result1;

	while (psip_port->pp_promisc_head)
	{
		mark_fd= psip_port->pp_rd_tail;

		for(i= 0; i<PSIP_FD_NR; i++)
		{
			psip_fd= psip_port->pp_rd_head;
			if (!psip_fd)
				return;
			psip_port->pp_rd_head= psip_fd->pf_rd_next;
			if (psip_fd->pf_flags & PFF_PROMISC)
				break;
			psip_fd->pf_rd_next= NULL;
			if (psip_port->pp_rd_head == NULL)
				psip_port->pp_rd_head= psip_fd;
			else
				psip_port->pp_rd_tail->pf_rd_next= psip_fd;
			psip_port->pp_rd_tail= psip_fd;
			if (psip_fd == mark_fd)
				return;
		}
		if (i == PSIP_FD_NR)
			ip_panic(( "psip'promisc_restart_read: loop" ));

		assert(psip_fd->pf_flags & PFF_READ_IP);
		psip_fd->pf_flags &= ~PFF_READ_IP;

		pack= psip_port->pp_promisc_head;
		buf_size= bf_bufsize(pack);
		if (buf_size <= psip_fd->pf_rd_count)
		{
			psip_port->pp_promisc_head= pack->acc_ext_link;
			result= (*psip_fd->pf_put_userdata)(psip_fd->pf_srfd, 
				(size_t)0, pack, FALSE);
			if (result == NW_OK)
				result= buf_size;
		}
		else
			result= EPACKSIZE;

		result1= (*psip_fd->pf_put_userdata)(psip_fd->pf_srfd,
				(size_t)result, NULL, FALSE);
		assert(result1 == NW_OK);
	}
}

PRIVATE int psip_setopt(psip_fd, newoptp)
psip_fd_t *psip_fd;
nwio_psipopt_t *newoptp;
{
	nwio_psipopt_t oldopt;
	int result;
	unsigned int new_en_flags, new_di_flags, old_en_flags, old_di_flags,
		all_flags, flags;
	unsigned long new_flags;
	int i;

	oldopt= psip_fd->pf_psipopt;

	old_en_flags= oldopt.nwpo_flags & 0xffff;
	old_di_flags= (oldopt.nwpo_flags >> 16) & 0xffff;

	new_en_flags= newoptp->nwpo_flags & 0xffff;
	new_di_flags= (newoptp->nwpo_flags >> 16) & 0xffff;

	if (new_en_flags & new_di_flags)
		return EBADMODE;

	/* NWUO_LOCADDR_MASK */
	if (!((new_en_flags | new_di_flags) & NWPO_PROMISC_MASK))
	{
		new_en_flags |= (old_en_flags & NWPO_PROMISC_MASK);
		new_di_flags |= (old_di_flags & NWPO_PROMISC_MASK);
	}

	new_flags= ((unsigned long)new_di_flags << 16) | new_en_flags;

	psip_fd->pf_psipopt= *newoptp;
	psip_fd->pf_psipopt.nwpo_flags= new_flags;

	return NW_OK;
}

PRIVATE void psip_buffree (priority)
int priority;
{
	int i;
	psip_port_t *psip_port;
	acc_t *tmp_acc, *next_acc;

	if (priority == PSIP_PRI_EXP_PROMISC)
	{
		for (i=0, psip_port= psip_port_table; i<psip_conf_nr;
			i++, psip_port++)
		{
			if (!(psip_port->pp_flags & PPF_CONFIGURED) )
				continue;
			if (psip_port->pp_promisc_head)
			{
				tmp_acc= psip_port->pp_promisc_head;
				while(tmp_acc)
				{
					next_acc= tmp_acc->acc_ext_link;
					bf_afree(tmp_acc);
					tmp_acc= next_acc;
				}
				psip_port->pp_promisc_head= NULL;
			}
		}
	}
}

#ifdef BUF_CONSISTENCY_CHECK
PRIVATE void psip_bufcheck()
{
	int i;
	psip_port_t *psip_port;
	acc_t *tmp_acc;

	for (i= 0, psip_port= psip_port_table; i<psip_conf_nr;
		i++, psip_port++)
	{
		for (tmp_acc= psip_port->pp_promisc_head; tmp_acc; 
			tmp_acc= tmp_acc->acc_ext_link)
		{
			bf_check_acc(tmp_acc);
		}
	}
}
#endif

/*
reply_thr_put
*/

PRIVATE void reply_thr_put(psip_fd, reply, for_ioctl)
psip_fd_t *psip_fd;
int reply;
int for_ioctl;
{
	int result;

	result= (*psip_fd->pf_put_userdata)(psip_fd->pf_srfd, reply,
		(acc_t *)0, for_ioctl);
	assert(result == NW_OK);
}

/*
reply_thr_get
*/

PRIVATE void reply_thr_get(psip_fd, reply, for_ioctl)
psip_fd_t *psip_fd;
int reply;
int for_ioctl;
{
	acc_t *result;
	result= (*psip_fd->pf_get_userdata)(psip_fd->pf_srfd, reply,
		(size_t)0, for_ioctl);
	assert (!result);
}

#endif /* ENABLE_PSIP */

/*
 * $PchId: psip.c,v 1.6 1996/05/07 20:50:31 philip Exp $
 */
