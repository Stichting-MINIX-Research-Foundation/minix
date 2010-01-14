
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

#include "glo.h"
#include "proto.h"
#include "util.h"

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

