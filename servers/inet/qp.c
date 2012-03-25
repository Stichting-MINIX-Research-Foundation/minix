/*
inet/qp.c

Query parameters

Created:	June 1995 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#include "inet.h"
#include "generic/assert.h"

#include <sys/svrctl.h>
#include "queryparam.h"

#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/event.h"
#include "generic/type.h"
#include "generic/sr.h"

#include "generic/tcp_int.h"
#include "generic/udp_int.h"
#include "mq.h"
#include "qp.h"
#include "sr_int.h"

THIS_FILE

#define MAX_REQ	1024	/* Maximum size of a request */

#define QP_FD_NR	4

typedef struct qp_fd
{
	int qf_flags;
	int qf_srfd;
	get_userdata_t qf_get_userdata;
	put_userdata_t qf_put_userdata;
	acc_t *qf_req_pkt;
} qp_fd_t;

#define QFF_EMPTY	0
#define QFF_INUSE	1

static qp_fd_t qp_fd_table[QP_FD_NR];

static struct export_param_list inet_ex_list[]=
{
	QP_VARIABLE(sr_fd_table),
	QP_VARIABLE(ip_dev),
	QP_VARIABLE(tcp_fd_table),
	QP_VARIABLE(tcp_conn_table),
	QP_VARIABLE(tcp_cancel_f),
	QP_VECTOR(udp_port_table, udp_port_table, ip_conf_nr),
	QP_VARIABLE(udp_fd_table),
	QP_END()
};

static struct export_params inet_ex_params= { inet_ex_list, NULL };

static struct queryvars {
	/* Input */
	acc_t *param;

	/* Output */
	qp_fd_t *qp_fd;
	off_t fd_offset;
	size_t rd_bytes_left;
	off_t outbuf_off;
	char outbuf[256];

	int r;	/* result */
} *qvars;


static int  qp_open ARGS(( int port, int srfd,
	get_userdata_t get_userdata, put_userdata_t put_userdata,
	put_pkt_t put_pkt, select_res_t select_res ));
static void qp_close ARGS(( int fd ));
static int qp_read ARGS(( int fd, size_t count ));
static int qp_write ARGS(( int fd, size_t count ));
static int qp_ioctl ARGS(( int fd, ioreq_t req ));
static int qp_cancel ARGS(( int fd, int which_operation ));
static int qp_select ARGS(( int fd, unsigned operations ));
static qp_fd_t *get_qp_fd ARGS(( int fd ));
static int do_query ARGS(( qp_fd_t *qp_fd, acc_t *pkt, int count ));
static int qp_getc ARGS(( void ));
static void qp_putc ARGS(( struct queryvars *qv, int c ));
static void qp_buffree ARGS(( int priority ));
#ifdef BUF_CONSISTENCY_CHECK
static void qp_bufcheck ARGS(( void ));
#endif

void qp_init()
{
	int i;

	qp_export(&inet_ex_params);

	for (i= 0; i<QP_FD_NR; i++)
		qp_fd_table[i].qf_flags= QFF_EMPTY;

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(qp_buffree);
#else
	bf_logon(qp_buffree, qp_bufcheck);
#endif

	sr_add_minor(IPSTAT_MINOR, 0, qp_open, qp_close, qp_read, qp_write,
		qp_ioctl, qp_cancel, qp_select);
}

static int qp_open(port, srfd, get_userdata, put_userdata, put_pkt,
	select_res)
int port;
int srfd;
get_userdata_t get_userdata;
put_userdata_t put_userdata;
put_pkt_t put_pkt;
select_res_t select_res;
{
	int i;
	qp_fd_t *qp_fd;

	for (i= 0; i< QP_FD_NR; i++)
	{
		if (!(qp_fd_table[i].qf_flags & QFF_INUSE))
			break;
	}
	if (i >= QP_FD_NR)
		return EAGAIN;
	qp_fd= &qp_fd_table[i];
	qp_fd->qf_flags= QFF_INUSE;
	qp_fd->qf_srfd= srfd;
	qp_fd->qf_get_userdata= get_userdata;
	qp_fd->qf_put_userdata= put_userdata;
	qp_fd->qf_req_pkt= NULL;

	return i;
}

static void qp_close(fd)
int fd;
{
	qp_fd_t *qp_fd;

	qp_fd= get_qp_fd(fd);
	qp_fd->qf_flags= QFF_EMPTY;
	if (qp_fd->qf_req_pkt)
	{
		bf_afree(qp_fd->qf_req_pkt);
		qp_fd->qf_req_pkt= NULL;
	}
}

static int qp_read(fd, count)
int fd;
size_t count;
{
	int r;
	acc_t *pkt;
	qp_fd_t *qp_fd;

	qp_fd= get_qp_fd(fd);
	pkt= qp_fd->qf_req_pkt;
	qp_fd->qf_req_pkt= NULL;
	if (!pkt)
	{
		/* Nothing to do */
		qp_fd->qf_put_userdata(qp_fd->qf_srfd, EIO, 0,
			FALSE /* !for_ioctl*/);
		return OK;
	}
	r= do_query(qp_fd, pkt, count);
	qp_fd->qf_put_userdata(qp_fd->qf_srfd, r, 0,
			FALSE /* !for_ioctl*/);
	return OK;
}

static int qp_write(fd, count)
int fd;
size_t count;
{
	acc_t *pkt;
	qp_fd_t *qp_fd;

	qp_fd= get_qp_fd(fd);
	if (count > MAX_REQ)
	{
		qp_fd->qf_get_userdata(qp_fd->qf_srfd, ENOMEM, 0,
			FALSE /* !for_ioctl*/);
		return OK;
	}
	pkt= qp_fd->qf_get_userdata(qp_fd->qf_srfd, 0, count,
		FALSE /* !for_ioctl*/);
	if (!pkt)
	{
		qp_fd->qf_get_userdata(qp_fd->qf_srfd, EFAULT, 0,
			FALSE /* !for_ioctl*/);
		return OK;
	}
	if (qp_fd->qf_req_pkt)
	{
		bf_afree(qp_fd->qf_req_pkt);
		qp_fd->qf_req_pkt= NULL;
	}
	qp_fd->qf_req_pkt= pkt;
	qp_fd->qf_get_userdata(qp_fd->qf_srfd, count, 0,
		FALSE /* !for_ioctl*/);
	return OK;
}

static int qp_ioctl(fd, req)
int fd;
ioreq_t req;
{
	qp_fd_t *qp_fd;

	qp_fd= get_qp_fd(fd);
	qp_fd->qf_get_userdata(qp_fd->qf_srfd, ENOTTY, 0,
			TRUE /* for_ioctl*/);
	return OK;
}

static int qp_cancel(fd, which_operation)
int fd;
int which_operation;
{
	ip_panic(( "qp_cancel: should not be here, no blocking calls" ));
	return OK;
}

static int qp_select(fd, operations)
int fd;
unsigned operations;
{
	unsigned resops;

	resops= 0;
	if (operations & SR_SELECT_READ)
		resops |= SR_SELECT_READ;
	if (operations & SR_SELECT_WRITE)
		resops |= SR_SELECT_WRITE;
	return resops;
}

static qp_fd_t *get_qp_fd(fd)
int fd;
{
	qp_fd_t *qp_fd;

	assert(fd >= 0 && fd < QP_FD_NR);
	qp_fd= &qp_fd_table[fd];
	assert(qp_fd->qf_flags & QFF_INUSE);
	return qp_fd;
}

static int do_query(qp_fd, pkt, count)
qp_fd_t *qp_fd;
acc_t *pkt;
int count;
{
	struct queryvars qv;
	void *addr;
	size_t n, size;
	int byte;
	int more;
	static char hex[]= "0123456789ABCDEF";

	qvars= &qv;
	qv.param= pkt; pkt= NULL;
	qv.qp_fd= qp_fd;
	qv.fd_offset= 0;
	qv.outbuf_off= 0;
	qv.rd_bytes_left= count;
	qv.r= 0;

	do {
		more= queryparam(qp_getc, &addr, &size);
		for (n= 0; n < size; n++) {
			byte= ((u8_t *) addr)[n];
			qp_putc(&qv, hex[byte >> 4]);
			qp_putc(&qv, hex[byte & 0x0F]);
		}
		qp_putc(&qv, more ? ',' : 0);
		if (qv.r)
			break;
	} while (more);
	if (qv.param)
	{
		assert(0);
		bf_afree(qv.param);
		qv.param= NULL;
	}
	if (qv.r)
		return qv.r;
	return qv.fd_offset;
}

static int qp_getc()
{
	/* Return one character of the names to search for. */
	acc_t *pkt;
	struct queryvars *qv= qvars;
	u8_t c;

	pkt= qv->param;
	qv->param= NULL;
	if (pkt == NULL)
		return 0;

	assert(bf_bufsize(pkt) > 0);
	c= ptr2acc_data(pkt)[0];
	if (bf_bufsize(pkt) > 1)
		qv->param= bf_delhead(pkt, 1);
	else
	{
		bf_afree(pkt);
		qv->param= NULL;
	}

	return c;
}

static void qp_putc(qv, c)
struct queryvars *qv;
int c;
{
	/* Send one character back to the user. */
	acc_t *pkt;
	qp_fd_t *qp_fd;
	size_t bytes_left;
	off_t off;

	bytes_left= qv->rd_bytes_left;
	if (qv->r || bytes_left == 0)
		return;

	off= qv->outbuf_off;
	assert(off < sizeof(qv->outbuf));
	qv->outbuf[off]= c;
	off++;
	bytes_left--;
	qv->rd_bytes_left= bytes_left;
	if (c != '\0' && off < sizeof(qv->outbuf) && bytes_left != 0)
	{
		qv->outbuf_off= off;
		return;
	}

	pkt= bf_memreq(off);
	assert(pkt->acc_next == NULL);
	memcpy(ptr2acc_data(pkt), qv->outbuf, off);
	qp_fd= qv->qp_fd;
	qv->r= qp_fd->qf_put_userdata(qp_fd->qf_srfd, qv->fd_offset,
		pkt, FALSE /* !for_ioctl*/ );
	qv->fd_offset += off;
	qv->outbuf_off= 0;
}

static void qp_buffree (priority)
int priority;
{
	/* For the moment, we are not going to free anything */
}

#ifdef BUF_CONSISTENCY_CHECK
static void qp_bufcheck()
{
	int i;
	qp_fd_t *qp_fd;

	for (i= 0, qp_fd= qp_fd_table; i<QP_FD_NR; i++, qp_fd++)
	{
		if (!(qp_fd->qf_flags & QFF_INUSE))
			continue;
		if (qp_fd->qf_req_pkt)
			bf_check_acc(qp_fd->qf_req_pkt);
	}
}
#endif

/*
 * $PchId: qp.c,v 1.7 2005/06/28 14:25:25 philip Exp $
 */
