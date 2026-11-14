/*
 * MXV Phase 2 VM-server side RAM pin protocol.
 *
 * A pin is accepted only for one entire MAP_CONTIG|MAP_PREALLOC allocation.
 * VM first proves the requested region uses the contiguous allocator; the
 * kernel independently verifies the owner's present, writable user PTEs.
 * The region token blocks unmap until the EPT reference has been removed.
 */
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
#include <minix/mxvctl.h>

#include <errno.h>
#include <assert.h>
#include <string.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "region.h"

#if defined(__x86_64__)
#define MXV_PIN_PENDING ((u64_t)~(u64_t)0)

static int
mxv_find_contiguous_region(struct vmproc *vmp, vir_bytes addr,
    vir_bytes len, struct vir_region **regionp)
{
	struct vir_region *vr;
	struct phys_region *pr;
	phys_bytes base = 0;
	vir_bytes offset;
	int first = TRUE;

	if (len == 0 || len > MXV_MAX_RAM_BYTES ||
	    (addr & (VM_PAGE_SIZE - 1)) != 0 ||
	    (len & (VM_PAGE_SIZE - 1)) != 0)
		return EINVAL;
	vr = map_lookup(vmp, addr, NULL);
	if (vr == NULL || vr->vaddr != addr || vr->length != len)
		return EINVAL;
	if (vr->def_memtype != &mem_type_anon_contig ||
	    !(vr->flags & VR_WRITABLE))
		return EACCES;
	if (vr->mxv_pin_token != 0)
		return EBUSY;

	for (offset = 0; offset < len; offset += VM_PAGE_SIZE) {
		pr = physblock_get(vr, offset);
		if (pr == NULL || pr->ph == NULL)
			return EFAULT;
		if (first) {
			base = pr->ph->phys;
			first = FALSE;
		} else if (pr->ph->phys != base + offset) {
			return EINVAL;
		}
	}
	if (regionp != NULL)
		*regionp = vr;
	return OK;
}

int
do_mxv_mem_pin(message *m)
{
	struct vmproc *vmp;
	struct vir_region *vr;
	struct mxvctl_req req;
	endpoint_t owner = m->m_source;
	vir_bytes addr, len;
	int proc_nr, r;

	if ((r = vm_isokendpt(owner, &proc_nr)) != OK)
		return EINVAL;
	if (proc_nr != RS_PROC_NR)
		return EPERM;
	addr = (vir_bytes)m->m9_ull1;
	if (m->m9_l1 <= 0 || (u64_t)m->m9_l1 > MXV_MAX_RAM_BYTES ||
	    (u64_t)addr != m->m9_ull1)
		return EINVAL;
	len = (vir_bytes)m->m9_l1;
	vmp = &vmproc[proc_nr];

	r = mxv_find_contiguous_region(vmp, addr, len, &vr);
	if (r != OK)
		return r;

	/* Reserve region across the kernel call so unmap cannot race it. */
	vr->mxv_pin_token = MXV_PIN_PENDING;
	memset(&req, 0, sizeof(req));
	req.version = MXVCTL_API_VERSION;
	req.operation = MXV_PIN_RANGE;
	req.object_handle = (u64_t)(int64_t)owner;
	req.arg0 = (u64_t)addr;
	req.arg1 = (u64_t)len;
	r = sys_mxvctl_req(&req);
	if (r != OK || req.arg0 == 0 || req.arg0 == MXV_PIN_PENDING) {
		vr->mxv_pin_token = 0;
		return r != OK ? r : EIO;
	}
	vr->mxv_pin_token = req.arg0;
	m->m9_ull2 = req.arg0;
	return OK;
}

int
do_mxv_mem_unpin(message *m)
{
	struct vmproc *vmp;
	struct vir_region *vr;
	struct mxvctl_req req;
	endpoint_t owner = m->m_source;
	vir_bytes addr;
	int proc_nr, r;

	if ((r = vm_isokendpt(owner, &proc_nr)) != OK)
		return EINVAL;
	if (proc_nr != RS_PROC_NR)
		return EPERM;
	addr = (vir_bytes)m->m9_ull1;
	if ((u64_t)addr != m->m9_ull1 || m->m9_ull2 == 0 ||
	    m->m9_ull2 == MXV_PIN_PENDING)
		return EINVAL;
	vmp = &vmproc[proc_nr];
	vr = map_lookup(vmp, addr, NULL);
	if (vr == NULL || vr->vaddr != addr ||
	    vr->mxv_pin_token != m->m9_ull2)
		return ESTALE;

	memset(&req, 0, sizeof(req));
	req.version = MXVCTL_API_VERSION;
	req.operation = MXV_PIN_RELEASE;
	req.object_handle = (u64_t)(int64_t)owner;
	req.arg0 = m->m9_ull2;
	r = sys_mxvctl_req(&req);
	if (r != OK)
		return r;
	vr->mxv_pin_token = 0;
	return OK;
}
#else
int do_mxv_mem_pin(message *m) { (void)m; return ENOSYS; }
int do_mxv_mem_unpin(message *m) { (void)m; return ENOSYS; }
#endif
