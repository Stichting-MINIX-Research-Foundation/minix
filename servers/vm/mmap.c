
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

	if(!(vmp->vm_flags & VMF_HASPT))
		return ENXIO;

	if(m->VMM_FD == -1 || (m->VMM_FLAGS & MAP_ANON)) {
		int s;
		vir_bytes v;
		u32_t vrflags = VR_ANON | VR_WRITABLE;
		size_t len = (vir_bytes) m->VMM_LEN;

		if(m->VMM_FD != -1) {
			return EINVAL;
		}

		if(m->VMM_FLAGS & MAP_CONTIG) mfflags |= MF_CONTIG;
		if(m->VMM_FLAGS & MAP_PREALLOC) mfflags |= MF_PREALLOC;
		if(m->VMM_FLAGS & MAP_LOWER16M) vrflags |= VR_LOWER16MB;
		if(m->VMM_FLAGS & MAP_LOWER1M)  vrflags |= VR_LOWER1MB;
		if(m->VMM_FLAGS & MAP_ALIGN64K) vrflags |= VR_PHYS64K;
		if(m->VMM_FLAGS & MAP_SHARED) vrflags |= VR_SHARED;

		if(len % VM_PAGE_SIZE)
			len += VM_PAGE_SIZE - (len % VM_PAGE_SIZE);

		if(!(vr = map_page_region(vmp,
			arch_vir2map(vmp,
				m->VMM_ADDR ? m->VMM_ADDR : vmp->vm_stacktop),
			VM_DATATOP, len, MAP_NONE, vrflags, mfflags))) {
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
 *				map_perm_check		     		     *
 *===========================================================================*/
PUBLIC int map_perm_check(endpoint_t caller, endpoint_t target,
	phys_bytes physaddr, phys_bytes len)
{
	int r;

	/* TTY and memory are allowed to do anything.
	 * They have to be special cases as they have to be able to do
	 * anything; TTY even on behalf of anyone for the TIOCMAPMEM
	 * ioctl. MEM just for itself.
	 */
	if(caller == TTY_PROC_NR)
		return OK;
	if(caller != target)
		return EPERM;
	if(caller == MEM_PROC_NR)
		return OK;

	/* Anyone else needs explicit permission from the kernel (ultimately
	 * set by PCI).
	 */
	r = sys_privquery_mem(caller, physaddr, len);

	return r;
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
	vir_bytes len;
	phys_bytes startaddr;

	target = m->VMMP_EP;
	len = m->VMMP_LEN;

	if(target == SELF)
		target = m->m_source;

	if((r=vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	startaddr = (vir_bytes)m->VMMP_PHADDR;

	/* First check permission, then round range down/up. Caller can't
	 * help it if we can't map in lower than page granularity.
	 */
	if(map_perm_check(m->m_source, target, startaddr, len) != OK) {
		printf("VM: unauthorized mapping of 0x%lx by %d\n",
			startaddr, m->m_source);
		return EPERM;
	}

	vmp = &vmproc[n];

	if(!(vmp->vm_flags & VMF_HASPT))
		return ENXIO;

	if(len % VM_PAGE_SIZE)
		len += VM_PAGE_SIZE - (len % VM_PAGE_SIZE);

	if(!(vr = map_page_region(vmp, arch_vir2map(vmp, vmp->vm_stacktop),
		VM_DATATOP, len, startaddr,
		VR_DIRECT | VR_NOPF | VR_WRITABLE, 0))) {
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

	if((r=vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	vmp = &vmproc[n];

	if(!(region = map_lookup(vmp,
	  arch_vir2map(vmp, (vir_bytes) m->VMUM_ADDR)))) {
		return EINVAL;
	}

	if(!(region->flags & VR_DIRECT)) {
		return EINVAL;
	}

	if(map_unmap_region(vmp, region, region->length) != OK) {
		return EINVAL;
	}

	return OK;
}

/*===========================================================================*
 *				do_remap		     		     *
 *===========================================================================*/
PUBLIC int do_remap(message *m)
{
	int d, dn, s, sn;
	vir_bytes da, sa, startv;
	size_t size;
	struct vir_region *vr, *region;
	struct vmproc *dvmp, *svmp;
	int r;

	d = m->VMRE_D;
	s = m->VMRE_S;
	da = (vir_bytes) m->VMRE_DA;
	sa = (vir_bytes) m->VMRE_SA;
	size = m->VMRE_SIZE;

	if ((r = vm_isokendpt(d, &dn)) != OK)
		return EINVAL;
	if ((r = vm_isokendpt(s, &sn)) != OK)
		return EINVAL;

	dvmp = &vmproc[dn];
	svmp = &vmproc[sn];

	/* da is not translated by arch_vir2map(),
	 * it's handled a little differently,
	 * since in map_remap(), we have to know
	 * about whether the user needs to bind to
	 * THAT address or be chosen by the system.
	 */
	sa = arch_vir2map(svmp, sa);

	if (!(region = map_lookup(svmp, sa)))
		return EINVAL;

	if ((r = map_remap(dvmp, da, size, region, &startv)) != OK)
		return r;

	m->VMRE_RETA = (char *) arch_map2vir(dvmp, startv);
	return OK;
}

/*===========================================================================*
 *				do_shared_unmap		     		     *
 *===========================================================================*/
PUBLIC int do_shared_unmap(message *m)
{
	int r, n;
	struct vmproc *vmp;
	endpoint_t target;
	struct vir_region *vr;
	vir_bytes addr;

	target = m->VMUN_ENDPT;

	if ((r = vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	vmp = &vmproc[n];

	addr = arch_vir2map(vmp, m->VMUN_ADDR);

	if(!(vr = map_lookup(vmp, addr))) {
		printf("VM: addr 0x%lx not found.\n", m->VMUN_ADDR);
		return EFAULT;
	}

	if(vr->vaddr != addr) {
		printf("VM: wrong address for shared_unmap.\n");
		return EFAULT;
	}

	if(!(vr->flags & VR_SHARED)) {
		printf("VM: address does not point to shared region.\n");
		return EFAULT;
	}

	if(map_unmap_region(vmp, vr, vr->length) != OK)
		vm_panic("do_shared_unmap: map_unmap_region failed", NO_NUM);

	return OK;
}

/*===========================================================================*
 *				do_get_phys		     		     *
 *===========================================================================*/
PUBLIC int do_get_phys(message *m)
{
	int r, n;
	struct vmproc *vmp;
	endpoint_t target;
	phys_bytes ret;
	vir_bytes addr;

	target = m->VMPHYS_ENDPT;
	addr = m->VMPHYS_ADDR;

	if ((r = vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	vmp = &vmproc[n];
	addr = arch_vir2map(vmp, addr);

	r = map_get_phys(vmp, addr, &ret);

	m->VMPHYS_RETA = ret;
	return r;
}

/*===========================================================================*
 *				do_get_refcount		     		     *
 *===========================================================================*/
PUBLIC int do_get_refcount(message *m)
{
	int r, n;
	struct vmproc *vmp;
	endpoint_t target;
	u8_t cnt;
	vir_bytes addr;

	target = m->VMREFCNT_ENDPT;
	addr = m->VMREFCNT_ADDR;

	if ((r = vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	vmp = &vmproc[n];
	addr = arch_vir2map(vmp, addr);

	r = map_get_ref(vmp, addr, &cnt);

	m->VMREFCNT_RETC = cnt;
	return r;
}

/*===========================================================================*
 *                              do_munmap                                    *
 *===========================================================================*/
PUBLIC int do_munmap(message *m)
{
        int r, n;
        struct vmproc *vmp;
        vir_bytes addr, len;
	struct vir_region *vr;
        
        if((r=vm_isokendpt(m->m_source, &n)) != OK) {
                vm_panic("do_mmap: message from strange source", m->m_source);
        }
 
        vmp = &vmproc[n];

	if(!(vmp->vm_flags & VMF_HASPT))
		return ENXIO;

	if(m->m_type == VM_MUNMAP) {
	        addr = (vir_bytes) arch_vir2map(vmp, (vir_bytes) m->VMUM_ADDR);
	} else if(m->m_type == VM_MUNMAP_TEXT) {
	        addr = (vir_bytes) arch_vir2map_text(vmp, (vir_bytes) m->VMUM_ADDR);
	} else {
		vm_panic("do_munmap: strange type", NO_NUM);
	}

        if(!(vr = map_lookup(vmp, addr))) {
                printf("VM: unmap: virtual address 0x%lx not found in %d\n",
                        m->VMUM_ADDR, vmp->vm_endpoint);
                return EFAULT;
        }
 
	len = m->VMUM_LEN;
	if (len % VM_PAGE_SIZE)
		len += VM_PAGE_SIZE - (len % VM_PAGE_SIZE);

        if(addr != vr->vaddr || len > vr->length || len < VM_PAGE_SIZE) {
                return EFAULT;
        }       

	if(map_unmap_region(vmp, vr, len) != OK)
		vm_panic("do_munmap: map_unmap_region failed", NO_NUM);

	return OK;
}

int unmap_ok = 0;

/*===========================================================================*
 *                     munmap_lin (used for overrides for VM)                *
 *===========================================================================*/
PRIVATE int munmap_lin(vir_bytes addr, size_t len)
{
	if(addr % VM_PAGE_SIZE) {
		printf("munmap_lin: offset not page aligned\n");
		return EFAULT;
	}

	if(len % VM_PAGE_SIZE) {
		printf("munmap_lin: len not page aligned\n");
		return EFAULT;
	}

	if(pt_writemap(&vmproc[VM_PROC_NR].vm_pt, addr, MAP_NONE, len, 0,
		WMF_OVERWRITE | WMF_FREE) != OK) {
		printf("munmap_lin: pt_writemap failed\n");
		return EFAULT;
	}

	return OK;
}

/*===========================================================================*
 *                              munmap (override for VM)                    *
 *===========================================================================*/
PUBLIC int munmap(void *addr, size_t len)
{
	vir_bytes laddr;
	if(!unmap_ok)
		return ENOSYS;
	laddr = (vir_bytes) arch_vir2map(&vmproc[VM_PROC_NR], (vir_bytes) addr);
	return munmap_lin(laddr, len);
}

/*===========================================================================*
 *                              munmap_text (override for VM)                *
 *===========================================================================*/
PUBLIC int munmap_text(void *addr, size_t len)
{
	vir_bytes laddr;
	if(!unmap_ok)
		return ENOSYS;
	laddr = (vir_bytes) arch_vir2map_text(&vmproc[VM_PROC_NR],
		(vir_bytes) addr);
	return munmap_lin(laddr, len);
}

