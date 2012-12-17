
/* This file contains some utility routines for VM.  */

#define _SYSTEM 1

#define _MINIX 1	/* To get the brk() prototype (as _brk()). */

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
#include <string.h>
#include <errno.h>
#include <env.h>
#include <unistd.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/mman.h>

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
void get_mem_chunks(mem_chunks)
struct memory *mem_chunks;                      /* store mem chunks here */ 
{  
/* Initialize the free memory list from the 'memory' boot variable.  Translate
 * the byte offsets and sizes in this list to clicks, properly truncated.
 */
  phys_bytes base, size, limit;
  int i;
  struct memory *memp;

  /* Obtain and parse memory from system environment. */
  if(env_memory_parse(mem_chunks, NR_MEMS) != OK) 
        panic("couldn't obtain memory chunks"); 
        
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
int vm_isokendpt(endpoint_t endpoint, int *proc)
{
        *proc = _ENDPOINT_P(endpoint);
        if(*proc < 0 || *proc >= NR_PROCS)
		return EINVAL;
        if(*proc >= 0 && endpoint != vmproc[*proc].vm_endpoint)
                return EDEADEPT;
        if(*proc >= 0 && !(vmproc[*proc].vm_flags & VMF_INUSE))
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

	ptr = (vir_bytes) m->VMI_PTR;

	switch(m->VMI_WHAT) {
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
		if(m->VMI_EP < 0)
			get_usage_info_kernel(&vui);
		else if (vm_isokendpt(m->VMI_EP, &pr) != OK)
			return EINVAL;
		else get_usage_info(&vmproc[pr], &vui);

		addr = (vir_bytes) &vui;
		size = sizeof(vui);

		break;

	case VMIW_REGION:
		if (vm_isokendpt(m->VMI_EP, &pr) != OK)
			return EINVAL;

		count = MIN(m->VMI_COUNT, MAX_VRI_COUNT);
		next = m->VMI_NEXT;

		count = get_region_info(&vmproc[pr], vri, count, &next);

		m->VMI_COUNT = count;
		m->VMI_NEXT = next;

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
	r = handle_memory(vmp, ptr, size, 1 /*wrflag*/);
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
int swap_proc_dyn_data(struct vmproc *src_vmp, struct vmproc *dst_vmp)
{
	int is_vm;
	int r;

	is_vm = (dst_vmp->vm_endpoint == VM_PROC_NR);

        /* For VM, transfer memory regions above the stack first. */
        if(is_vm) {
#if LU_DEBUG
		printf("VM: swap_proc_dyn_data: tranferring regions above the stack from old VM (%d) to new VM (%d)\n",
			src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif
		r = pt_map_in_range(src_vmp, dst_vmp, VM_STACKTOP, 0);
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

	/* For regular processes, transfer regions above the stack now.
	 * In case of rollback, we need to skip this step. To sandbox the
	 * new instance and prevent state corruption on rollback, we share all
	 * the regions between the two instances as COW.
	 */
	if(!is_vm) {
		struct vir_region *vr;
		vr = map_lookup(dst_vmp, VM_STACKTOP, NULL);
		if(vr && !map_lookup(src_vmp, VM_STACKTOP, NULL)) {
#if LU_DEBUG
			printf("VM: swap_proc_dyn_data: tranferring regions above the stack from %d to %d\n",
				src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif
			r = map_proc_copy_from(src_vmp, dst_vmp, vr);
			if(r != OK) {
				return r;
			}
		}
	}

	return OK;
}

void *minix_mmap(void *addr, size_t len, int f, int f2, int f3, off_t o)
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

int minix_munmap(void * addr, size_t len)
{
	vm_freepages((vir_bytes) addr, roundup(len, VM_PAGE_SIZE)/VM_PAGE_SIZE);
	return 0;
}

int _brk(void *addr)
{
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
        ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW, 0) != OK) {
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


