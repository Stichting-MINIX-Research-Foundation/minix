
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
#include "region.h"

#define LU_DEBUG 0

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
	struct vmproc *src_vmp, *dst_vmp;
	struct vmproc orig_src_vmproc, orig_dst_vmproc;
	int src_p, dst_p, r;
	struct vir_region *vr;

	src_e = m_ptr->VM_RS_SRC_ENDPT;
	dst_e = m_ptr->VM_RS_DST_ENDPT;

	/* Let the kernel do the update first. */
	r = sys_update(src_e, dst_e);
	if(r != OK) {
		return r;
	}

	/* Lookup slots for source and destination process. */
	if(vm_isokendpt(src_e, &src_p) != OK) {
		printf("do_rs_update: bad src endpoint %d\n", src_e);
		return EINVAL;
	}
	src_vmp = &vmproc[src_p];
	if(vm_isokendpt(dst_e, &dst_p) != OK) {
		printf("do_rs_update: bad dst endpoint %d\n", dst_e);
		return EINVAL;
	}
	dst_vmp = &vmproc[dst_p];

#if LU_DEBUG
	printf("do_rs_update: updating %d (%d, %d) into %d (%d, %d)\n",
	    src_vmp->vm_endpoint, src_p, src_vmp->vm_slot,
	    dst_vmp->vm_endpoint, dst_p, dst_vmp->vm_slot);

	printf("do_rs_update: map_printmap for source before updating:\n");
	map_printmap(src_vmp);
	printf("do_rs_update: map_printmap for destination before updating:\n");
	map_printmap(dst_vmp);
#endif

	/* Save existing data. */
	orig_src_vmproc = *src_vmp;
	orig_dst_vmproc = *dst_vmp;

	/* Swap slots. */
	*src_vmp = orig_dst_vmproc;
	*dst_vmp = orig_src_vmproc;

	/* Preserve endpoints and slot numbers. */
	src_vmp->vm_endpoint = orig_src_vmproc.vm_endpoint;
	src_vmp->vm_slot = orig_src_vmproc.vm_slot;
	dst_vmp->vm_endpoint = orig_dst_vmproc.vm_endpoint;
	dst_vmp->vm_slot = orig_dst_vmproc.vm_slot;

	/* Preserve vir_region's parents. */
	for(vr = src_vmp->vm_regions; vr; vr = vr->next) {
		vr->parent = src_vmp;
	}
	for(vr = dst_vmp->vm_regions; vr; vr = vr->next) {
		vr->parent = dst_vmp;
	}

	/* Adjust page tables. */
	vm_assert(src_vmp->vm_flags & VMF_HASPT);
	vm_assert(dst_vmp->vm_flags & VMF_HASPT);
	pt_bind(&src_vmp->vm_pt, src_vmp);
	pt_bind(&dst_vmp->vm_pt, dst_vmp);
	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("do_rs_update: VMCTL_FLUSHTLB failed: %d", r);
	}

#if LU_DEBUG
	printf("do_rs_update: updated %d (%d, %d) into %d (%d, %d)\n",
	    src_vmp->vm_endpoint, src_p, src_vmp->vm_slot,
	    dst_vmp->vm_endpoint, dst_p, dst_vmp->vm_slot);

	printf("do_rs_update: map_printmap for source after updating:\n");
	map_printmap(src_vmp);
	printf("do_rs_update: map_printmap for destination after updating:\n");
	map_printmap(dst_vmp);
#endif

	return OK;
}

