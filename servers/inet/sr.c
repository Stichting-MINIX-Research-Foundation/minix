/*	this file contains the interface of the network software with the file
 *	system.
 *
 * Copyright 1995 Philip Homburg
 */

#include "inet.h"

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

static int sr_open(devminor_t minor, int access, endpoint_t user_endpt);
static int sr_close(devminor_t minor);
static ssize_t sr_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t sr_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int sr_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id);
static int sr_rwio(sr_req_t *req);
static int sr_restart_read(sr_fd_t *fdp);
static int sr_restart_write(sr_fd_t *fdp);
static int sr_restart_ioctl(sr_fd_t *fdp);
static int sr_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id);
static int sr_select(devminor_t minor, unsigned int ops, endpoint_t endpt);
static sr_fd_t *sr_getchannel(int minor);
static acc_t *sr_get_userdata(int fd, size_t offset, size_t count, int
	for_ioctl);
static int sr_put_userdata(int fd, size_t offset, acc_t *data, int
	for_ioctl);
static void sr_select_res(int fd, unsigned ops);
static int walk_queue(sr_fd_t *sr_fd, mq_t **q_head_ptr, mq_t **q_tail_ptr,
	int type, endpoint_t endpt, cdev_id_t id, int first_flag);
static void sr_event(event_t *evp, ev_arg_t arg);
static int cp_u2b(endpoint_t proc, cp_grant_id_t gid, vir_bytes offset,
	acc_t **var_acc_ptr, int size);
static int cp_b2u(acc_t *acc_ptr, endpoint_t proc, cp_grant_id_t gid,
	vir_bytes offset);

static struct chardriver inet_tab = {
	.cdr_open	= sr_open,
	.cdr_close	= sr_close,
	.cdr_read	= sr_read,
	.cdr_write	= sr_write,
	.cdr_ioctl	= sr_ioctl,
	.cdr_cancel	= sr_cancel,
	.cdr_select	= sr_select
};

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

void sr_rec(message *m, int ipc_status)
{
	chardriver_process(&inet_tab, m, ipc_status);
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

static int sr_open(devminor_t minor, int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
	sr_fd_t *sr_fd;
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
	return CDEV_CLONED | i;
}

static int sr_close(devminor_t minor)
{
	sr_fd_t *sr_fd;

	sr_fd= sr_getchannel(minor);
	assert (sr_fd);

	if (sr_fd->srf_flags & SFF_BUSY)
		ip_panic(("close on busy channel"));

	assert (!(sr_fd->srf_flags & SFF_MINOR));
	(*sr_fd->srf_close)(sr_fd->srf_fd);
	sr_fd->srf_flags= SFF_FREE;

	return OK;
}

static int sr_rwio(sr_req_t *req)
{
	sr_fd_t *sr_fd;
	mq_t *m, **q_head_ptr = NULL, **q_tail_ptr = NULL;
	int ip_flag = 0, susp_flag = 0, first_flag = 0;
	int r = OK;
	ioreq_t request;
	size_t size;

	if (!(m = mq_get()))
		ip_panic(("out of messages"));
	m->mq_req = *req;

	sr_fd= sr_getchannel(m->mq_req.srr_minor);
	assert (sr_fd);

	switch(m->mq_req.srr_type)
	{
	case SRR_READ:
		q_head_ptr= &sr_fd->srf_read_q;
		q_tail_ptr= &sr_fd->srf_read_q_tail;
		ip_flag= SFF_READ_IP;
		susp_flag= SFF_READ_SUSP;
		first_flag= SFF_READ_FIRST;
		break;
	case SRR_WRITE:
		q_head_ptr= &sr_fd->srf_write_q;
		q_tail_ptr= &sr_fd->srf_write_q_tail;
		ip_flag= SFF_WRITE_IP;
		susp_flag= SFF_WRITE_SUSP;
		first_flag= SFF_WRITE_FIRST;
		break;
	case SRR_IOCTL:
		q_head_ptr= &sr_fd->srf_ioctl_q;
		q_tail_ptr= &sr_fd->srf_ioctl_q_tail;
		ip_flag= SFF_IOCTL_IP;
		susp_flag= SFF_IOCTL_SUSP;
		first_flag= SFF_IOCTL_FIRST;
		break;
	default:
		ip_panic(("illegal request type"));
	}

	if (sr_fd->srf_flags & ip_flag)
	{
		assert(sr_fd->srf_flags & susp_flag);
		assert(*q_head_ptr);

		if (m->mq_req.srr_flags & CDEV_NONBLOCK) {
			mq_free(m);
			return EAGAIN;
		}

		(*q_tail_ptr)->mq_next= m;
		*q_tail_ptr= m;
		return EDONTREPLY;
	}
	assert(!*q_head_ptr);

	*q_tail_ptr= *q_head_ptr= m;
	sr_fd->srf_flags |= ip_flag;
	assert(!(sr_fd->srf_flags & first_flag));
	sr_fd->srf_flags |= first_flag;

	switch(m->mq_req.srr_type)
	{
	case SRR_READ:
		r= (*sr_fd->srf_read)(sr_fd->srf_fd, m->mq_req.srr_size);
		break;
	case SRR_WRITE:
		r= (*sr_fd->srf_write)(sr_fd->srf_fd, m->mq_req.srr_size);
		break;
	case SRR_IOCTL:
		request= m->mq_req.srr_req;
		size= _MINIX_IOCTL_SIZE(request);
		if (size>MAX_IOCTL_S)
		{
			DBLOCK(1, printf("replying EINVAL\n"));
			r= sr_put_userdata(sr_fd-sr_fd_table, EINVAL, 
				NULL, 1);
			assert(r == OK);
			assert(sr_fd->srf_flags & first_flag);
			sr_fd->srf_flags &= ~first_flag;
			return EDONTREPLY;
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
		if (m->mq_req.srr_flags & CDEV_NONBLOCK) {
			r= sr_cancel(m->mq_req.srr_minor, m->mq_req.srr_endpt,
				m->mq_req.srr_id);
			assert(r == EDONTREPLY);	/* head of the queue */
		}
	} else
		mq_free(m);
	return EDONTREPLY;		/* request already completed */
}

static ssize_t sr_read(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
	sr_req_t req;

	req.srr_type = SRR_READ;
	req.srr_minor = minor;
	req.srr_endpt = endpt;
	req.srr_grant = grant;
	req.srr_size = size;
	req.srr_flags = flags;
	req.srr_id = id;

	return sr_rwio(&req);
}

static ssize_t sr_write(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
	sr_req_t req;

	req.srr_type = SRR_WRITE;
	req.srr_minor = minor;
	req.srr_endpt = endpt;
	req.srr_grant = grant;
	req.srr_size = size;
	req.srr_flags = flags;
	req.srr_id = id;

	return sr_rwio(&req);
}

static int sr_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t UNUSED(user_endpt),
	cdev_id_t id)
{
	sr_req_t req;

	req.srr_type = SRR_IOCTL;
	req.srr_minor = minor;
	req.srr_req = request;
	req.srr_endpt = endpt;
	req.srr_grant = grant;
	req.srr_flags = flags;
	req.srr_id = id;

	return sr_rwio(&req);
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

	r= (*sr_fd->srf_read)(sr_fd->srf_fd, mp->mq_req.srr_size);

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

	r= (*sr_fd->srf_write)(sr_fd->srf_fd, mp->mq_req.srr_size);

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

	r= (*sr_fd->srf_ioctl)(sr_fd->srf_fd, mp->mq_req.srr_req);

	assert(r == OK || r == SUSPEND || 
		(printf("r= %d\n", r), 0));
	if (r == SUSPEND)
		sr_fd->srf_flags |= SFF_IOCTL_SUSP;
	return r;
}

static int sr_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
	sr_fd_t *sr_fd;
	int result;

	sr_fd= sr_getchannel(minor);
	assert (sr_fd);

	result= walk_queue(sr_fd, &sr_fd->srf_ioctl_q,
		&sr_fd->srf_ioctl_q_tail, SR_CANCEL_IOCTL,
		endpt, id, SFF_IOCTL_FIRST);
	if (result != EAGAIN)
		return (result == OK) ? EDONTREPLY : EINTR;

	result= walk_queue(sr_fd, &sr_fd->srf_read_q, 
		&sr_fd->srf_read_q_tail, SR_CANCEL_READ,
		endpt, id, SFF_READ_FIRST);
	if (result != EAGAIN)
		return (result == OK) ? EDONTREPLY : EINTR;

	result= walk_queue(sr_fd, &sr_fd->srf_write_q, 
		&sr_fd->srf_write_q_tail, SR_CANCEL_WRITE,
		endpt, id, SFF_WRITE_FIRST);
	if (result != EAGAIN)
		return (result == OK) ? EDONTREPLY : EINTR;

	/* We already replied to the request, so don't reply to the CANCEL. */
	return EDONTREPLY;
}

static int sr_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
	sr_fd_t *sr_fd;
	int r, m_ops;
	unsigned int i_ops;

	sr_fd= sr_getchannel(minor);
	assert (sr_fd);

	sr_fd->srf_select_proc= endpt;

	i_ops= 0;
	if (ops & CDEV_OP_RD) i_ops |= SR_SELECT_READ;
	if (ops & CDEV_OP_WR) i_ops |= SR_SELECT_WRITE;
	if (ops & CDEV_OP_ERR) i_ops |= SR_SELECT_EXCEPTION;
	if (!(ops & CDEV_NOTIFY)) i_ops |= SR_SELECT_POLL;

	r= (*sr_fd->srf_select)(sr_fd->srf_fd,  i_ops);
	if (r < 0) {
		m_ops= r;
	} else {
		m_ops= 0;
		if (r & SR_SELECT_READ) m_ops |= CDEV_OP_RD;
		if (r & SR_SELECT_WRITE) m_ops |= CDEV_OP_WR;
		if (r & SR_SELECT_EXCEPTION) m_ops |= CDEV_OP_ERR;
	}

	return m_ops;
}

static int walk_queue(sr_fd, q_head_ptr, q_tail_ptr, type, endpt, id,
	first_flag)
sr_fd_t *sr_fd;
mq_t **q_head_ptr;
mq_t **q_tail_ptr;
int type;
endpoint_t endpt;
cdev_id_t id;
int first_flag;
{
	mq_t *q_ptr_prv, *q_ptr;
	int result;

	for(q_ptr_prv= NULL, q_ptr= *q_head_ptr; q_ptr; 
		q_ptr_prv= q_ptr, q_ptr= q_ptr->mq_next)
	{
		if (q_ptr->mq_req.srr_endpt != endpt)
			continue;
		if (q_ptr->mq_req.srr_id != id)
			continue;
		if (!q_ptr_prv)
		{
			assert(!(sr_fd->srf_flags & first_flag));
			sr_fd->srf_flags |= first_flag;

			/* This will also send a reply. */
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
		chardriver_reply_task(m->mq_req.srr_endpt, m->mq_req.srr_id,
			result);
		if (is_revive)
			mq_free(m);
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

	result= cp_u2b ((*head_ptr)->mq_req.srr_endpt,
		(*head_ptr)->mq_req.srr_grant, offset, &acc, count);

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
		chardriver_reply_task(m->mq_req.srr_endpt, m->mq_req.srr_id,
			result);
		if (is_revive)
			mq_free(m);
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

	return cp_b2u (data, (*head_ptr)->mq_req.srr_endpt,
		(*head_ptr)->mq_req.srr_grant, offset);
}

static void sr_select_res(int fd, unsigned ops)
{
	sr_fd_t *sr_fd;
	unsigned int m_ops;

	sr_fd= &sr_fd_table[fd];

	m_ops= 0;
	if (ops & SR_SELECT_READ) m_ops |= CDEV_OP_RD;
	if (ops & SR_SELECT_WRITE) m_ops |= CDEV_OP_WR;
	if (ops & SR_SELECT_EXCEPTION) m_ops |= CDEV_OP_ERR;

	chardriver_reply_select(sr_fd->srf_select_proc, fd, m_ops);
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
