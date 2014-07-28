
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
#include <minix/debug.h>

#include <machine/vmparam.h>

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
	if(vmm_flags & MAP_ALIGNMENT_64KB) vrflags |= VR_PHYS64K;
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

static int mmap_file(struct vmproc *vmp,
	int vmfd, off_t file_offset, int flags,
	ino_t ino, dev_t dev, u64_t filesize, vir_bytes addr, vir_bytes len,
	vir_bytes *retaddr, u16_t clearend, int writable, int mayclosefd)
{
/* VFS has replied to a VMVFSREQ_FDLOOKUP request. */
	struct vir_region *vr;
	u64_t page_offset;
	int result = OK;
	u32_t vrflags = 0;

	if(writable) vrflags |= VR_WRITABLE;

	/* Do some page alignments. */
	if((page_offset = (file_offset % VM_PAGE_SIZE))) {
		file_offset -= page_offset;
		len += page_offset;
	}

	len = roundup(len, VM_PAGE_SIZE);

	/* All numbers should be page-aligned now. */
	assert(!(len % VM_PAGE_SIZE));
	assert(!(filesize % VM_PAGE_SIZE));
	assert(!(file_offset % VM_PAGE_SIZE));

#if 0
	/* XXX ld.so relies on longer-than-file mapping */
	if((u64_t) len + file_offset > filesize) {
		printf("VM: truncating mmap dev 0x%x ino %d beyond file size in %d; offset %llu, len %lu, size %llu; ",
			dev, ino, vmp->vm_endpoint,
			file_offset, len, filesize);
		len = filesize - file_offset;
		return EINVAL;
	}
#endif

	if(!(vr = mmap_region(vmp, addr, flags, len,
		vrflags, &mem_type_mappedfile, 0))) {
		result = ENOMEM;
	} else {
		*retaddr = vr->vaddr + page_offset;
		result = OK;

		mappedfile_setfile(vmp, vr, vmfd,
			file_offset, dev, ino, clearend, 1, mayclosefd);
	}

	return result;
}

int do_vfs_mmap(message *m)
{
	vir_bytes v;
	struct vmproc *vmp;
	int r, n;
	u16_t clearend, flags = 0;

	/* It might be disabled */
	if(!enable_filemap) return ENXIO;

	clearend = m->m_vm_vfs_mmap.clearend;
	flags = m->m_vm_vfs_mmap.flags;

	if((r=vm_isokendpt(m->m_vm_vfs_mmap.who, &n)) != OK)
		panic("bad ep %d from vfs", m->m_vm_vfs_mmap.who);
	vmp = &vmproc[n];

	return mmap_file(vmp, m->m_vm_vfs_mmap.fd, m->m_vm_vfs_mmap.offset,
		MAP_PRIVATE | MAP_FIXED,
		m->m_vm_vfs_mmap.ino, m->m_vm_vfs_mmap.dev,
		(u64_t) LONG_MAX * VM_PAGE_SIZE,
		m->m_vm_vfs_mmap.vaddr, m->m_vm_vfs_mmap.len, &v,
		clearend, flags, 0);
}

static void mmap_file_cont(struct vmproc *vmp, message *replymsg, void *cbarg,
	void *origmsg_v)
{
	message *origmsg = (message *) origmsg_v;
	message mmap_reply;
	int result;
	int writable = 0;
	vir_bytes v = (vir_bytes) MAP_FAILED;

	if(origmsg->m_mmap.prot & PROT_WRITE)
		writable = 1;

	if(replymsg->VMV_RESULT != OK) {
#if 0   /* Noisy diagnostic for mmap() by ld.so */
		printf("VM: VFS reply failed (%d)\n", replymsg->VMV_RESULT);
		sys_diagctl_stacktrace(vmp->vm_endpoint);
#endif
		result = origmsg->VMV_RESULT;
	} else {
		/* Finish mmap */
		result = mmap_file(vmp, replymsg->VMV_FD, origmsg->m_mmap.offset,
			origmsg->m_mmap.flags, 
			replymsg->VMV_INO, replymsg->VMV_DEV,
			(u64_t) replymsg->VMV_SIZE_PAGES*PAGE_SIZE,
			(vir_bytes) origmsg->m_mmap.addr,
			origmsg->m_mmap.len, &v, 0, writable, 1);
	}

	/* Unblock requesting process. */
	memset(&mmap_reply, 0, sizeof(mmap_reply));
	mmap_reply.m_type = result;
	mmap_reply.m_mmap.retaddr = (void *) v;

	if(ipc_send(vmp->vm_endpoint, &mmap_reply) != OK)
		panic("VM: mmap_file_cont: ipc_send() failed");
}

/*===========================================================================*
 *				do_mmap			     		     *
 *===========================================================================*/
int do_mmap(message *m)
{
	int r, n;
	struct vmproc *vmp;
	vir_bytes addr = (vir_bytes) m->m_mmap.addr;
	struct vir_region *vr = NULL;
	int execpriv = 0;
	size_t len = (vir_bytes) m->m_mmap.len;

	/* RS and VFS can do slightly more special mmap() things */
	if(m->m_source == VFS_PROC_NR || m->m_source == RS_PROC_NR)
		execpriv = 1;

	if(m->m_mmap.flags & MAP_THIRDPARTY) {
		if(!execpriv) return EPERM;
		if((r=vm_isokendpt(m->m_mmap.forwhom, &n)) != OK)
			return ESRCH;
	} else {
		/* regular mmap, i.e. for caller */
		if((r=vm_isokendpt(m->m_source, &n)) != OK) {
			panic("do_mmap: message from strange source: %d",
				m->m_source);
		}
	}

	vmp = &vmproc[n];

	/* "SUSv3 specifies that mmap() should fail if length is 0" */
	if(len <= 0) {
		return EINVAL;
	}

	if(m->m_mmap.fd == -1 || (m->m_mmap.flags & MAP_ANON)) {
		/* actual memory in some form */
		mem_type_t *mt = NULL;

		if(m->m_mmap.fd != -1) {
			printf("VM: mmap: fd %d, len 0x%x\n", m->m_mmap.fd, len);
			return EINVAL;
		}

		/* Contiguous phys memory has to be preallocated. */
		if((m->m_mmap.flags & (MAP_CONTIG|MAP_PREALLOC)) == MAP_CONTIG) {
			return EINVAL;
		}

		if(m->m_mmap.flags & MAP_CONTIG) {
			mt = &mem_type_anon_contig;
		} else	mt = &mem_type_anon;

		if(!(vr = mmap_region(vmp, addr, m->m_mmap.flags, len,
			VR_WRITABLE | VR_ANON, mt, execpriv))) {
			return ENOMEM;
		}
	} else {
		/* File mapping might be disabled */
		if(!enable_filemap) return ENXIO;

		/* For files, we only can't accept writable MAP_SHARED
		 * mappings.
		 */
		if((m->m_mmap.flags & MAP_SHARED) && (m->m_mmap.prot & PROT_WRITE)) {
			return ENXIO;
		}

		if(vfs_request(VMVFSREQ_FDLOOKUP, m->m_mmap.fd, vmp, 0, 0,
			mmap_file_cont, NULL, m, sizeof(*m)) != OK) {
			printf("VM: vfs_request for mmap failed\n");
			return ENXIO;
		}

		/* request queued; don't reply. */
		return SUSPEND;
	}

	/* Return mapping, as seen from process. */
	m->m_mmap.retaddr = (void *) vr->vaddr;

	return OK;
}

/*===========================================================================*
 *				map_perm_check		     		     *
 *===========================================================================*/
static int map_perm_check(endpoint_t caller, endpoint_t target,
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

	target = m->m_lsys_vm_map_phys.ep;
	len = m->m_lsys_vm_map_phys.len;

	if (len <= 0) return EINVAL;

	if(target == SELF)
		target = m->m_source;

	if((r=vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	startaddr = (vir_bytes)m->m_lsys_vm_map_phys.phaddr;

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

	m->m_lsys_vm_map_phys.reply = (void *) (vr->vaddr + offset);

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

	da = (vir_bytes) m->m_lsys_vm_vmremap.dest_addr;
	sa = (vir_bytes) m->m_lsys_vm_vmremap.src_addr;
	size = m->m_lsys_vm_vmremap.size;

	if (size <= 0) return EINVAL;

	if ((r = vm_isokendpt((endpoint_t) m->m_lsys_vm_vmremap.destination, &dn)) != OK)
		return EINVAL;
	if ((r = vm_isokendpt((endpoint_t) m->m_lsys_vm_vmremap.source, &sn)) != OK)
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

	m->m_lsys_vm_vmremap.ret_addr = (void *) vr->vaddr;
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

	target = m->m_lc_vm_getphys.endpt;
	addr = (vir_bytes) m->m_lc_vm_getphys.addr;

	if ((r = vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	vmp = &vmproc[n];

	r = map_get_phys(vmp, addr, &ret);

	m->m_lc_vm_getphys.ret_addr = (void *) ret;
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

	target = m->m_lsys_vm_getref.endpt;
	addr = (vir_bytes) m->m_lsys_vm_getref.addr;

	if ((r = vm_isokendpt(target, &n)) != OK)
		return EINVAL;

	vmp = &vmproc[n];

	r = map_get_ref(vmp, addr, &cnt);

	m->m_lsys_vm_getref.retc = cnt;
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
		target = m->m_lsys_vm_unmap_phys.ep;
	} else if(m->m_type == VM_SHM_UNMAP) {
		target = m->m_lc_vm_shm_unmap.forwhom;
	}

	if(target == SELF)
		target = m->m_source;

        if((r=vm_isokendpt(target, &n)) != OK) {
                panic("do_mmap: message from strange source: %d", m->m_source);
        }
 
        vmp = &vmproc[n];

	if(m->m_type == VM_UNMAP_PHYS) {
		addr = (vir_bytes) m->m_lsys_vm_unmap_phys.vaddr;
	} else if(m->m_type == VM_SHM_UNMAP) {
		addr = (vir_bytes) m->m_lc_vm_shm_unmap.addr;
	} else	addr = (vir_bytes) m->VMUM_ADDR;

	if(addr % VM_PAGE_SIZE)
		return EFAULT;
 
	if(m->m_type == VM_UNMAP_PHYS || m->m_type == VM_SHM_UNMAP) {
		struct vir_region *vr;
	        if(!(vr = map_lookup(vmp, addr, NULL))) {
			printf("VM: unmap: address 0x%lx not found in %d\n",
	                       addr, target);
			sys_diagctl_stacktrace(target);
	                return EFAULT;
		}
		len = vr->length;
	} else len = roundup(m->VMUM_LEN, VM_PAGE_SIZE);

	return map_unmap_range(vmp, addr, len);
}

