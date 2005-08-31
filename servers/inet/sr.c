/*	this file contains the interface of the network software with the file
 *	system.
 *
 * Copyright 1995 Philip Homburg
 *
 * The valid messages and their parameters are:
 * 
 * Requests:
 *
 *    m_type      NDEV_MINOR   NDEV_PROC    NDEV_REF   NDEV_MODE
 * -------------------------------------------------------------
 * | DEV_OPEN    |minor dev  | proc nr   |  fd       |   mode   |
 * |-------------+-----------+-----------+-----------+----------+
 * | DEV_CLOSE   |minor dev  | proc nr   |  fd       |          |
 * |-------------+-----------+-----------+-----------+----------+
 *
 *    m_type      NDEV_MINOR   NDEV_PROC    NDEV_REF   NDEV_COUNT NDEV_BUFFER
 * ---------------------------------------------------------------------------
 * | DEV_READ    |minor dev  | proc nr   |  fd       |  count    | buf ptr   |
 * |-------------+-----------+-----------+-----------+-----------+-----------|
 * | DEV_WRITE   |minor dev  | proc nr   |  fd       |  count    | buf ptr   |
 * |-------------+-----------+-----------+-----------+-----------+-----------|
 *
 *    m_type      NDEV_MINOR   NDEV_PROC    NDEV_REF   NDEV_IOCTL NDEV_BUFFER
 * ---------------------------------------------------------------------------
 * | DEV_IOCTL3  |minor dev  | proc nr   |  fd       |  command  | buf ptr   |
 * |-------------+-----------+-----------+-----------+-----------+-----------|
 *
 *    m_type      NDEV_MINOR   NDEV_PROC    NDEV_REF   NDEV_OPERATION
 * -------------------------------------------------------------------|
 * | DEV_CANCEL  |minor dev  | proc nr   |  fd       | which operation|
 * |-------------+-----------+-----------+-----------+----------------|
 *
 * Replies:
 *
 *    m_type        REP_PROC_NR   REP_STATUS   REP_REF    REP_OPERATION
 * ----------------------------------------------------------------------|
 * | DEVICE_REPLY |   proc nr   |  status    |  fd     | which operation |
 * |--------------+-------------+------------+---------+-----------------|
 */

#include "inet.h"

#ifndef __minix_vmd /* Minix 3 */
#include <sys/select.h>
#endif
#include <sys/svrctl.h>
#include <minix/callnr.h>

#include "mq.h"
#include "qp.h"
#include "proto.h"
#include "generic/type.h"

#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/event.h"
#include "generic/sr.h"
#include "sr_int.h"

#ifndef __minix_vmd /* Minix 3 */
#define DEV_CANCEL NW_CANCEL
#define DEVICE_REPLY REVIVE
#define DEV_IOCTL3 DEV_IOCTL
#define NDEV_BUFFER ADDRESS
#define NDEV_COUNT COUNT
#define NDEV_IOCTL REQUEST
#define NDEV_MINOR DEVICE
#define NDEV_PROC PROC_NR
#endif

THIS_FILE

PUBLIC sr_fd_t sr_fd_table[FD_NR];

PRIVATE mq_t *repl_queue, *repl_queue_tail;
#ifdef __minix_vmd
PRIVATE cpvec_t cpvec[CPVEC_NR];
#else /* Minix 3 */
PRIVATE struct vir_cp_req vir_cp_req[CPVEC_NR];
#endif

FORWARD _PROTOTYPE ( int sr_open, (message *m) );
FORWARD _PROTOTYPE ( void sr_close, (message *m) );
FORWARD _PROTOTYPE ( int sr_rwio, (mq_t *m) );
FORWARD _PROTOTYPE ( int sr_restart_read, (sr_fd_t *fdp) );
FORWARD _PROTOTYPE ( int sr_restart_write, (sr_fd_t *fdp) );
FORWARD _PROTOTYPE ( int sr_restart_ioctl, (sr_fd_t *fdp) );
FORWARD _PROTOTYPE ( int sr_cancel, (message *m) );
#ifndef __minix_vmd /* Minix 3 */
FORWARD _PROTOTYPE ( int sr_select, (message *m) );
FORWARD _PROTOTYPE ( void sr_status, (message *m) );
#endif
FORWARD _PROTOTYPE ( void sr_reply_, (mq_t *m, int reply, int is_revive) );
FORWARD _PROTOTYPE ( sr_fd_t *sr_getchannel, (int minor));
FORWARD _PROTOTYPE ( acc_t *sr_get_userdata, (int fd, vir_bytes offset,
					vir_bytes count, int for_ioctl) );
FORWARD _PROTOTYPE ( int sr_put_userdata, (int fd, vir_bytes offset,
						acc_t *data, int for_ioctl) );
#ifdef __minix_vmd 
#define sr_select_res 0
#else /* Minix 3 */
FORWARD _PROTOTYPE (void sr_select_res, (int fd, unsigned ops) );
#endif
FORWARD _PROTOTYPE ( int sr_repl_queue, (int proc, int ref, int operation) );
FORWARD _PROTOTYPE ( int walk_queue, (sr_fd_t *sr_fd, mq_t **q_head_ptr, 
	mq_t **q_tail_ptr, int type, int proc_nr, int ref, int first_flag) );
FORWARD _PROTOTYPE ( void process_req_q, (mq_t *mq, mq_t *tail, 
							mq_t **tail_ptr) );
FORWARD _PROTOTYPE ( void sr_event, (event_t *evp, ev_arg_t arg) );
FORWARD _PROTOTYPE ( int cp_u2b, (int proc, char *src, acc_t **var_acc_ptr,
								 int size) );
FORWARD _PROTOTYPE ( int cp_b2u, (acc_t *acc_ptr, int proc, char *dest) );

PUBLIC void sr_init()
{
	int i;

	for (i=0; i<FD_NR; i++)
	{
		sr_fd_table[i].srf_flags= SFF_FREE;
		ev_init(&sr_fd_table[i].srf_ioctl_ev);
		ev_init(&sr_fd_table[i].srf_read_ev);
		ev_init(&sr_fd_table[i].srf_write_ev);
	}
	repl_queue= NULL;
}

PUBLIC void sr_rec(m)
mq_t *m;
{
	int result;
	int send_reply, free_mess;

	if (repl_queue)
	{
		if (m->mq_mess.m_type == DEV_CANCEL)
		{
#ifdef __minix_vmd
			result= sr_repl_queue(m->mq_mess.NDEV_PROC,
				m->mq_mess.NDEV_REF, 
				m->mq_mess.NDEV_OPERATION);
#else /* Minix 3 */
			result= sr_repl_queue(m->mq_mess.PROC_NR, 0, 0);
#endif
			if (result)
			{
				mq_free(m);
				return;	/* canceled request in queue */
			}
		}
#if 0
		else
			sr_repl_queue(ANY, 0, 0);
#endif
	}

	switch (m->mq_mess.m_type)
	{
	case DEV_OPEN:
		result= sr_open(&m->mq_mess);
		send_reply= 1;
		free_mess= 1;
		break;
	case DEV_CLOSE:
		sr_close(&m->mq_mess);
		result= OK;
		send_reply= 1;
		free_mess= 1;
		break;
	case DEV_READ:
	case DEV_WRITE:
	case DEV_IOCTL3:
		result= sr_rwio(m);
		assert(result == OK || result == SUSPEND);
		send_reply= (result == SUSPEND);
		free_mess= 0;
		break;
	case DEV_CANCEL:
		result= sr_cancel(&m->mq_mess);
		assert(result == OK || result == EINTR);
		send_reply= (result == EINTR);
		free_mess= 1;
#ifdef __minix_vmd
		m->mq_mess.m_type= m->mq_mess.NDEV_OPERATION;
#else /* Minix 3 */
		m->mq_mess.m_type= 0;
#endif
		break;
#ifndef __minix_vmd /* Minix 3 */
	case DEV_SELECT:
		result= sr_select(&m->mq_mess);
		send_reply= 1;
		free_mess= 1;
		break;
	case DEV_STATUS:
		sr_status(&m->mq_mess);
		send_reply= 0;
		free_mess= 1;
		break;
#endif
	default:
		ip_panic(("unknown message, from %d, type %d",
				m->mq_mess.m_source, m->mq_mess.m_type));
	}
	if (send_reply)
	{
		sr_reply_(m, result, FALSE /* !is_revive */);
	}
	if (free_mess)
		mq_free(m);
}

PUBLIC void sr_add_minor(minor, port, openf, closef, readf, writef,
	ioctlf, cancelf, selectf)
int minor;
int port;
sr_open_t openf;
sr_close_t closef;
sr_read_t readf;
sr_write_t writef;
sr_ioctl_t ioctlf;
sr_cancel_t cancelf;
sr_select_t selectf;
{
	sr_fd_t *sr_fd;

	assert (minor>=0 && minor<FD_NR);

	sr_fd= &sr_fd_table[minor];

	assert(!(sr_fd->srf_flags & SFF_INUSE));

	sr_fd->srf_flags= SFF_INUSE | SFF_MINOR;
	sr_fd->srf_port= port;
	sr_fd->srf_open= openf;
	sr_fd->srf_close= closef;
	sr_fd->srf_write= writef;
	sr_fd->srf_read= readf;
	sr_fd->srf_ioctl= ioctlf;
	sr_fd->srf_cancel= cancelf;
	sr_fd->srf_select= selectf;
}

PRIVATE int sr_open(m)
message *m;
{
	sr_fd_t *sr_fd;

	int minor= m->NDEV_MINOR;
	int i, fd;

	if (minor<0 || minor>FD_NR)
	{
		DBLOCK(1, printf("replying EINVAL\n"));
		return EINVAL;
	}
	if (!(sr_fd_table[minor].srf_flags & SFF_MINOR))
	{
		DBLOCK(1, printf("replying ENXIO\n"));
		return ENXIO;
	}
	for (i=0; i<FD_NR && (sr_fd_table[i].srf_flags & SFF_INUSE); i++);

	if (i>=FD_NR)
	{
		DBLOCK(1, printf("replying ENFILE\n"));
		return ENFILE;
	}

	sr_fd= &sr_fd_table[i];
	*sr_fd= sr_fd_table[minor];
	sr_fd->srf_flags= SFF_INUSE;
	fd= (*sr_fd->srf_open)(sr_fd->srf_port, i, sr_get_userdata,
		sr_put_userdata, 0 /* no put_pkt */, sr_select_res);
	if (fd<0)
	{
		sr_fd->srf_flags= SFF_FREE;
		DBLOCK(1, printf("replying %d\n", fd));
		return fd;
	}
	sr_fd->srf_fd= fd;
	return i;
}

PRIVATE void sr_close(m)
message *m;
{
	sr_fd_t *sr_fd;

	sr_fd= sr_getchannel(m->NDEV_MINOR);
	assert (sr_fd);

	if (sr_fd->srf_flags & SFF_BUSY)
		ip_panic(("close on busy channel"));

	assert (!(sr_fd->srf_flags & SFF_MINOR));
	(*sr_fd->srf_close)(sr_fd->srf_fd);
	sr_fd->srf_flags= SFF_FREE;
}

PRIVATE int sr_rwio(m)
mq_t *m;
{
	sr_fd_t *sr_fd;
	mq_t **q_head_ptr, **q_tail_ptr;
	int ip_flag, susp_flag, first_flag;
	int r;
	ioreq_t request;
	size_t size;

	sr_fd= sr_getchannel(m->mq_mess.NDEV_MINOR);
	assert (sr_fd);

	switch(m->mq_mess.m_type)
	{
	case DEV_READ:
		q_head_ptr= &sr_fd->srf_read_q;
		q_tail_ptr= &sr_fd->srf_read_q_tail;
		ip_flag= SFF_READ_IP;
		susp_flag= SFF_READ_SUSP;
		first_flag= SFF_READ_FIRST;
		break;
	case DEV_WRITE:
		q_head_ptr= &sr_fd->srf_write_q;
		q_tail_ptr= &sr_fd->srf_write_q_tail;
		ip_flag= SFF_WRITE_IP;
		susp_flag= SFF_WRITE_SUSP;
		first_flag= SFF_WRITE_FIRST;
		break;
	case DEV_IOCTL3:
		q_head_ptr= &sr_fd->srf_ioctl_q;
		q_tail_ptr= &sr_fd->srf_ioctl_q_tail;
		ip_flag= SFF_IOCTL_IP;
		susp_flag= SFF_IOCTL_SUSP;
		first_flag= SFF_IOCTL_FIRST;
		break;
	default:
		ip_panic(("illegal case entry"));
	}

	if (sr_fd->srf_flags & ip_flag)
	{
		assert(sr_fd->srf_flags & susp_flag);
		assert(*q_head_ptr);

		(*q_tail_ptr)->mq_next= m;
		*q_tail_ptr= m;
		return SUSPEND;
	}
	assert(!*q_head_ptr);

	*q_tail_ptr= *q_head_ptr= m;
	sr_fd->srf_flags |= ip_flag;
	assert(!(sr_fd->srf_flags & first_flag));
	sr_fd->srf_flags |= first_flag;

	switch(m->mq_mess.m_type)
	{
	case DEV_READ:
		r= (*sr_fd->srf_read)(sr_fd->srf_fd, 
			m->mq_mess.NDEV_COUNT);
		break;
	case DEV_WRITE:
		r= (*sr_fd->srf_write)(sr_fd->srf_fd, 
			m->mq_mess.NDEV_COUNT);
		break;
	case DEV_IOCTL3:
		request= m->mq_mess.NDEV_IOCTL;

		/* There should be a better way to do this... */
		if (request == NWIOQUERYPARAM)
		{
			r= qp_query(m->mq_mess.NDEV_PROC,
				(vir_bytes)m->mq_mess.NDEV_BUFFER);
			r= sr_put_userdata(sr_fd-sr_fd_table, r, NULL, 1);
			assert(r == OK);
			return OK;
		}

		/* And now, we continue with our regular program. */
		size= (request >> 16) & _IOCPARM_MASK;
		if (size>MAX_IOCTL_S)
		{
			DBLOCK(1, printf("replying EINVAL\n"));
			r= sr_put_userdata(sr_fd-sr_fd_table, EINVAL, 
				NULL, 1);
			assert(r == OK);
			return OK;
		}
		r= (*sr_fd->srf_ioctl)(sr_fd->srf_fd, request);
		break;
	default:
		ip_panic(("illegal case entry"));
	}

	assert(sr_fd->srf_flags & first_flag);
	sr_fd->srf_flags &= ~first_flag;

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= susp_flag;
	else
		mq_free(m);
	return r;
}

PRIVATE int sr_restart_read(sr_fd)
sr_fd_t *sr_fd;
{
	mq_t *mp;
	int r;

	mp= sr_fd->srf_read_q;
	assert(mp);

	if (sr_fd->srf_flags & SFF_READ_IP)
	{
		assert(sr_fd->srf_flags & SFF_READ_SUSP);
		return SUSPEND;
	}
	sr_fd->srf_flags |= SFF_READ_IP;

	r= (*sr_fd->srf_read)(sr_fd->srf_fd, 
		mp->mq_mess.NDEV_COUNT);

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= SFF_READ_SUSP;
	return r;
}

PRIVATE int sr_restart_write(sr_fd)
sr_fd_t *sr_fd;
{
	mq_t *mp;
	int r;

	mp= sr_fd->srf_write_q;
	assert(mp);

	if (sr_fd->srf_flags & SFF_WRITE_IP)
	{
		assert(sr_fd->srf_flags & SFF_WRITE_SUSP);
		return SUSPEND;
	}
	sr_fd->srf_flags |= SFF_WRITE_IP;

	r= (*sr_fd->srf_write)(sr_fd->srf_fd, 
		mp->mq_mess.NDEV_COUNT);

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= SFF_WRITE_SUSP;
	return r;
}

PRIVATE int sr_restart_ioctl(sr_fd)
sr_fd_t *sr_fd;
{
	mq_t *mp;
	int r;

	mp= sr_fd->srf_ioctl_q;
	assert(mp);

	if (sr_fd->srf_flags & SFF_IOCTL_IP)
	{
		assert(sr_fd->srf_flags & SFF_IOCTL_SUSP);
		return SUSPEND;
	}
	sr_fd->srf_flags |= SFF_IOCTL_IP;

	r= (*sr_fd->srf_ioctl)(sr_fd->srf_fd, 
		mp->mq_mess.NDEV_COUNT);

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= SFF_IOCTL_SUSP;
	return r;
}

PRIVATE int sr_cancel(m)
message *m;
{
	sr_fd_t *sr_fd;
	int result;
	int proc_nr, ref, operation;

        result=EINTR;
	proc_nr=  m->NDEV_PROC;
#ifdef __minix_vmd
	ref=  m->NDEV_REF;
	operation= m->NDEV_OPERATION;
#else /* Minix 3 */
	ref=  0;
	operation= 0;
#endif
	sr_fd= sr_getchannel(m->NDEV_MINOR);
	assert (sr_fd);

#ifdef __minix_vmd
	if (operation == CANCEL_ANY || operation == DEV_IOCTL3)
#endif
	{
		result= walk_queue(sr_fd, &sr_fd->srf_ioctl_q, 
			&sr_fd->srf_ioctl_q_tail, SR_CANCEL_IOCTL,
			proc_nr, ref, SFF_IOCTL_FIRST);
		if (result != EAGAIN)
			return result;
	}
#ifdef __minix_vmd
	if (operation == CANCEL_ANY || operation == DEV_READ)
#endif
	{
		result= walk_queue(sr_fd, &sr_fd->srf_read_q, 
			&sr_fd->srf_read_q_tail, SR_CANCEL_READ,
			proc_nr, ref, SFF_READ_FIRST);
		if (result != EAGAIN)
			return result;
	}
#ifdef __minix_vmd
	if (operation == CANCEL_ANY || operation == DEV_WRITE)
#endif
	{
		result= walk_queue(sr_fd, &sr_fd->srf_write_q, 
			&sr_fd->srf_write_q_tail, SR_CANCEL_WRITE,
			proc_nr, ref, SFF_WRITE_FIRST);
		if (result != EAGAIN)
			return result;
	}
#ifdef __minix_vmd
	ip_panic((
"request not found: from %d, type %d, MINOR= %d, PROC= %d, REF= %d OPERATION= %ld",
		m->m_source, m->m_type, m->NDEV_MINOR,
		m->NDEV_PROC, m->NDEV_REF, m->NDEV_OPERATION));
#else /* Minix 3 */
	ip_panic((
"request not found: from %d, type %d, MINOR= %d, PROC= %d",
		m->m_source, m->m_type, m->NDEV_MINOR,
		m->NDEV_PROC));
#endif
}

#ifndef __minix_vmd /* Minix 3 */
PRIVATE int sr_select(m)
message *m;
{
	sr_fd_t *sr_fd;
	mq_t **q_head_ptr, **q_tail_ptr;
	int ip_flag, susp_flag;
	int r, ops;
	unsigned m_ops, i_ops;
	ioreq_t request;
	size_t size;

	sr_fd= sr_getchannel(m->NDEV_MINOR);
	assert (sr_fd);

	sr_fd->srf_select_proc= m->m_source;

	m_ops= m->PROC_NR;
	i_ops= 0;
	if (m_ops & SEL_RD) i_ops |= SR_SELECT_READ;
	if (m_ops & SEL_WR) i_ops |= SR_SELECT_WRITE;
	if (m_ops & SEL_ERR) i_ops |= SR_SELECT_EXCEPTION;
	if (!(m_ops & SEL_NOTIFY)) i_ops |= SR_SELECT_POLL;

	r= (*sr_fd->srf_select)(sr_fd->srf_fd,  i_ops);
	if (r < 0)
		return r;
	m_ops= 0;
	if (r & SR_SELECT_READ) m_ops |= SEL_RD;
	if (r & SR_SELECT_WRITE) m_ops |= SEL_WR;
	if (r & SR_SELECT_EXCEPTION) m_ops |= SEL_ERR;

	return m_ops;
}

PRIVATE void sr_status(m)
message *m;
{
	int fd, result;
	unsigned m_ops;
	sr_fd_t *sr_fd;
	mq_t *mq;

	mq= repl_queue;
	if (mq != NULL)
	{
		repl_queue= mq->mq_next;

		mq->mq_mess.m_type= DEV_REVIVE;
		result= send(mq->mq_mess.m_source, &mq->mq_mess);
		if (result != OK)
			ip_panic(("unable to send"));
		mq_free(mq);

		return;
	}

	for (fd=0, sr_fd= sr_fd_table; fd<FD_NR; fd++, sr_fd++)
	{
		if ((sr_fd->srf_flags &
			(SFF_SELECT_R|SFF_SELECT_W|SFF_SELECT_X)) == 0)
		{
			/* Nothing to report */
			continue;
		}
		if (sr_fd->srf_select_proc != m->m_source)
		{
			/* Wrong process */
			continue;
		}

		m_ops= 0;
		if (sr_fd->srf_flags & SFF_SELECT_R) m_ops |= SEL_RD;
		if (sr_fd->srf_flags & SFF_SELECT_W) m_ops |= SEL_WR;
		if (sr_fd->srf_flags & SFF_SELECT_X) m_ops |= SEL_ERR;

		sr_fd->srf_flags &= ~(SFF_SELECT_R|SFF_SELECT_W|SFF_SELECT_X);

		m->m_type= DEV_IO_READY;
		m->DEV_MINOR= fd;
		m->DEV_SEL_OPS= m_ops;

		result= send(m->m_source, m);
		if (result != OK)
			ip_panic(("unable to send"));
		return;
	}

	m->m_type= DEV_NO_STATUS;
	result= send(m->m_source, m);
	if (result != OK)
		ip_panic(("unable to send"));
}
#endif

PRIVATE int walk_queue(sr_fd, q_head_ptr, q_tail_ptr, type, proc_nr, ref,
	first_flag)
sr_fd_t *sr_fd;
mq_t **q_head_ptr;
mq_t **q_tail_ptr;
int type;
int proc_nr;
int ref;
int first_flag;
{
	mq_t *q_ptr_prv, *q_ptr;
	int result;

	for(q_ptr_prv= NULL, q_ptr= *q_head_ptr; q_ptr; 
		q_ptr_prv= q_ptr, q_ptr= q_ptr->mq_next)
	{
		if (q_ptr->mq_mess.NDEV_PROC != proc_nr)
			continue;
#ifdef __minix_vmd
		if (q_ptr->mq_mess.NDEV_REF != ref)
			continue;
#endif
		if (!q_ptr_prv)
		{
			assert(!(sr_fd->srf_flags & first_flag));
			sr_fd->srf_flags |= first_flag;

			result= (*sr_fd->srf_cancel)(sr_fd->srf_fd, type);
			assert(result == OK);

			*q_head_ptr= q_ptr->mq_next;
			mq_free(q_ptr);

			assert(sr_fd->srf_flags & first_flag);
			sr_fd->srf_flags &= ~first_flag;

			return OK;
		}
		q_ptr_prv->mq_next= q_ptr->mq_next;
		mq_free(q_ptr);
		if (!q_ptr_prv->mq_next)
			*q_tail_ptr= q_ptr_prv;
		return EINTR;
	}
	return EAGAIN;
}

PRIVATE sr_fd_t *sr_getchannel(minor)
int minor;
{
	sr_fd_t *loc_fd;

	compare(minor, >=, 0);
	compare(minor, <, FD_NR);

	loc_fd= &sr_fd_table[minor];

	assert (!(loc_fd->srf_flags & SFF_MINOR) &&
		(loc_fd->srf_flags & SFF_INUSE));

	return loc_fd;
}

PRIVATE void sr_reply_(mq, status, is_revive)
mq_t *mq;
int status;
int is_revive;
{
	int result, proc, ref,operation;
	message reply, *mp;

	proc= mq->mq_mess.NDEV_PROC;
#ifdef __minix_vmd
	ref= mq->mq_mess.NDEV_REF;
#else /* Minix 3 */
	ref= 0;
#endif
	operation= mq->mq_mess.m_type;
#ifdef __minix_vmd
	assert(operation != DEV_CANCEL);
#endif

	if (is_revive)
		mp= &mq->mq_mess;
	else
		mp= &reply;

	mp->m_type= DEVICE_REPLY;
	mp->REP_PROC_NR= proc;
	mp->REP_STATUS= status;
#ifdef __minix_vmd
	mp->REP_REF= ref;
	mp->REP_OPERATION= operation;
#endif
	if (is_revive)
	{
		notify(mq->mq_mess.m_source);
		result= ELOCKED;
	}
	else
		result= send(mq->mq_mess.m_source, mp);

	if (result == ELOCKED && is_revive)
	{
		mq->mq_next= NULL;
		if (repl_queue)
			repl_queue_tail->mq_next= mq;
		else
			repl_queue= mq;
		repl_queue_tail= mq;
		return;
	}
	if (result != OK)
		ip_panic(("unable to send"));
	if (is_revive)
		mq_free(mq);
}

PRIVATE acc_t *sr_get_userdata (fd, offset, count, for_ioctl)
int fd;
vir_bytes offset;
vir_bytes count;
int for_ioctl;
{
	sr_fd_t *loc_fd;
	mq_t **head_ptr, *m, *mq;
	int ip_flag, susp_flag, first_flag;
	int result, suspended, is_revive;
	char *src;
	acc_t *acc;
	event_t *evp;
	ev_arg_t arg;

	loc_fd= &sr_fd_table[fd];

	if (for_ioctl)
	{
		head_ptr= &loc_fd->srf_ioctl_q;
		evp= &loc_fd->srf_ioctl_ev;
		ip_flag= SFF_IOCTL_IP;
		susp_flag= SFF_IOCTL_SUSP;
		first_flag= SFF_IOCTL_FIRST;
	}
	else
	{
		head_ptr= &loc_fd->srf_write_q;
		evp= &loc_fd->srf_write_ev;
		ip_flag= SFF_WRITE_IP;
		susp_flag= SFF_WRITE_SUSP;
		first_flag= SFF_WRITE_FIRST;
	}
		
assert (loc_fd->srf_flags & ip_flag);

	if (!count)
	{
		m= *head_ptr;
		mq= m->mq_next;
		*head_ptr= mq;
		result= (int)offset;
		is_revive= !(loc_fd->srf_flags & first_flag);
		sr_reply_(m, result, is_revive);
		suspended= (loc_fd->srf_flags & susp_flag);
		loc_fd->srf_flags &= ~(ip_flag|susp_flag);
		if (suspended)
		{
			if (mq)
			{
				arg.ev_ptr= loc_fd;
				ev_enqueue(evp, sr_event, arg);
			}
		}
		return NULL;
	}

	src= (*head_ptr)->mq_mess.NDEV_BUFFER + offset;
	result= cp_u2b ((*head_ptr)->mq_mess.NDEV_PROC, src, &acc, count);

	return result<0 ? NULL : acc;
}

PRIVATE int sr_put_userdata (fd, offset, data, for_ioctl)
int fd;
vir_bytes offset;
acc_t *data;
int for_ioctl;
{
	sr_fd_t *loc_fd;
	mq_t **head_ptr, *m, *mq;
	int ip_flag, susp_flag, first_flag;
	int result, suspended, is_revive;
	char *dst;
	event_t *evp;
	ev_arg_t arg;

	loc_fd= &sr_fd_table[fd];

	if (for_ioctl)
	{
		head_ptr= &loc_fd->srf_ioctl_q;
		evp= &loc_fd->srf_ioctl_ev;
		ip_flag= SFF_IOCTL_IP;
		susp_flag= SFF_IOCTL_SUSP;
		first_flag= SFF_IOCTL_FIRST;
	}
	else
	{
		head_ptr= &loc_fd->srf_read_q;
		evp= &loc_fd->srf_read_ev;
		ip_flag= SFF_READ_IP;
		susp_flag= SFF_READ_SUSP;
		first_flag= SFF_READ_FIRST;
	}
		
	assert (loc_fd->srf_flags & ip_flag);

	if (!data)
	{
		m= *head_ptr;
		mq= m->mq_next;
		*head_ptr= mq;
		result= (int)offset;
		is_revive= !(loc_fd->srf_flags & first_flag);
		sr_reply_(m, result, is_revive);
		suspended= (loc_fd->srf_flags & susp_flag);
		loc_fd->srf_flags &= ~(ip_flag|susp_flag);
		if (suspended)
		{
			if (mq)
			{
				arg.ev_ptr= loc_fd;
				ev_enqueue(evp, sr_event, arg);
			}
		}
		return OK;
	}

	dst= (*head_ptr)->mq_mess.NDEV_BUFFER + offset;
	return cp_b2u (data, (*head_ptr)->mq_mess.NDEV_PROC, dst);
}

#ifndef __minix_vmd /* Minix 3 */
PRIVATE void sr_select_res(fd, ops)
int fd;
unsigned ops;
{
	sr_fd_t *sr_fd;

	sr_fd= &sr_fd_table[fd];
	
	if (ops & SR_SELECT_READ) sr_fd->srf_flags |= SFF_SELECT_R;
	if (ops & SR_SELECT_WRITE) sr_fd->srf_flags |= SFF_SELECT_W;
	if (ops & SR_SELECT_EXCEPTION) sr_fd->srf_flags |= SFF_SELECT_X;

	notify(sr_fd->srf_select_proc);
}
#endif

PRIVATE void process_req_q(mq, tail, tail_ptr)
mq_t *mq, *tail, **tail_ptr;
{
	mq_t *m;
	int result;

	for(;mq;)
	{
		m= mq;
		mq= mq->mq_next;

		result= sr_rwio(m);
		if (result == SUSPEND)
		{
			if (mq)
			{
				(*tail_ptr)->mq_next= mq;
				*tail_ptr= tail;
			}
			return;
		}
	}
	return;
}

PRIVATE void sr_event(evp, arg)
event_t *evp;
ev_arg_t arg;
{
	sr_fd_t *sr_fd;
	int r;

	sr_fd= arg.ev_ptr;
	if (evp == &sr_fd->srf_write_ev)
	{
		while(sr_fd->srf_write_q)
		{
			r= sr_restart_write(sr_fd);
			if (r == SUSPEND)
				return;
		}
		return;
	}
	if (evp == &sr_fd->srf_read_ev)
	{
		while(sr_fd->srf_read_q)
		{
			r= sr_restart_read(sr_fd);
			if (r == SUSPEND)
				return;
		}
		return;
	}
	if (evp == &sr_fd->srf_ioctl_ev)
	{
		while(sr_fd->srf_ioctl_q)
		{
			r= sr_restart_ioctl(sr_fd);
			if (r == SUSPEND)
				return;
		}
		return;
	}
	ip_panic(("sr_event: unkown event\n"));
}

PRIVATE int cp_u2b (proc, src, var_acc_ptr, size)
int proc;
char *src;
acc_t **var_acc_ptr;
int size;
{
	static message mess;
	acc_t *acc;
	int i;

	acc= bf_memreq(size);

	*var_acc_ptr= acc;
	i=0;

	while (acc)
	{
		size= (vir_bytes)acc->acc_length;

#ifdef __minix_vmd
		cpvec[i].cpv_src= (vir_bytes)src;
		cpvec[i].cpv_dst= (vir_bytes)ptr2acc_data(acc);
		cpvec[i].cpv_size= size;
#else /* Minix 3 */
		vir_cp_req[i].count= size;
		vir_cp_req[i].src.proc_nr = proc;
		vir_cp_req[i].src.segment = D;
		vir_cp_req[i].src.offset = (vir_bytes) src;
		vir_cp_req[i].dst.proc_nr = this_proc;
		vir_cp_req[i].dst.segment = D;
		vir_cp_req[i].dst.offset = (vir_bytes) ptr2acc_data(acc);
#endif

		src += size;
		acc= acc->acc_next;
		i++;

		if (i == CPVEC_NR || acc == NULL)
		{
#ifdef __minix_vmd
			mess.m_type= SYS_VCOPY;
			mess.m1_i1= proc;
			mess.m1_i2= this_proc;
			mess.m1_i3= i;
			mess.m1_p1= (char *)cpvec;
#else /* Minix 3 */
			mess.m_type= SYS_VIRVCOPY;
			mess.VCP_VEC_SIZE= i;
			mess.VCP_VEC_ADDR= (char *)vir_cp_req;
#endif
			if (sendrec(SYSTASK, &mess) <0)
				ip_panic(("unable to sendrec"));
			if (mess.m_type <0)
			{
				bf_afree(*var_acc_ptr);
				*var_acc_ptr= 0;
				return mess.m_type;
			}
			i= 0;
		}
	}
	return OK;
}

PRIVATE int cp_b2u (acc_ptr, proc, dest)
acc_t *acc_ptr;
int proc;
char *dest;
{
	static message mess;
	acc_t *acc;
	int i, size;

	acc= acc_ptr;
	i=0;

	while (acc)
	{
		size= (vir_bytes)acc->acc_length;

		if (size)
		{
#ifdef __minix_vmd
			cpvec[i].cpv_src= (vir_bytes)ptr2acc_data(acc);
			cpvec[i].cpv_dst= (vir_bytes)dest;
			cpvec[i].cpv_size= size;
#else /* Minix 3 */
			vir_cp_req[i].src.proc_nr = this_proc;
			vir_cp_req[i].src.segment = D;
			vir_cp_req[i].src.offset= (vir_bytes)ptr2acc_data(acc);
			vir_cp_req[i].dst.proc_nr = proc;
			vir_cp_req[i].dst.segment = D;
			vir_cp_req[i].dst.offset= (vir_bytes)dest;
			vir_cp_req[i].count= size;
#endif
			i++;
		}

		dest += size;
		acc= acc->acc_next;

		if (i == CPVEC_NR || acc == NULL)
		{
#ifdef __minix_vmd
			mess.m_type= SYS_VCOPY;
			mess.m1_i1= this_proc;
			mess.m1_i2= proc;
			mess.m1_i3= i;
			mess.m1_p1= (char *)cpvec;
#else /* Minix 3 */
			mess.m_type= SYS_VIRVCOPY;
			mess.VCP_VEC_SIZE= i;
			mess.VCP_VEC_ADDR= (char *) vir_cp_req;
#endif
			if (sendrec(SYSTASK, &mess) <0)
				ip_panic(("unable to sendrec"));
			if (mess.m_type <0)
			{
				bf_afree(acc_ptr);
				return mess.m_type;
			}
			i= 0;
		}
	}
	bf_afree(acc_ptr);
	return OK;
}

PRIVATE int sr_repl_queue(proc, ref, operation)
int proc;
int ref;
int operation;
{
	mq_t *m, *m_cancel, *m_tmp;
	int result;

	m_cancel= NULL;

	for (m= repl_queue; m;)
	{
#ifdef __minix_vmd
		if (m->mq_mess.REP_PROC_NR == proc && 
			m->mq_mess.REP_REF ==ref &&
			(m->mq_mess.REP_OPERATION == operation ||
				operation == CANCEL_ANY))
#else /* Minix 3 */
		if (m->mq_mess.REP_PROC_NR == proc)
#endif
		{
assert(!m_cancel);
			m_cancel= m;
			m= m->mq_next;
			continue;
		}
		result= send(m->mq_mess.m_source, &m->mq_mess);
		if (result != OK)
			ip_panic(("unable to send: %d", result));
		m_tmp= m;
		m= m->mq_next;
		mq_free(m_tmp);
	}
	repl_queue= NULL;
	if (m_cancel)
	{
		result= send(m_cancel->mq_mess.m_source, &m_cancel->mq_mess);
		if (result != OK)
			ip_panic(("unable to send: %d", result));
		mq_free(m_cancel);
		return 1;
	}
	return 0;
}

/*
 * $PchId: sr.c,v 1.17 2005/06/28 14:26:16 philip Exp $
 */
