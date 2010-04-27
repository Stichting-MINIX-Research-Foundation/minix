
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

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <assert.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "region.h"

/*===========================================================================*
 *				do_rs_set_priv				     *
 *===========================================================================*/
PUBLIC int do_rs_set_priv(message *m)
{
	int r, n, nr;
	struct vmproc *vmp;

	nr = m->VM_RS_NR;

	if ((r = vm_isokendpt(nr, &n)) != OK) {
		printf("do_rs_set_priv: message from strange source %d\n", nr);
		return EINVAL;
	}

	vmp = &vmproc[n];

	if (m->VM_RS_BUF) {
		r = sys_datacopy(m->m_source, (vir_bytes) m->VM_RS_BUF,
				 SELF, (vir_bytes) vmp->vm_call_mask,
				 sizeof(vmp->vm_call_mask));
		if (r != OK)
			return r;
	}

	return OK;
}

/*===========================================================================*
 *				do_rs_update	     			     *
 *===========================================================================*/
PUBLIC int do_rs_update(message *m_ptr)
{
	endpoint_t src_e, dst_e;
	int r;

	src_e = m_ptr->VM_RS_SRC_ENDPT;
	dst_e = m_ptr->VM_RS_DST_ENDPT;

	/* Let the kernel do the update first. */
	r = sys_update(src_e, dst_e);
	if(r != OK) {
		return r;
	}

	/* Do the update in VM now. */
	r = swap_proc(src_e, dst_e);

	return r;
}

