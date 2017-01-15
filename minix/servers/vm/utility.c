
/* This file contains some utility routines for VM.  */

#define _SYSTEM		1

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
#include <minix/type.h>
#include <minix/bitmap.h>
#include <minix/rs.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "proto.h"
#include "glo.h"
#include "util.h"
#include "region.h"
#include "sanitycheck.h"

#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"
#include "kernel/proc.h"

/*===========================================================================*
 *                              get_mem_chunks                               *
 *===========================================================================*/
void get_mem_chunks(
struct memory *mem_chunks)                      /* store mem chunks here */ 
{  
/* Initialize the free memory list from the kernel-provided memory map.  Translate
 * the byte offsets and sizes in this list to clicks, properly truncated.
 */
  phys_bytes base, size, limit;
  int i;
  struct memory *memp;

  /* Initialize everything to zero. */
  memset(mem_chunks, 0, NR_MEMS*sizeof(*mem_chunks));

  /* Obtain and parse memory from kernel environment. */
  /* XXX Any memory chunk in excess of NR_MEMS is silently ignored. */
  for(i = 0; i < MIN(MAXMEMMAP, NR_MEMS); i++) {
  	mem_chunks[i].base = kernel_boot_info.memmap[i].mm_base_addr;
  	mem_chunks[i].size = kernel_boot_info.memmap[i].mm_length;
  }

  /* Round physical memory to clicks. Round start up, round end down. */
  for (i = 0; i < NR_MEMS; i++) {
        memp = &mem_chunks[i];          /* next mem chunk is stored here */
        base = mem_chunks[i].base;
        size = mem_chunks[i].size;
        limit = base + size;
        base = (phys_bytes) (CLICK_CEIL(base));
        limit = (phys_bytes) (CLICK_FLOOR(limit));
        if (limit <= base) {
                memp->base = memp->size = 0;
        } else { 
                memp->base = base >> CLICK_SHIFT;
                memp->size = (limit - base) >> CLICK_SHIFT;
        }
  }
}  

/*===========================================================================*
 *                              vm_isokendpt                           	     *
 *===========================================================================*/
int vm_isokendpt(endpoint_t endpoint, int *procn)
{
        *procn = _ENDPOINT_P(endpoint);
        if(*procn < 0 || *procn >= NR_PROCS)
		return EINVAL;
        if(*procn >= 0 && endpoint != vmproc[*procn].vm_endpoint)
                return EDEADEPT;
        if(*procn >= 0 && !(vmproc[*procn].vm_flags & VMF_INUSE))
                return EDEADEPT;
        return OK;
}


/*===========================================================================*
 *                              do_info                                      *
 *===========================================================================*/
int do_info(message *m)
{
	struct vm_stats_info vsi;
	struct vm_usage_info vui;
	static struct vm_region_info vri[MAX_VRI_COUNT];
	struct vmproc *vmp;
	vir_bytes addr, size, next, ptr;
	int r, pr, dummy, count, free_pages, largest_contig;

	if (vm_isokendpt(m->m_source, &pr) != OK)
		return EINVAL;
	vmp = &vmproc[pr];

	ptr = (vir_bytes) m->m_lsys_vm_info.ptr;

	switch(m->m_lsys_vm_info.what) {
	case VMIW_STATS:
		vsi.vsi_pagesize = VM_PAGE_SIZE;
		vsi.vsi_total = total_pages;
		memstats(&dummy, &free_pages, &largest_contig);
		vsi.vsi_free = free_pages;
		vsi.vsi_largest = largest_contig;

		get_stats_info(&vsi);

		addr = (vir_bytes) &vsi;
		size = sizeof(vsi);

		break;

	case VMIW_USAGE:
		if(m->m_lsys_vm_info.ep < 0)
			get_usage_info_kernel(&vui);
		else if (vm_isokendpt(m->m_lsys_vm_info.ep, &pr) != OK)
			return EINVAL;
		else get_usage_info(&vmproc[pr], &vui);

		addr = (vir_bytes) &vui;
		size = sizeof(vui);

		break;

	case VMIW_REGION:
		if(m->m_lsys_vm_info.ep == SELF) {
			m->m_lsys_vm_info.ep = m->m_source;
		}
		if (vm_isokendpt(m->m_lsys_vm_info.ep, &pr) != OK)
			return EINVAL;

		count = MIN(m->m_lsys_vm_info.count, MAX_VRI_COUNT);
		next = m->m_lsys_vm_info.next;

		count = get_region_info(&vmproc[pr], vri, count, &next);

		m->m_lsys_vm_info.count = count;
		m->m_lsys_vm_info.next = next;

		addr = (vir_bytes) vri;
		size = sizeof(vri[0]) * count;

		break;

	default:
		return EINVAL;
	}

	if (size == 0)
		return OK;

	/* Make sure that no page faults can occur while copying out. A page
	 * fault would cause the kernel to send a notify to us, while we would
	 * be waiting for the result of the copy system call, resulting in a
	 * deadlock. Note that no memory mapping can be undone without the
	 * involvement of VM, so we are safe until we're done.
	 */
	r = handle_memory_once(vmp, ptr, size, 1 /*wrflag*/);
	if (r != OK) return r;

	/* Now that we know the copy out will succeed, perform the actual copy
	 * operation.
	 */
	return sys_datacopy(SELF, addr,
		(vir_bytes) vmp->vm_endpoint, ptr, size);
}

/*===========================================================================*
 *				swap_proc_slot	     			     *
 *===========================================================================*/
int swap_proc_slot(struct vmproc *src_vmp, struct vmproc *dst_vmp)
{
	struct vmproc orig_src_vmproc, orig_dst_vmproc;

#if LU_DEBUG
	printf("VM: swap_proc: swapping %d (%d) and %d (%d)\n",
	    src_vmp->vm_endpoint, src_vmp->vm_slot,
	    dst_vmp->vm_endpoint, dst_vmp->vm_slot);
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

#if LU_DEBUG
	printf("VM: swap_proc: swapped %d (%d) and %d (%d)\n",
	    src_vmp->vm_endpoint, src_vmp->vm_slot,
	    dst_vmp->vm_endpoint, dst_vmp->vm_slot);
#endif

	return OK;
}

/*
 * Transfer memory mapped regions, using CoW sharing, from 'src_vmp' to
 * 'dst_vmp', for the source process's address range of 'start_addr'
 * (inclusive) to 'end_addr' (exclusive).  Return OK or an error code.
 * If the regions seem to have been transferred already, do nothing.
 */
static int
transfer_mmap_regions(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	vir_bytes start_addr, vir_bytes end_addr)
{
	struct vir_region *start_vr, *check_vr, *end_vr;

	start_vr = region_search(&src_vmp->vm_regions_avl, start_addr,
	    AVL_GREATER_EQUAL);

	if (start_vr == NULL || start_vr->vaddr >= end_addr)
		return OK; /* nothing to do */

	/* In the case of multicomponent live update that includes VM, this
	 * function may be called for the same process more than once, for the
	 * sake of keeping code paths as little divergent as possible while at
	 * the same time ensuring that the regions are copied early enough.
	 *
	 * To compensate for these multiple calls, we perform a very simple
	 * check here to see if the region to transfer is already present in
	 * the target process.  If so, we can safely skip copying the regions
	 * again, because there is no other possible explanation for the
	 * region being present already.  Things would go horribly wrong if we
	 * tried copying anyway, but this check is not good enough to detect
	 * all such problems, since we do a check on the base address only.
	 */
	check_vr = region_search(&dst_vmp->vm_regions_avl, start_vr->vaddr,
	    AVL_EQUAL);
	if (check_vr != NULL) {
#if LU_DEBUG
		printf("VM: transfer_mmap_regions: skipping transfer from "
		    "%d to %d (0x%lx already present)\n",
		    src_vmp->vm_endpoint, dst_vmp->vm_endpoint,
		    start_vr->vaddr);
#endif
		return OK;
	}

	end_vr = region_search(&src_vmp->vm_regions_avl, end_addr, AVL_LESS);
	assert(end_vr != NULL);
	assert(start_vr->vaddr <= end_vr->vaddr);

#if LU_DEBUG
	printf("VM: transfer_mmap_regions: transferring memory mapped regions "
	    "from %d to %d (0x%lx to 0x%lx)\n", src_vmp->vm_endpoint,
	    dst_vmp->vm_endpoint, start_vr->vaddr, end_vr->vaddr);
#endif

	return map_proc_copy_range(dst_vmp, src_vmp, start_vr, end_vr);
}

/*
 * Create copy-on-write mappings in process 'dst_vmp' for all memory-mapped
 * regions present in 'src_vmp'.  Return OK on success, or an error otherwise.
 * In the case of failure, successfully created mappings are not undone.
 */
int
map_proc_dyn_data(struct vmproc *src_vmp, struct vmproc *dst_vmp)
{
	int r;

#if LU_DEBUG
	printf("VM: mapping dynamic data from %d to %d\n",
	    src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif

	/* Transfer memory mapped regions now. To sandbox the new instance and
	 * prevent state corruption on rollback, we share all the regions
	 * between the two instances as COW.
	 */
	r = transfer_mmap_regions(src_vmp, dst_vmp, VM_MMAPBASE, VM_MMAPTOP);

	/* If the stack is not mapped at the VM_DATATOP, there might be some
	 * more regions hiding above the stack.  We also have to transfer
	 * those.
	 */
	if (r == OK && VM_STACKTOP < VM_DATATOP)
		r = transfer_mmap_regions(src_vmp, dst_vmp, VM_STACKTOP,
		    VM_DATATOP);

	return r;
}

/*===========================================================================*
 *			      swap_proc_dyn_data	     		     *
 *===========================================================================*/
int swap_proc_dyn_data(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	int sys_upd_flags)
{
	int is_vm;
	int r;

	is_vm = (dst_vmp->vm_endpoint == VM_PROC_NR);

        /* For VM, transfer memory mapped regions first. */
        if(is_vm) {
#if LU_DEBUG
		printf("VM: swap_proc_dyn_data: tranferring memory mapped regions from old (%d) to new VM (%d)\n",
			src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif
		r = pt_map_in_range(src_vmp, dst_vmp, VM_OWN_HEAPBASE, VM_OWN_MMAPTOP);
		if(r != OK) {
			printf("swap_proc_dyn_data: pt_map_in_range failed\n");
			return r;
		}
		r = pt_map_in_range(src_vmp, dst_vmp, VM_STACKTOP, VM_DATATOP);
		if(r != OK) {
			printf("swap_proc_dyn_data: pt_map_in_range failed\n");
			return r;
		}

        }

#if LU_DEBUG
	printf("VM: swap_proc_dyn_data: swapping regions' parents for %d (%d) and %d (%d)\n",
	    src_vmp->vm_endpoint, src_vmp->vm_slot,
	    dst_vmp->vm_endpoint, dst_vmp->vm_slot);
#endif

	/* Swap vir_regions' parents. */
	map_setparent(src_vmp);
	map_setparent(dst_vmp);

	/* Don't transfer mmapped regions if not required. */
	if(is_vm || (sys_upd_flags & (SF_VM_ROLLBACK|SF_VM_NOMMAP))) {
		return OK;
	}

	/* Make sure regions are consistent. */
	assert(region_search_root(&src_vmp->vm_regions_avl) && region_search_root(&dst_vmp->vm_regions_avl));

	/* Source and destination are intentionally swapped here! */
	return map_proc_dyn_data(dst_vmp, src_vmp);
}

void *mmap(void *addr, size_t len, int f, int f2, int f3, off_t o)
{
	void *ret;
	phys_bytes p;

	assert(!addr);
	assert(!(len % VM_PAGE_SIZE));

	ret = vm_allocpages(&p, VMP_SLAB, len/VM_PAGE_SIZE);

	if(!ret) return MAP_FAILED;
	memset(ret, 0, len);
	return ret;
}

int munmap(void * addr, size_t len)
{
	vm_freepages((vir_bytes) addr, roundup(len, VM_PAGE_SIZE)/VM_PAGE_SIZE);
	return 0;
}

#ifdef __weak_alias
__weak_alias(brk, _brk)
#endif
int _brk(void *addr)
{
	/* brk is a special case function to allow vm itself to
	   allocate memory in it's own (cacheable) HEAP */
	vir_bytes target = roundup((vir_bytes)addr, VM_PAGE_SIZE), v;
	extern char _end;
	extern char *_brksize;
	static vir_bytes prevbrk = (vir_bytes) &_end;
	struct vmproc *vmprocess = &vmproc[VM_PROC_NR];

	for(v = roundup(prevbrk, VM_PAGE_SIZE); v < target;
		v += VM_PAGE_SIZE) {
		phys_bytes mem, newpage = alloc_mem(1, 0);
		if(newpage == NO_MEM) return -1;
		mem = CLICK2ABS(newpage);
		if(pt_writemap(vmprocess, &vmprocess->vm_pt,
			v, mem, VM_PAGE_SIZE,
			  ARCH_VM_PTE_PRESENT
			| ARCH_VM_PTE_USER
			| ARCH_VM_PTE_RW
#if defined(__arm__)
			| ARM_VM_PTE_CACHED
#endif
			, 0) != OK) {
			free_mem(newpage, 1);
			return -1;
		}
		prevbrk = v + VM_PAGE_SIZE;
	}

        _brksize = (char *) addr;

        if(sys_vmctl(SELF, VMCTL_FLUSHTLB, 0) != OK)
        	panic("flushtlb failed");

	return 0;
}

/*===========================================================================*
 *				do_getrusage		     		     *
 *===========================================================================*/
int do_getrusage(message *m)
{
	int res, slot;
	struct vmproc *vmp;
	struct rusage r_usage;

	/* If the request is not from PM, it is coming directly from userland.
	 * This is an obsolete construction. In the future, userland programs
	 * should no longer be allowed to call vm_getrusage(2) directly at all.
	 * For backward compatibility, we simply return success for now.
	 */
	if (m->m_source != PM_PROC_NR)
		return OK;

	/* Get the process for which resource usage is requested. */
	if ((res = vm_isokendpt(m->m_lsys_vm_rusage.endpt, &slot)) != OK)
		return ESRCH;

	vmp = &vmproc[slot];

	/* We are going to change only a few fields, so copy in the rusage
	 * structure first. The structure is still in PM's address space at
	 * this point, so use the message source.
	 */
	if ((res = sys_datacopy(m->m_source, m->m_lsys_vm_rusage.addr,
		SELF, (vir_bytes) &r_usage, (vir_bytes) sizeof(r_usage))) < 0)
		return res;

	if (!m->m_lsys_vm_rusage.children) {
		r_usage.ru_maxrss = vmp->vm_total_max / 1024L; /* unit is KB */
		r_usage.ru_minflt = vmp->vm_minor_page_fault;
		r_usage.ru_majflt = vmp->vm_major_page_fault;
	} else {
		/* XXX TODO: return the fields for terminated, waited-for
		 * children of the given process. We currently do not have this
		 * information! In the future, rather than teaching VM about
		 * the process hierarchy, PM should probably tell VM at process
		 * exit time which other process should inherit its resource
		 * usage fields. For now, we assume PM clears the fields before
		 * making this call, so we don't zero the fields explicitly.
		 */
	}

	/* Copy out the resulting structure back to PM. */
	return sys_datacopy(SELF, (vir_bytes) &r_usage, m->m_source,
		m->m_lsys_vm_rusage.addr, (vir_bytes) sizeof(r_usage));
}

/*===========================================================================*
 *                            adjust_proc_refs                              *
 *===========================================================================*/
void adjust_proc_refs()
{
       struct vmproc *vmp;
       region_iter iter;

       /* Fix up region parents. */
       for(vmp = vmproc; vmp < &vmproc[VMP_NR]; vmp++) {
               struct vir_region *vr;
               if(!(vmp->vm_flags & VMF_INUSE))
                       continue;
               region_start_iter_least(&vmp->vm_regions_avl, &iter);
               while((vr = region_get_iter(&iter))) {
                       USE(vr, vr->parent = vmp;);
                       region_incr_iter(&iter);
               }
       }
}

