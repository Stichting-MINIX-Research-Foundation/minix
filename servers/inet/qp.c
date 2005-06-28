/*
inet/qp.c

Query parameters

Created:	June 1995 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#include "inet.h"

#include <sys/svrctl.h>
#ifdef __minix_vmd
#include <minix/queryparam.h>
#else /* Minix 3 */
#include <minix3/queryparam.h>
#endif

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

FORWARD int get_userdata ARGS(( int proc, vir_bytes vaddr, vir_bytes vlen,
	void *buffer ));
FORWARD int put_userdata ARGS(( int proc, vir_bytes vaddr, vir_bytes vlen,
	void *buffer ));
FORWARD int iqp_getc ARGS(( void ));
FORWARD void iqp_putc ARGS(( int c ));

PRIVATE struct export_param_list inet_ex_list[]=
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

PRIVATE struct export_params inet_ex_params= { inet_ex_list, NULL };

PRIVATE struct queryvars {
	int proc;
	struct svrqueryparam qpar;
	char parbuf[256], valbuf[256];
	char *param, *value;
	int r;
} *qvars;

PUBLIC void qp_init()
{
	qp_export(&inet_ex_params);
}

PUBLIC int qp_query(proc, argp)
int proc;
vir_bytes argp;
{
	/* Return values, sizes, or addresses of variables in MM space. */

	struct queryvars qv;
	void *addr;
	size_t n, size;
	int byte;
	int more;
	static char hex[]= "0123456789ABCDEF";

	qv.r= get_userdata(proc, argp, sizeof(qv.qpar), &qv.qpar);

	/* Export these to mq_getc() and mq_putc(). */
	qvars= &qv;
	qv.proc= proc;
	qv.param= qv.parbuf + sizeof(qv.parbuf);
	qv.value= qv.valbuf;

	do {
		more= queryparam(iqp_getc, &addr, &size);
		for (n= 0; n < size; n++) {
			byte= ((u8_t *) addr)[n];
			iqp_putc(hex[byte >> 4]);
			iqp_putc(hex[byte & 0x0F]);
		}
		iqp_putc(more ? ',' : 0);
	} while (more);
	return qv.r;
}


PRIVATE int iqp_getc()
{
	/* Return one character of the names to search for. */
	struct queryvars *qv= qvars;
	size_t n;

	if (qv->r != OK || qv->qpar.psize == 0) return 0;
	if (qv->param == qv->parbuf + sizeof(qv->parbuf)) {
		/* Need to fill the parameter buffer. */
		n= sizeof(qv->parbuf);
		if (qv->qpar.psize < n) n= qv->qpar.psize;
		qv->r= get_userdata(qv->proc, (vir_bytes) qv->qpar.param, n,
								qv->parbuf);
		if (qv->r != OK) return 0;
		qv->qpar.param+= n;
		qv->param= qv->parbuf;
	}
	qv->qpar.psize--;
	return (u8_t) *qv->param++;
}


PRIVATE void iqp_putc(c)
int c;
{
	/* Send one character back to the user. */
	struct queryvars *qv= qvars;
	size_t n;

	if (qv->r != OK || qv->qpar.vsize == 0) return;
	*qv->value++= c;
	qv->qpar.vsize--;
	if (qv->value == qv->valbuf + sizeof(qv->valbuf)
					|| c == 0 || qv->qpar.vsize == 0) {
		/* Copy the value buffer to user space. */
		n= qv->value - qv->valbuf;
		qv->r= put_userdata(qv->proc, (vir_bytes) qv->qpar.value, n,
								qv->valbuf);
		qv->qpar.value+= n;
		qv->value= qv->valbuf;
	}
}

PRIVATE int get_userdata(proc, vaddr, vlen, buffer)
int proc;
vir_bytes vaddr;
vir_bytes vlen;
void *buffer;
{
#ifdef __minix_vmd
	return sys_copy(proc, SEG_D, (phys_bytes)vaddr, this_proc, SEG_D,
		(phys_bytes)buffer, (phys_bytes)vlen);
#else /* Minix 3 */
	return sys_vircopy(proc, D, vaddr, SELF, D, (vir_bytes)buffer, vlen);
#endif
}


PRIVATE int put_userdata(proc, vaddr, vlen, buffer)
int proc;
vir_bytes vaddr;
vir_bytes vlen;
void *buffer;
{
#ifdef __minix_vmd
	return sys_copy(this_proc, SEG_D, (phys_bytes)buffer,
		proc, SEG_D, (phys_bytes)vaddr, (phys_bytes)vlen);
#else /* Minix 3 */
	return sys_vircopy(SELF, D, (vir_bytes)buffer, proc, D, vaddr, vlen);
#endif
}



/*
 * $PchId: qp.c,v 1.7 2005/06/28 14:25:25 philip Exp $
 */
