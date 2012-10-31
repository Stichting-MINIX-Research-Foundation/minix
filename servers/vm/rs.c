
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
int do_rs_set_priv(message *m)
{
	int r, n, nr;
	struct vmproc *vmp;

	nr = m->VM_RS_NR;

	if ((r = vm_isokendpt(nr, &n)) != OK) {
		printf("do_rs_set_priv: bad endpoint %d\n", nr);
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
int do_rs_update(message *m_ptr)
{
	endpoint_t src_e, dst_e, reply_e;
	int src_p, dst_p;
	struct vmproc *src_vmp, *dst_vmp;
	int r;

	src_e = m_ptr->VM_RS_SRC_ENDPT;
	dst_e = m_ptr->VM_RS_DST_ENDPT;

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

	/* Let the kernel do the update first. */
	r = sys_update(src_e, dst_e);
	if(r != OK) {
		return r;
	}

	/* Do the update in VM now. */
	r = swap_proc_slot(src_vmp, dst_vmp);
	if(r != OK) {
		return r;
	}
	r = swap_proc_dyn_data(src_vmp, dst_vmp);
	if(r != OK) {
		return r;
	}
	pt_bind(&src_vmp->vm_pt, src_vmp);
	pt_bind(&dst_vmp->vm_pt, dst_vmp);

	/* Reply, update-aware. */
	reply_e = m_ptr->m_source;
	if(reply_e == src_e) reply_e = dst_e;
	else if(reply_e == dst_e) reply_e = src_e;
	m_ptr->m_type = OK;
	r = send(reply_e, m_ptr);
	if(r != OK) {
		panic("send() error");
	}

	return SUSPEND;
}

/*===========================================================================*
 *		           rs_memctl_make_vm_instance			     *
 *===========================================================================*/
static int rs_memctl_make_vm_instance(struct vmproc *new_vm_vmp)
{
	int r;
	u32_t flags;
	int verify;
	struct vmproc *this_vm_vmp;

	this_vm_vmp = &vmproc[VM_PROC_NR];

	/* Pin memory for the new VM instance. */
	r = map_pin_memory(new_vm_vmp);
	if(r != OK) {
		return r;
	}

	/* Preallocate page tables for the entire address space for both
	 * VM and the new VM instance.
	 */
	flags = 0;
	verify = FALSE;
	r = pt_ptalloc_in_range(&this_vm_vmp->vm_pt, 0, 0, flags, verify);
	if(r != OK) {
		return r;
	}
	r = pt_ptalloc_in_range(&new_vm_vmp->vm_pt, 0, 0, flags, verify);
	if(r != OK) {
		return r;
	}

	/* Let the new VM instance map VM's page tables and its own. */
	r = pt_ptmap(this_vm_vmp, new_vm_vmp);
	if(r != OK) {
		return r;
	}
	r = pt_ptmap(new_vm_vmp, new_vm_vmp);
	if(r != OK) {
		return r;
	}

	return OK;
}

/*===========================================================================*
 *				do_rs_memctl	     			     *
 *===========================================================================*/
int do_rs_memctl(message *m_ptr)
{
	endpoint_t ep;
	int req, r, proc_nr;
	struct vmproc *vmp;

	ep = m_ptr->VM_RS_CTL_ENDPT;
	req = m_ptr->VM_RS_CTL_REQ;

	/* Lookup endpoint. */
	if ((r = vm_isokendpt(ep, &proc_nr)) != OK) {
		printf("do_rs_memctl: bad endpoint %d\n", ep);
		return EINVAL;
	}
	vmp = &vmproc[proc_nr];

	/* Process request. */
	switch(req)
	{
	case VM_RS_MEM_PIN:

		/* Do not perform VM_RS_MEM_PIN yet - it costs the full
		 * size of the RS stack (64MB by default) in memory,
		 * and it's needed for functionality that isn't complete /
		 * merged in current Minix (surviving VM crashes).
		 */

#if 0
		r = map_pin_memory(vmp);
		return r;
#else
		return OK;
#endif

	case VM_RS_MEM_MAKE_VM:
		r = rs_memctl_make_vm_instance(vmp);
		return r;
	default:
		printf("do_rs_memctl: bad request %d\n", req);
		return EINVAL;
	}
}

