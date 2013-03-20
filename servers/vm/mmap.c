
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
#include <minix/debug.h>

#include <sys/mman.h>
#include <sys/param.h>

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <fcntl.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "region.h"


static struct vir_region *mmap_region(struct vmproc *vmp, vir_bytes addr,
	u32_t vmm_flags, size_t len, u32_t vrflags,
	mem_type_t *mt, int execpriv)
{
	u32_t mfflags = 0;
	struct vir_region *vr = NULL;

	if(vmm_flags & MAP_LOWER16M) vrflags |= VR_LOWER16MB;
	if(vmm_flags & MAP_LOWER1M)  vrflags |= VR_LOWER1MB;
	if(vmm_flags & MAP_ALIGN64K) vrflags |= VR_PHYS64K;
	if(vmm_flags & MAP_PREALLOC) mfflags |= MF_PREALLOC;
	if(vmm_flags & MAP_UNINITIALIZED) {
		if(!execpriv) return NULL;
		vrflags |= VR_UNINITIALIZED;
	}

	if(len <= 0) {
		return NULL;
	}

	if(len % VM_PAGE_SIZE)
		len += VM_PAGE_SIZE - (len % VM_PAGE_SIZE);

	if (addr && (vmm_flags & MAP_FIXED)) {
		int r = map_unmap_range(vmp, addr, len);
		if(r != OK) {
			printf("mmap_region: map_unmap_range failed (%d)\n", r);
			return NULL;
		}
	}

	if (addr || (vmm_flags & MAP_FIXED)) {
		/* An address is given, first try at that address. */
		vr = map_page_region(vmp, addr, 0, len,
			vrflags, mfflags, mt);
		if(!vr && (vmm_flags & MAP_FIXED))
			return NULL;
	}

	if (!vr) {
		/* No address given or address already in use. */
		vr = map_page_region(vmp, VM_PAGE_SIZE, VM_DATATOP, len,
			vrflags, mfflags, mt);
	}

	return vr;
}

/*===========================================================================*
 *				do_mmap			     		     *
 *===========================================================================*/
int do_mmap(message *m)
{
	int r, n;
	struct vmproc *vmp;
	vir_bytes addr = m->VMM_ADDR;
	struct vir_region *vr = NULL;
	int execpriv = 0;
	size_t len = (vir_bytes) m->VMM_LEN;

	/* RS and VFS can do slightly more special mmap() things */
	if(m->m_source == VFS_PROC_NR || m->m_source == RS_PROC_NR)
		execpriv = 1;

	if(m->VMM_FLAGS & MAP_THIRDPARTY) {
		if(!execpriv) return EPERM;
		if((r=vm_isokendpt(m->VMM_FORWHOM, &n)) != OK)
			return ESRCH;
	} else {
		/* regular mmap, i.e. for caller */
		if((r=vm_isokendpt(m->m_source, &n)) != OK) {
			panic("do_mmap: message from strange source: %d",
				m->m_source);
		}
	}

	vmp = &vmproc[n];

	if(m->VMM_FD == -1 || (m->VMM_FLAGS & MAP_ANON)) {
		/* actual memory in some form */
		mem_type_t *mt = NULL;

		if(m->VMM_FD != -1 || len <= 0) {
			printf("VM: mmap: fd %d, len 0x%x\n", m->VMM_FD, len);
			return EINVAL;
		}

		/* Contiguous phys memory has to be preallocated. */
		if((m->VMM_FLAGS & (MAP_CONTIG|MAP_PREALLOC)) == MAP_CONTIG) {
			return EINVAL;
		}

		if(m->VMM_FLAGS & MAP_CONTIG) {
			mt = &mem_type_anon_contig;
		} else	mt = &mem_type_anon;

		if(!(vr = mmap_region(vmp, addr, m->VMM_FLAGS, len,
			VR_WRITABLE | VR_ANON, mt, execpriv))) {
			return ENOMEM;
		}
	} else {
		return ENXIO;
	}

	/* Return mapping, as seen from process. */
	m->VMM_RETADDR = vr->vaddr;

	return OK;
}

/*===========================================================================*
 *				map_perm_check		     		     *
 *===========================================================================*/
int map_perm_check(endpoint_t caller, endpoint_t target,
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
int do_map_phys(message *m)
{
	int r, n;
	struct vmproc *vmp;
	endpoint_t target;
	struct vir_region *vr;
	vir_bytes len;
	phys_bytes startaddr;
	size_t offset;

	target = m->VMMP_EP;
	len = m->VMMP_LEN;

	if (len <= 0) return EINVAL;

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

	offset = startaddr % VM_PAGE_SIZE;
	len += offset;
	startaddr -= offset;

	if(len % VM_PAGE_SIZE)
		len += VM_PAGE_SIZE - (len % VM_PAGE_SIZE);

	if(!(vr = map_page_region(vmp, 0, VM_DATATOP, len, 
		VR_DIRECT | VR_WRITABLE, 0, &mem_type_directphys))) {
		return ENOMEM;
	}

	phys_setphys(vr, startaddr);

	m->VMMP_VADDR_REPLY = (void *) (vr->vaddr + offset);

	return OK;
}

/*===========================================================================*
 *				do_remap		     		     *
 *===========================================================================*/
int do_remap(message *m)
{
	int dn, sn;
	vir_bytes da, sa;
	size_t size;
	u32_t flags;
	struct vir_region *src_region, *vr;
	struct vmproc *dvmp, *svmp;
	int r;
	int readonly;

	if(m->m_type == VM_REMAP)
		readonly = 0;
	else if(m->m_type == VM_REMAP_RO)
		readonly = 1;
	else panic("do_remap: can't be");

	da = (vir_bytes) m->VMRE_DA;
	sa = (vir_bytes) m->VMRE_SA;
	size = m->VMRE_SIZE;

	if (size <= 0) return EINVAL;

	if ((r = vm_isokendpt((endpoint_t) m->VMRE_D, &dn)) != OK)
		return EINVAL;
	if ((r = vm_isokendpt((endpoint_t) m->VMRE_S, &sn)) != OK)
		return EINVAL;

	dvmp = &vmproc[dn];
	svmp = &vmproc[sn];

	if (!(src_region = map_lookup(svmp, sa, NULL)))
		return EINVAL;

	if(src_region->vaddr != sa) {
		printf("VM: do_remap: not start of region.\n");
		return EFAULT;
	}

	if (size % VM_PAGE_SIZE)  
		size += VM_PAGE_SIZE - size % VM_PAGE_SIZE;

	if(size != src_region->length) {
		printf("VM: do_remap: not size of region.\n");
		return EFAULT;
	}

	flags = VR_SHARED;
	if(!readonly)
		flags |= VR_WRITABLE;

	if(da)
		vr = map_page_region(dvmp, da, 0, size, flags, 0,
			&mem_type_shared);
	else
		vr = map_page_region(dvmp, 0, VM_DATATOP, size, flags, 0,
			&mem_type_shared);

	if(!vr) {
		printf("VM: re-map of shared area failed\n");
		return ENOMEM;
	}

	shared_setsource(vr, svmp->vm_endpoint, src_region);

	m->VMRE_RETA = (char *) vr->vaddr;
	return OK;
}

/*===========================================================================*
 *				do_get_phys		     		     *
 *===========================================================================*/
int do_get_phys(message *m)
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

	r = map_get_phys(vmp, addr, &ret);

	m->VMPHYS_RETA = ret;
	return r;
}

/*===========================================================================*
 *				do_get_refcount		     		     *
 *===========================================================================*/
int do_get_refcount(message *m)
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

	r = map_get_ref(vmp, addr, &cnt);

	m->VMREFCNT_RETC = cnt;
	return r;
}

/*===========================================================================*
 *                              do_munmap                                    *
 *===========================================================================*/
int do_munmap(message *m)
{
        int r, n;
        struct vmproc *vmp;
        vir_bytes addr, len;
	endpoint_t target = SELF;

	if(m->m_type == VM_UNMAP_PHYS) {
		target = m->VMUP_EP;
	} else if(m->m_type == VM_SHM_UNMAP) {
		target = m->VMUN_ENDPT;
	}

	if(target == SELF)
		target = m->m_source;

        if((r=vm_isokendpt(target, &n)) != OK) {
                panic("do_mmap: message from strange source: %d", m->m_source);
        }
 
        vmp = &vmproc[n];

	if(m->m_type == VM_UNMAP_PHYS) {
		addr = (vir_bytes) m->VMUP_VADDR;
	} else if(m->m_type == VM_SHM_UNMAP) {
		addr = (vir_bytes) m->VMUN_ADDR;
	} else	addr = (vir_bytes) m->VMUM_ADDR;

	if(addr % VM_PAGE_SIZE)
		return EFAULT;
 
	if(m->m_type == VM_UNMAP_PHYS || m->m_type == VM_SHM_UNMAP) {
		struct vir_region *vr;
	        if(!(vr = map_lookup(vmp, addr, NULL))) {
			printf("VM: unmap: address 0x%lx not found in %d\n",
	                       addr, target);
			sys_sysctl_stacktrace(target);
	                return EFAULT;
		}
		len = vr->length;
	} else len = roundup(m->VMUM_LEN, VM_PAGE_SIZE);

	return map_unmap_range(vmp, addr, len);
}

