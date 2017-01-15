
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>
#include <minix/bitmap.h>
#include <minix/rs.h>

#include <sys/mman.h>

#include <errno.h>
#include <string.h>
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
	bitchunk_t call_mask[VM_CALL_MASK_SIZE], *call_mask_p;

	nr = m->VM_RS_NR;

	if ((r = vm_isokendpt(nr, &n)) != OK) {
		printf("do_rs_set_priv: bad endpoint %d\n", nr);
		return EINVAL;
	}

	vmp = &vmproc[n];

	if (m->VM_RS_BUF) {
		r = sys_datacopy(m->m_source, (vir_bytes) m->VM_RS_BUF, SELF,
			(vir_bytes) call_mask, sizeof(call_mask));
		if (r != OK)
			return r;
		call_mask_p = call_mask;
	} else {
		if (m->VM_RS_SYS) {
			printf("VM: do_rs_set_priv: sys procs don't share!\n");
			return EINVAL;
		}
		call_mask_p = NULL;
	}

	acl_set(vmp, call_mask_p, m->VM_RS_SYS);

	return OK;
}

/*===========================================================================*
 *				do_rs_prepare	     			     *
 *===========================================================================*/
int do_rs_prepare(message *m_ptr)
{
	/* Prepare a new instance of a service for an upcoming live-update
	 * switch, based on the old instance of this service.  This call is
	 * used only by RS and only for a multicomponent live update which
	 * includes VM.  In this case, all processes need to be prepared such
	 * that they don't require the new VM instance to perform actions
	 * during live update that cannot be undone in the case of a rollback.
	 */
	endpoint_t src_e, dst_e;
	int src_p, dst_p;
	struct vmproc *src_vmp, *dst_vmp;
	struct vir_region *src_data_vr, *dst_data_vr;
	vir_bytes src_addr, dst_addr;
	int sys_upd_flags;

	src_e = m_ptr->m_lsys_vm_update.src;
	dst_e = m_ptr->m_lsys_vm_update.dst;
        sys_upd_flags = m_ptr->m_lsys_vm_update.flags;

	/* Lookup slots for source and destination process. */
	if(vm_isokendpt(src_e, &src_p) != OK) {
		printf("VM: do_rs_prepare: bad src endpoint %d\n", src_e);
		return EINVAL;
	}
	src_vmp = &vmproc[src_p];
	if(vm_isokendpt(dst_e, &dst_p) != OK) {
		printf("VM: do_rs_prepare: bad dst endpoint %d\n", dst_e);
		return EINVAL;
	}
	dst_vmp = &vmproc[dst_p];

	/* Pin memory for the source process. */
	map_pin_memory(src_vmp);

	/* See if the source process has a larger heap than the destination
	 * process.  If so, extend the heap of the destination process to
	 * match the source's.  While this may end up wasting quite some
	 * memory, it is absolutely essential that the destination process
	 * does not run out of heap memory during the live update window,
	 * and since most processes will be doing an identity transfer, they
	 * are likely to require as much heap as their previous instances.
	 * Better safe than sorry.  TODO: prevent wasting memory somehow;
	 * this seems particularly relevant for RS.
	 */
	src_data_vr = region_search(&src_vmp->vm_regions_avl, VM_MMAPBASE,
	    AVL_LESS);
	assert(src_data_vr);
	dst_data_vr = region_search(&dst_vmp->vm_regions_avl, VM_MMAPBASE,
	    AVL_LESS);
	assert(dst_data_vr);

	src_addr = src_data_vr->vaddr + src_data_vr->length;
	dst_addr = dst_data_vr->vaddr + dst_data_vr->length;
	if (src_addr > dst_addr)
		real_brk(dst_vmp, src_addr);

	/* Now also pin memory for the destination process. */
	map_pin_memory(dst_vmp);

	/* Finally, map the source process's memory-mapped regions into the
	 * destination process.  This needs to happen now, because VM may not
	 * allocate any objects during the live update window, since this
	 * would prevent successful rollback of VM afterwards.  The
	 * destination may not actually touch these regions during the live
	 * update window either, because they are mapped copy-on-write and a
	 * pagefault would also cause object allocation.  Objects are pages,
	 * slab objects, anything in the new VM instance to which changes are
	 * visible in the old VM basically.
	 */
	if (!(sys_upd_flags & SF_VM_NOMMAP))
		map_proc_dyn_data(src_vmp, dst_vmp);

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
	int r, sys_upd_flags;

	src_e = m_ptr->m_lsys_vm_update.src;
	dst_e = m_ptr->m_lsys_vm_update.dst;
        sys_upd_flags = m_ptr->m_lsys_vm_update.flags;
        reply_e = m_ptr->m_source;

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

	/* Check flags. */
	if((sys_upd_flags & (SF_VM_ROLLBACK|SF_VM_NOMMAP)) == 0) {
	        /* Can't preallocate when transfering mmapped regions. */
	        if(map_region_lookup_type(dst_vmp, VR_PREALLOC_MAP)) {
			return ENOSYS;
	        }
	}

	/* Let the kernel do the update first. */
	r = sys_update(src_e, dst_e,
	    sys_upd_flags & SF_VM_ROLLBACK ? SYS_UPD_ROLLBACK : 0);
	if(r != OK) {
		return r;
	}

	/* Do the update in VM now. */
	r = swap_proc_slot(src_vmp, dst_vmp);
	if(r != OK) {
		return r;
	}
	r = swap_proc_dyn_data(src_vmp, dst_vmp, sys_upd_flags);
	if(r != OK) {
		return r;
	}
	pt_bind(&src_vmp->vm_pt, src_vmp);
	pt_bind(&dst_vmp->vm_pt, dst_vmp);

	/* Reply in case of external request, update-aware. */
	if(reply_e != VM_PROC_NR) {
            if(reply_e == src_e) reply_e = dst_e;
            else if(reply_e == dst_e) reply_e = src_e;
            m_ptr->m_type = OK;
            r = ipc_send(reply_e, m_ptr);
            if(r != OK) {
                    panic("ipc_send() error");
            }
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

	pt_assert(&this_vm_vmp->vm_pt);

	/* Check if the operation is allowed. */
	assert(num_vm_instances == 1 || num_vm_instances == 2);
	if(num_vm_instances == 2) {
		printf("VM can currently support no more than 2 VM instances at the time.");
		return EPERM;
	}

	/* Copy settings from current VM. */
	new_vm_vmp->vm_flags |= VMF_VM_INSTANCE;
	num_vm_instances++;

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
	r = pt_ptalloc_in_range(&this_vm_vmp->vm_pt,
		VM_OWN_HEAPBASE, VM_DATATOP, flags, verify);
	if(r != OK) {
		return r;
	}
	r = pt_ptalloc_in_range(&new_vm_vmp->vm_pt,
		VM_OWN_HEAPBASE, VM_DATATOP, flags, verify);
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

	pt_assert(&this_vm_vmp->vm_pt);
	pt_assert(&new_vm_vmp->vm_pt);

	return OK;
}

/*===========================================================================*
 *		           rs_memctl_heap_prealloc			     *
 *===========================================================================*/
static int rs_memctl_heap_prealloc(struct vmproc *vmp,
	vir_bytes *addr, size_t *len)
{
	struct vir_region *data_vr;
	vir_bytes bytes;

	if(*len <= 0) {
		return EINVAL;
	}
	data_vr = region_search(&vmp->vm_regions_avl, VM_MMAPBASE, AVL_LESS);
	*addr = data_vr->vaddr + data_vr->length;
	bytes = *addr + *len;

	return real_brk(vmp, bytes);
}

/*===========================================================================*
 *		           rs_memctl_map_prealloc			     *
 *===========================================================================*/
static int rs_memctl_map_prealloc(struct vmproc *vmp,
	vir_bytes *addr, size_t *len)
{
	struct vir_region *vr;
	vir_bytes base, top;
	int is_vm;

	if(*len <= 0) {
		return EINVAL;
	}
	*len = CLICK_CEIL(*len);

	is_vm = (vmp->vm_endpoint == VM_PROC_NR);
	base = is_vm ? VM_OWN_MMAPBASE : VM_MMAPBASE;
	top = is_vm ? VM_OWN_MMAPTOP : VM_MMAPTOP;

	if (!(vr = map_page_region(vmp, base, top, *len,
	    VR_ANON|VR_WRITABLE|VR_UNINITIALIZED, MF_PREALLOC,
	    &mem_type_anon))) {
		return ENOMEM;
	}
	vr->flags |= VR_PREALLOC_MAP;
	*addr = vr->vaddr;
	return OK;
}

/*===========================================================================*
 *		         rs_memctl_get_prealloc_map			     *
 *===========================================================================*/
static int rs_memctl_get_prealloc_map(struct vmproc *vmp,
	vir_bytes *addr, size_t *len)
{
	struct vir_region *vr;

	vr = map_region_lookup_type(vmp, VR_PREALLOC_MAP);
	if(!vr) {
		*addr = 0;
		*len = 0;
	}
	else {
		*addr = vr->vaddr;
		*len = vr->length;
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
		/* Only actually pin RS memory if VM can recover from crashes (saves memory). */
		if (num_vm_instances <= 1)
			return OK;
		r = map_pin_memory(vmp);
		return r;
	case VM_RS_MEM_MAKE_VM:
		r = rs_memctl_make_vm_instance(vmp);
		return r;
	case VM_RS_MEM_HEAP_PREALLOC:
		r = rs_memctl_heap_prealloc(vmp, (vir_bytes*) &m_ptr->VM_RS_CTL_ADDR, (size_t*) &m_ptr->VM_RS_CTL_LEN);
		return r;
	case VM_RS_MEM_MAP_PREALLOC:
		r = rs_memctl_map_prealloc(vmp, (vir_bytes*) &m_ptr->VM_RS_CTL_ADDR, (size_t*) &m_ptr->VM_RS_CTL_LEN);
		return r;
	case VM_RS_MEM_GET_PREALLOC_MAP:
		r = rs_memctl_get_prealloc_map(vmp, (vir_bytes*) &m_ptr->VM_RS_CTL_ADDR, (size_t*) &m_ptr->VM_RS_CTL_LEN);
		return r;
	default:
		printf("do_rs_memctl: bad request %d\n", req);
		return EINVAL;
	}
}

