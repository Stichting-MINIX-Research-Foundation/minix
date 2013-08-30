/*	this file contains the interface of the network software with the file
 *	system.
 *
 * Copyright 1995 Philip Homburg
 *
 * The valid messages and their parameters are:
 * 
 * Requests:
 *
 *    m_type      DEVICE      USER_ENDPT    COUNT
 * --------------------------------------------------
 * | DEV_OPEN    | minor dev | proc nr   |   mode   |
 * |-------------+-----------+-----------+----------+
 * | DEV_CLOSE   | minor dev | proc nr   |          |
 * |-------------+-----------+-----------+----------+
 *
 *    m_type      DEVICE      USER_ENDPT    COUNT      IO_GRANT
 * ---------------------------------------------------------------
 * | DEV_READ_S  | minor dev | proc nr   |  count    | grant ID  |
 * |-------------+-----------+-----------+-----------+-----------|
 * | DEV_WRITE_S | minor dev | proc nr   |  count    | grant ID  |
 * |-------------+-----------+-----------+-----------+-----------|
 * | DEV_IOCTL_S | minor dev | proc nr   |  command  | grant ID  |
 * |-------------+-----------+-----------+-----------+-----------|
 * | DEV_SELECT  | minor dev | ops       |           |           |
 * |-------------+-----------+-----------+-----------+-----------|
 *
 *    m_type      DEVICE      USER_ENDPT    COUNT
 * --------------------------------------------------|
 * | CANCEL      | minor dev | proc nr   |  mode     |
 * |-------------+-----------+-----------+-----------|
 */

#include "inet.h"

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

THIS_FILE

sr_fd_t sr_fd_table[FD_NR];

static struct vscp_vec s_cp_req[SCPVEC_NR];

static int sr_open(message *m);
static void sr_close(message *m);
static int sr_rwio(mq_t *m);
static int sr_restart_read(sr_fd_t *fdp);
static int sr_restart_write(sr_fd_t *fdp);
static int sr_restart_ioctl(sr_fd_t *fdp);
static int sr_cancel(message *m);
static void sr_select(message *m);
static void sr_reply_(mq_t *m, int code, int reply, int is_revive);
static sr_fd_t *sr_getchannel(int minor);
static acc_t *sr_get_userdata(int fd, size_t offset, size_t count, int
	for_ioctl);
static int sr_put_userdata(int fd, size_t offset, acc_t *data, int
	for_ioctl);
static void sr_select_res(int fd, unsigned ops);
static int walk_queue(sr_fd_t *sr_fd, mq_t **q_head_ptr, mq_t
	**q_tail_ptr, int type, int proc_nr, int ref, int first_flag);
static void sr_event(event_t *evp, ev_arg_t arg);
static int cp_u2b(endpoint_t proc, cp_grant_id_t gid, vir_bytes offset,
	acc_t **var_acc_ptr, int size);
static int cp_b2u(acc_t *acc_ptr, endpoint_t proc, cp_grant_id_t gid,
	vir_bytes offset);

void sr_init()
{
	int i;

	for (i=0; i<FD_NR; i++)
	{
		sr_fd_table[i].srf_flags= SFF_FREE;
		ev_init(&sr_fd_table[i].srf_ioctl_ev);
		ev_init(&sr_fd_table[i].srf_read_ev);
		ev_init(&sr_fd_table[i].srf_write_ev);
	}
}

void sr_rec(m)
mq_t *m;
{
	int code = DEV_REVIVE, result;
	int send_reply = 0, free_mess = 0;

	switch (m->mq_mess.m_type)
	{
	case DEV_OPEN:
		result= sr_open(&m->mq_mess);
		code= DEV_OPEN_REPL;
		send_reply= 1;
		free_mess= 1;
		break;
	case DEV_CLOSE:
		sr_close(&m->mq_mess);
		result= OK;
		code= DEV_CLOSE_REPL;
		send_reply= 1;
		free_mess= 1;
		break;
	case DEV_READ_S:
	case DEV_WRITE_S:
	case DEV_IOCTL_S:
		result= sr_rwio(m);
		assert(result == OK || result == EAGAIN || result == EINTR ||
			result == SUSPEND);
		send_reply= (result == EAGAIN);
		free_mess= (result == EAGAIN);
		break;
	case CANCEL:
		result= sr_cancel(&m->mq_mess);
		assert(result == OK || result == EINTR || result == SUSPEND);
		send_reply= (result == EINTR);
		free_mess= 1;
		break;
	case DEV_SELECT:
		sr_select(&m->mq_mess);
		send_reply= 0;
		free_mess= 1;
		break;
	default:
		ip_panic(("unknown message, from %d, type %d",
				m->mq_mess.m_source, m->mq_mess.m_type));
	}
	if (send_reply)
	{
		sr_reply_(m, code, result, FALSE /* !is_revive */);
	}
	if (free_mess)
		mq_free(m);
}

void sr_add_minor(minor, port, openf, closef, readf, writef,
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

static int sr_open(m)
message *m;
{
	sr_fd_t *sr_fd;

	int minor= m->DEVICE;
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

static void sr_close(m)
message *m;
{
	sr_fd_t *sr_fd;

	sr_fd= sr_getchannel(m->DEVICE);
	assert (sr_fd);

	if (sr_fd->srf_flags & SFF_BUSY)
		ip_panic(("close on busy channel"));

	assert (!(sr_fd->srf_flags & SFF_MINOR));
	(*sr_fd->srf_close)(sr_fd->srf_fd);
	sr_fd->srf_flags= SFF_FREE;
}

static int sr_rwio(m)
mq_t *m;
{
	sr_fd_t *sr_fd;
	mq_t **q_head_ptr = NULL, **q_tail_ptr = NULL;
	int ip_flag = 0, susp_flag = 0, first_flag = 0;
	int r = OK;
	ioreq_t request;
	size_t size;

	sr_fd= sr_getchannel(m->mq_mess.DEVICE);
	assert (sr_fd);

	switch(m->mq_mess.m_type)
	{
	case DEV_READ_S:
		q_head_ptr= &sr_fd->srf_read_q;
		q_tail_ptr= &sr_fd->srf_read_q_tail;
		ip_flag= SFF_READ_IP;
		susp_flag= SFF_READ_SUSP;
		first_flag= SFF_READ_FIRST;
		break;
	case DEV_WRITE_S:
		q_head_ptr= &sr_fd->srf_write_q;
		q_tail_ptr= &sr_fd->srf_write_q_tail;
		ip_flag= SFF_WRITE_IP;
		susp_flag= SFF_WRITE_SUSP;
		first_flag= SFF_WRITE_FIRST;
		break;
	case DEV_IOCTL_S:
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

		if (m->mq_mess.FLAGS & FLG_OP_NONBLOCK)
			return EAGAIN;

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
	case DEV_READ_S:
		r= (*sr_fd->srf_read)(sr_fd->srf_fd, 
			m->mq_mess.COUNT);
		break;
	case DEV_WRITE_S:
		r= (*sr_fd->srf_write)(sr_fd->srf_fd, 
			m->mq_mess.COUNT);
		break;
	case DEV_IOCTL_S:
		request= m->mq_mess.REQUEST;
		size= (request >> 16) & _IOCPARM_MASK;
		if (size>MAX_IOCTL_S)
		{
			DBLOCK(1, printf("replying EINVAL\n"));
			r= sr_put_userdata(sr_fd-sr_fd_table, EINVAL, 
				NULL, 1);
			assert(r == OK);
			assert(sr_fd->srf_flags & first_flag);
			sr_fd->srf_flags &= ~first_flag;
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
	if (r == SUSPEND) {
		sr_fd->srf_flags |= susp_flag;
		if (m->mq_mess.FLAGS & FLG_OP_NONBLOCK) {
			r= sr_cancel(&m->mq_mess);
			assert(r == OK); /* must have been head of queue */
			return EINTR;
		}
	} else
		mq_free(m);
	return r;
}

static int sr_restart_read(sr_fd)
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
		mp->mq_mess.COUNT);

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= SFF_READ_SUSP;
	return r;
}

static int sr_restart_write(sr_fd)
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
		mp->mq_mess.COUNT);

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= SFF_WRITE_SUSP;
	return r;
}

static int sr_restart_ioctl(sr_fd)
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
		mp->mq_mess.COUNT);

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= SFF_IOCTL_SUSP;
	return r;
}

static int sr_cancel(m)
message *m;
{
	sr_fd_t *sr_fd;
	int result;
	int proc_nr, ref;

	proc_nr=  m->USER_ENDPT;
	ref=  (int)m->IO_GRANT;
	sr_fd= sr_getchannel(m->DEVICE);
	assert (sr_fd);

	result= walk_queue(sr_fd, &sr_fd->srf_ioctl_q,
		&sr_fd->srf_ioctl_q_tail, SR_CANCEL_IOCTL,
		proc_nr, ref, SFF_IOCTL_FIRST);
	if (result != EAGAIN)
		return result;

	result= walk_queue(sr_fd, &sr_fd->srf_read_q, 
		&sr_fd->srf_read_q_tail, SR_CANCEL_READ,
		proc_nr, ref, SFF_READ_FIRST);
	if (result != EAGAIN)
		return result;

	result= walk_queue(sr_fd, &sr_fd->srf_write_q, 
		&sr_fd->srf_write_q_tail, SR_CANCEL_WRITE,
		proc_nr, ref, SFF_WRITE_FIRST);
	if (result != EAGAIN)
		return result;

	/* We already replied to the request, so don't reply to the CANCEL. */
	return SUSPEND;
}

static void sr_select(m)
message *m;
{
	message m_reply;
	sr_fd_t *sr_fd;
	int r;
	unsigned m_ops, i_ops;

	sr_fd= sr_getchannel(m->DEV_MINOR);
	assert (sr_fd);

	sr_fd->srf_select_proc= m->m_source;

	m_ops= m->DEV_SEL_OPS;
	i_ops= 0;
	if (m_ops & SEL_RD) i_ops |= SR_SELECT_READ;
	if (m_ops & SEL_WR) i_ops |= SR_SELECT_WRITE;
	if (m_ops & SEL_ERR) i_ops |= SR_SELECT_EXCEPTION;
	if (!(m_ops & SEL_NOTIFY)) i_ops |= SR_SELECT_POLL;

	r= (*sr_fd->srf_select)(sr_fd->srf_fd,  i_ops);
	if (r < 0) {
		m_ops= r;
	} else {
		m_ops= 0;
		if (r & SR_SELECT_READ) m_ops |= SEL_RD;
		if (r & SR_SELECT_WRITE) m_ops |= SEL_WR;
		if (r & SR_SELECT_EXCEPTION) m_ops |= SEL_ERR;
	}

	memset(&m_reply, 0, sizeof(m_reply));
	m_reply.m_type= DEV_SEL_REPL1;
	m_reply.DEV_MINOR= m->DEV_MINOR;
	m_reply.DEV_SEL_OPS= m_ops;

	r= send(m->m_source, &m_reply);
	if (r != OK)
		ip_panic(("unable to send"));
}

static int walk_queue(sr_fd, q_head_ptr, q_tail_ptr, type, proc_nr, ref,
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
		if (q_ptr->mq_mess.USER_ENDPT != proc_nr)
			continue;
		if ((int)q_ptr->mq_mess.IO_GRANT != ref)
			continue;
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

static sr_fd_t *sr_getchannel(minor)
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

static void sr_reply_(mq, code, status, is_revive)
mq_t *mq;
int code;
int status;
int is_revive;
{
	int result, proc, ref;
	message reply, *mp;

	proc= mq->mq_mess.USER_ENDPT;
	ref= (int)mq->mq_mess.IO_GRANT;

	if (is_revive)
		mp= &mq->mq_mess;
	else
		mp= &reply;

	mp->m_type= code;
	mp->REP_ENDPT= proc;
	mp->REP_STATUS= status;
	mp->REP_IO_GRANT= ref;
	result= send(mq->mq_mess.m_source, mp);

	if (result != OK)
		ip_panic(("unable to send"));
	if (is_revive)
		mq_free(mq);
}

static acc_t *sr_get_userdata (fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	sr_fd_t *loc_fd;
	mq_t **head_ptr, *m, *mq;
	int ip_flag, susp_flag, first_flag;
	int result, suspended, is_revive;
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
		sr_reply_(m, DEV_REVIVE, result, is_revive);
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

	result= cp_u2b ((*head_ptr)->mq_mess.m_source,
		(int)(*head_ptr)->mq_mess.IO_GRANT, offset, &acc, count);

	return result<0 ? NULL : acc;
}

static int sr_put_userdata (fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	sr_fd_t *loc_fd;
	mq_t **head_ptr, *m, *mq;
	int ip_flag, susp_flag, first_flag;
	int result, suspended, is_revive;
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
		sr_reply_(m, DEV_REVIVE, result, is_revive);
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

	return cp_b2u (data, (*head_ptr)->mq_mess.m_source, 
		(int)(*head_ptr)->mq_mess.IO_GRANT, offset);
}

static void sr_select_res(int fd, unsigned ops)
{
	message m;
	sr_fd_t *sr_fd;
	unsigned int m_ops;
	int result;

	sr_fd= &sr_fd_table[fd];

	m_ops= 0;
	if (ops & SR_SELECT_READ) m_ops |= SEL_RD;
	if (ops & SR_SELECT_WRITE) m_ops |= SEL_WR;
	if (ops & SR_SELECT_EXCEPTION) m_ops |= SEL_ERR;

	memset(&m, 0, sizeof(m));
	m.m_type= DEV_SEL_REPL2;
	m.DEV_MINOR= fd;
	m.DEV_SEL_OPS= m_ops;

	result= send(sr_fd->srf_select_proc, &m);
	if (result != OK)
		ip_panic(("unable to send"));
}

static void sr_event(evp, arg)
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
	ip_panic(("sr_event: unknown event\n"));
}

static int cp_u2b(proc, gid, offset, var_acc_ptr, size)
endpoint_t proc;
cp_grant_id_t gid;
vir_bytes offset;
acc_t **var_acc_ptr;
int size;
{
	acc_t *acc;
	int i, r;

	acc= bf_memreq(size);

	*var_acc_ptr= acc;
	i=0;

	while (acc)
	{
		size= (vir_bytes)acc->acc_length;

		s_cp_req[i].v_from= proc;
		s_cp_req[i].v_to= SELF;
		s_cp_req[i].v_gid= gid;
		s_cp_req[i].v_offset= offset;
		s_cp_req[i].v_addr= (vir_bytes) ptr2acc_data(acc);
		s_cp_req[i].v_bytes= size;

		offset += size;
		acc= acc->acc_next;
		i++;

		if (acc == NULL && i == 1)
		{
			r= sys_safecopyfrom(s_cp_req[0].v_from,
				s_cp_req[0].v_gid, s_cp_req[0].v_offset,
				s_cp_req[0].v_addr, s_cp_req[0].v_bytes);
			if (r <0)
			{
				printf("sys_safecopyfrom failed: %d\n", r);
				bf_afree(*var_acc_ptr);
				*var_acc_ptr= 0;
				return r;
			}
			i= 0;
			continue;
		}
		if (i == SCPVEC_NR || acc == NULL)
		{
			r= sys_vsafecopy(s_cp_req, i);

			if (r <0)
			{
				printf("cp_u2b: sys_vsafecopy failed: %d\n",
					r);
				bf_afree(*var_acc_ptr);
				*var_acc_ptr= 0;
				return r;
			}
			i= 0;
		}
	}
	return OK;
}

static int cp_b2u(acc_ptr, proc, gid, offset)
acc_t *acc_ptr;
endpoint_t proc;
cp_grant_id_t gid;
vir_bytes offset;
{
	acc_t *acc;
	int i, r, size;

	acc= acc_ptr;
	i=0;

	while (acc)
	{
		size= (vir_bytes)acc->acc_length;

		if (size)
		{
			s_cp_req[i].v_from= SELF;
			s_cp_req[i].v_to= proc;
			s_cp_req[i].v_gid= gid;
			s_cp_req[i].v_offset= offset;
			s_cp_req[i].v_addr= (vir_bytes) ptr2acc_data(acc);
			s_cp_req[i].v_bytes= size;

			i++;
		}

		offset += size;
		acc= acc->acc_next;

		if (acc == NULL && i == 1)
		{
			r= sys_safecopyto(s_cp_req[0].v_to,
				s_cp_req[0].v_gid, s_cp_req[0].v_offset,
				s_cp_req[0].v_addr, s_cp_req[0].v_bytes);
			if (r <0)
			{
				printf("sys_safecopyto failed: %d\n", r);
				bf_afree(acc_ptr);
				return r;
			}
			i= 0;
			continue;
		}
		if (i == SCPVEC_NR || acc == NULL)
		{
			r= sys_vsafecopy(s_cp_req, i);

			if (r <0)
			{
				printf("cp_b2u: sys_vsafecopy failed: %d\n",
					r);
				bf_afree(acc_ptr);
				return r;
			}
			i= 0;
		}
	}
	bf_afree(acc_ptr);
	return OK;
}

/*
 * $PchId: sr.c,v 1.17 2005/06/28 14:26:16 philip Exp $
 */
