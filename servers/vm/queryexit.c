
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>
#include <minix/bitmap.h>
#include <minix/vm.h>
#include <minix/ds.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>

#include "glo.h"
#include "proto.h"
#include "util.h"

struct query_exit_struct {
	int avail;
	endpoint_t ep;
};
static struct query_exit_struct array[NR_PROCS];

/*===========================================================================*
 *				do_query_exit				     *
 *===========================================================================*/
PUBLIC int do_query_exit(message *m)
{
	int i, nr;
	endpoint_t ep;

	for (i = 0; i < NR_PROCS; i++) {
		if (!array[i].avail) {
			array[i].avail = 1;
			ep = array[i].ep;
			array[i].ep = 0;

			break;
		}
	}

	nr = 0;
	for (i = 0; i < NR_PROCS; i++) {
		if (!array[i].avail)
			nr++;
	}
	m->VM_QUERY_RET_PT = ep;
	m->VM_QUERY_IS_MORE = (nr > 0);

	return OK;
}

/*===========================================================================*
 *				do_notify_sig				     *
 *===========================================================================*/
PUBLIC int do_notify_sig(message *m)
{
	int i, avails = 0;
	endpoint_t ep = m->VM_NOTIFY_SIG_ENDPOINT;
	endpoint_t ipc_ep = m->VM_NOTIFY_SIG_IPC;
	int r;

	for (i = 0; i < NR_PROCS; i++) {
		/* its signal is already here */
		if (!array[i].avail && array[i].ep == ep)
			goto out;
		if (array[i].avail)
			avails++;
	}
	if (!avails) {
		/* no slot for signals, impossible */
		printf("VM: no slot for signals!\n");
		return ENOMEM;
	}

	for (i = 0; i < NR_PROCS; i++) {
		if (array[i].avail) {
			array[i].avail = 0;
			array[i].ep = ep;

			break;
		}
	}

out:
	/* only care when IPC server starts up,
	 * and bypass the process to be signal is IPC itself.
	 */
	if (ipc_ep != 0 && ep != ipc_ep) {
		r = notify(ipc_ep);
		if (r != OK)
			printf("VM: notify IPC error!\n");
	}
	return OK;
}

/*===========================================================================*
 *				init_query_exit				     *
 *===========================================================================*/
PUBLIC void init_query_exit(void)
{
	int i;

	for (i = 0; i < NR_PROCS; i++) {
		array[i].avail = 1;
		array[i].ep = 0;
	}
}

