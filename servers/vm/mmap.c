
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
#include <memory.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "region.h"

/*===========================================================================*
 *				do_mmap			     		     *
 *===========================================================================*/
int do_mmap(message *m)
{
	int r, n;
	struct vmproc *vmp;
	int mfflags = 0;
	vir_bytes addr;
	struct vir_region *vr = NULL;
	int execpriv = 0;

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
		u32_t vrflags = VR_ANON | VR_WRITABLE;
		size_t len = (vir_bytes) m->VMM_LEN;

		if(m->VMM_FD != -1 || len <= 0) {
			return EINVAL;
		}

		/* Contiguous phys memory has to be preallocated. */
		if((m->VMM_FLAGS & (MAP_CONTIG|MAP_PREALLOC)) == MAP_CONTIG) {
			return EINVAL;
		}

		if(m->VMM_FLAGS & MAP_PREALLOC) mfflags |= MF_PREALLOC;
		if(m->VMM_FLAGS & MAP_LOWER16M) vrflags |= VR_LOWER16MB;
		if(m->VMM_FLAGS & MAP_LOWER1M)  vrflags |= VR_LOWER1MB;
		if(m->VMM_FLAGS & MAP_ALIGN64K) vrflags |= VR_PHYS64K;
		if(m->VMM_FLAGS & MAP_UNINITIALIZED) {
			if(!execpriv) return EPERM;
			vrflags |= VR_UNINITIALIZED;
		}
		if(m->VMM_FLAGS & MAP_IPC_SHARED) {
			vrflags |= VR_SHARED;
			/* Shared memory has to be preallocated. */
			if((m->VMM_FLAGS & (MAP_PREALLOC|MAP_ANON)) !=
				(MAP_PREALLOC|MAP_ANON)) {
				return EINVAL;
			}
		}
		if(m->VMM_FLAGS & MAP_CONTIG) vrflags |= VR_CONTIG;

		if(len % VM_PAGE_SIZE)
			len += VM_PAGE_SIZE - (len % VM_PAGE_SIZE);

		vr = NULL;
		if (m->VMM_ADDR || (m->VMM_FLAGS & MAP_FIXED)) {
			/* An address is given, first try at that address. */
			addr = m->VMM_ADDR;
			vr = map_page_region(vmp, addr, 0, len, MAP_NONE,
				vrflags, mfflags);
			if(!vr && (m->VMM_FLAGS & MAP_FIXED))
				return ENOMEM;
		}
		if (!vr) {
			/* No address given or address already in use. */
			vr = map_page_region(vmp, 0, VM_DATATOP, len,
				MAP_NONE, vrflags, mfflags);
		}
		if (!vr) {
			return ENOMEM;
		}
	} else {
		return ENOSYS;
	}

	/* Return mapping, as seen from process. */
	assert(vr);
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

	if(!(vr = map_page_region(vmp, 0, VM_DATATOP, len, startaddr,
		VR_DIRECT | VR_NOPF | VR_WRITABLE, 0))) {
		return ENOMEM;
	}

	m->VMMP_VADDR_REPLY = (void *) (vr->vaddr + offset);

	return OK;
}

/*===========================================================================*
 *				do_unmap_phys		     		     *
 *===========================================================================*/
int do_unmap_phys(message *m)
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

	if(!(region = map_lookup(vmp, (vir_bytes) m->VMUM_ADDR))) {
		return EINVAL;
	}

	if(!(region->flags & VR_DIRECT)) {
		return EINVAL;
	}

	if(map_unmap_region(vmp, region, 0, region->length) != OK) {
		return EINVAL;
	}

	return OK;
}

/*===========================================================================*
 *				do_remap		     		     *
 *===========================================================================*/
int do_remap(message *m)
{
	int dn, sn;
	vir_bytes da, sa, startv;
	size_t size;
	struct vir_region *region;
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

	/* da is not translated by arch_vir2map(),
	 * it's handled a little differently,
	 * since in map_remap(), we have to know
	 * about whether the user needs to bind to
	 * THAT address or be chosen by the system.
	 */
	if (!(region = map_lookup(svmp, sa)))
		return EINVAL;

	if(region->vaddr != sa) {
		printf("VM: do_remap: not start of region.\n");
		return EFAULT;
	}

	if(!(region->flags & VR_SHARED)) {
		printf("VM: do_remap: not shared.\n");
		return EFAULT;
	}

	if (size % VM_PAGE_SIZE)  
		size += VM_PAGE_SIZE - size % VM_PAGE_SIZE;

	if(size != region->length) {
		printf("VM: do_remap: not size of region.\n");
		return EFAULT;
	}

	if ((r = map_remap(dvmp, da, size, region, &startv, readonly)) != OK)
		return r;

	m->VMRE_RETA = (char *) startv;
	return OK;
}

/*===========================================================================*
 *				do_shared_unmap		     		     *
 *===========================================================================*/
int do_shared_unmap(message *m)
{
	int r, n;
	struct vmproc *vmp;
	endpoint_t target;
	struct vir_region *vr;
	vir_bytes addr;

	target = m->VMUN_ENDPT;
	if (target == SELF)
		target = m->m_source;

	if ((r = vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	vmp = &vmproc[n];

	addr = m->VMUN_ADDR;

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

	if(map_unmap_region(vmp, vr, 0, vr->length) != OK)
		panic("do_shared_unmap: map_unmap_region failed");

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
        vir_bytes addr, len, offset;
	struct vir_region *vr;

        if((r=vm_isokendpt(m->m_source, &n)) != OK) {
                panic("do_mmap: message from strange source: %d", m->m_source);
        }
 
        vmp = &vmproc[n];

	assert(m->m_type == VM_MUNMAP);
        addr = (vir_bytes) (vir_bytes) m->VMUM_ADDR;

        if(!(vr = map_lookup(vmp, addr))) {
                printf("VM: unmap: virtual address %p not found in %d\n",
                        m->VMUM_ADDR, vmp->vm_endpoint);
                return EFAULT;
        }

	if(addr % VM_PAGE_SIZE)
		return EFAULT;
 
	len = roundup(m->VMUM_LEN, VM_PAGE_SIZE);

	offset = addr - vr->vaddr;

	if(offset + len > vr->length) {
		printf("munmap: addr 0x%lx len 0x%lx spills out of region\n",
			addr, len);
		return EFAULT;
	}

	if(map_unmap_region(vmp, vr, offset, len) != OK)
		panic("do_munmap: map_unmap_region failed");

	return OK;
}

int unmap_ok = 0;

/*===========================================================================*
 *                     munmap_lin (used for overrides for VM)                *
 *===========================================================================*/
static int munmap_lin(vir_bytes addr, size_t len)
{
	if(addr % VM_PAGE_SIZE) {
		printf("munmap_lin: offset not page aligned\n");
		return EFAULT;
	}

	if(len % VM_PAGE_SIZE) {
		printf("munmap_lin: len not page aligned\n");
		return EFAULT;
	}

	if(pt_writemap(NULL, &vmproc[VM_PROC_NR].vm_pt, addr, MAP_NONE, len, 0,
		WMF_OVERWRITE | WMF_FREE) != OK) {
		printf("munmap_lin: pt_writemap failed\n");
		return EFAULT;
	}

	return OK;
}

/*===========================================================================*
 *                              munmap (override for VM)                    *
 *===========================================================================*/
int minix_munmap(void *addr, size_t len)
{
	vir_bytes laddr;
	if(!unmap_ok)
		return ENOSYS;
	laddr = (vir_bytes) (vir_bytes) addr;
	return munmap_lin(laddr, len);
}

