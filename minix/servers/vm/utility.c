
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
#include <env.h>
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

/*===========================================================================*
 *			      swap_proc_dyn_data	     		     *
 *===========================================================================*/
int swap_proc_dyn_data(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	int sys_upd_flags)
{
	int is_vm;
	int r;
	struct vir_region *start_vr, *end_vr;

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

	/* Transfer memory mapped regions now. To sandbox the new instance and
	 * prevent state corruption on rollback, we share all the regions
	 * between the two instances as COW.
	 */
	start_vr = region_search(&dst_vmp->vm_regions_avl, VM_MMAPBASE, AVL_GREATER_EQUAL);
	end_vr = region_search(&dst_vmp->vm_regions_avl, VM_MMAPTOP, AVL_LESS);
	if(start_vr) {
#if LU_DEBUG
		printf("VM: swap_proc_dyn_data: tranferring memory mapped regions from %d to %d\n",
			dst_vmp->vm_endpoint, src_vmp->vm_endpoint);
#endif
		assert(end_vr);
		r = map_proc_copy_range(src_vmp, dst_vmp, start_vr, end_vr);
		if(r != OK) {
			return r;
		}
	}

	return OK;
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
	if ((res = vm_isokendpt(m->m_source, &slot)) != OK)
		return ESRCH;

	vmp = &vmproc[slot];

	if ((res = sys_datacopy(m->m_source, m->m_lc_vm_rusage.addr,
		SELF, (vir_bytes) &r_usage, (vir_bytes) sizeof(r_usage))) < 0)
		return res;

	r_usage.ru_maxrss = vmp->vm_total_max;
	r_usage.ru_minflt = vmp->vm_minor_page_fault;
	r_usage.ru_majflt = vmp->vm_major_page_fault;

	return sys_datacopy(SELF, (vir_bytes) &r_usage, m->m_source,
		m->m_lc_vm_rusage.addr, (vir_bytes) sizeof(r_usage));
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

