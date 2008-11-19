
#define _SYSTEM 1

#define VERBOSE 0

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

#include <sys/mman.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "region.h"

/*===========================================================================*
 *				do_mmap			     		     *
 *===========================================================================*/
PUBLIC int do_mmap(message *m)
{
	int r, n;
	struct vmproc *vmp;
	int mfflags = 0;
	struct vir_region *vr = NULL;

	if((r=vm_isokendpt(m->m_source, &n)) != OK) {
		vm_panic("do_mmap: message from strange source", m->m_source);
	}

	vmp = &vmproc[n];

	if(m->VMM_FLAGS & MAP_LOWER16M)
		printf("VM: warning for %d: MAP_LOWER16M not implemented\n",
			m->m_source);

	if(!(vmp->vm_flags & VMF_HASPT))
		return ENXIO;

	if(m->VMM_FD == -1 || (m->VMM_FLAGS & MAP_ANON)) {
		int s;
		vir_bytes v;
		size_t len = (vir_bytes) m->VMM_LEN;

		if(m->VMM_FD != -1) {
			return EINVAL;
		}

		if(m->VMM_FLAGS & MAP_CONTIG) mfflags |= MF_CONTIG;
		if(m->VMM_FLAGS & MAP_PREALLOC) mfflags |= MF_PREALLOC;

		if(len % VM_PAGE_SIZE)
			len += VM_PAGE_SIZE - (len % VM_PAGE_SIZE);

		if(!(vr = map_page_region(vmp, vmp->vm_stacktop,
			VM_DATATOP, len, 0,
			VR_ANON | VR_WRITABLE, mfflags))) {
			return ENOMEM;
		}
	} else {
		return ENOSYS;
	}

	/* Return mapping, as seen from process. */
	vm_assert(vr);
	m->VMM_RETADDR = arch_map2vir(vmp, vr->vaddr);

	return OK;
}

/*===========================================================================*
 *				do_map_phys		     		     *
 *===========================================================================*/
PUBLIC int do_map_phys(message *m)
{
	int r, n;
	struct vmproc *vmp;
	endpoint_t target;
	struct vir_region *vr;

	target = m->VMMP_EP;
	if(target == SELF)
		target = m->m_source;

	if((r=vm_isokendpt(target, &n)) != OK) {
		printf("do_map_phys: bogus target %d\n", target);
		return EINVAL;
	}

	vmp = &vmproc[n];

	if(!(vmp->vm_flags & VMF_HASPT))
		return ENXIO;

	if(!(vr = map_page_region(vmp, vmp->vm_stacktop, VM_DATATOP,
		(vir_bytes) m->VMMP_LEN, (vir_bytes)m->VMMP_PHADDR,
		VR_DIRECT | VR_NOPF | VR_WRITABLE, 0))) {
		printf("VM:do_map_phys: map_page_region failed\n");
		return ENOMEM;
	}

	m->VMMP_VADDR_REPLY = (void *) arch_map2vir(vmp, vr->vaddr);

	return OK;
}

/*===========================================================================*
 *				do_unmap_phys		     		     *
 *===========================================================================*/
PUBLIC int do_unmap_phys(message *m)
{
	int r, n;
	struct vmproc *vmp;
	endpoint_t target;
	struct vir_region *region;

	target = m->VMUP_EP;
	if(target == SELF)
		target = m->m_source;

	if((r=vm_isokendpt(target, &n)) != OK) {
		printf("VM:do_unmap_phys: bogus target %d\n", target);
		return EINVAL;
	}

	vmp = &vmproc[n];

	if(!(region = map_lookup(vmp, (vir_bytes) m->VMUM_ADDR))) {
		printf("VM:do_unmap_phys: map_lookup failed\n");
		return EINVAL;
	}

	if(!(region->flags & VR_DIRECT)) {
		printf("VM:do_unmap_phys: region not a DIRECT mapping\n");
		return EINVAL;
	}

	if(map_unmap_region(vmp, region) != OK) {
		printf("VM:do_unmap_phys: map_unmap_region failed\n");
		return EINVAL;
	}

	return OK;
}
