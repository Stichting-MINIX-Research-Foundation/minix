
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
#include <memory.h>
#include <assert.h>

#include "proto.h"
#include "glo.h"
#include "util.h"
#include "region.h"

#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"
#include "kernel/proc.h"

#define SWAP_PROC_DEBUG 0

/*===========================================================================*
 *                              get_mem_map                                  *
 *===========================================================================*/
PUBLIC int get_mem_map(proc_nr, mem_map)
int proc_nr;                                    /* process to get map of */
struct mem_map *mem_map;                        /* put memory map here */
{
	struct proc p;
	int s;

	if ((s=sys_getproc(&p, proc_nr)) != OK)
		return(s);

	memcpy(mem_map, p.p_memmap, sizeof(p.p_memmap));
	return(OK);
}

/*===========================================================================*
 *                              get_mem_chunks                               *
 *===========================================================================*/
PUBLIC void get_mem_chunks(mem_chunks)
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
 *                              reserve_proc_mem                             *
 *===========================================================================*/
PUBLIC void reserve_proc_mem(mem_chunks, map_ptr)
struct memory *mem_chunks;                      /* store mem chunks here */
struct mem_map *map_ptr;                        /* memory to remove */
{
/* Remove server memory from the free memory list. The boot monitor
 * promises to put processes at the start of memory chunks. The 
 * tasks all use same base address, so only the first task changes
 * the memory lists. The servers and init have their own memory
 * spaces and their memory will be removed from the list.
 */
  struct memory *memp;
  for (memp = mem_chunks; memp < &mem_chunks[NR_MEMS]; memp++) {
        if (memp->base == map_ptr[T].mem_phys) {
                memp->base += map_ptr[T].mem_len + map_ptr[S].mem_vir;
                memp->size -= map_ptr[T].mem_len + map_ptr[S].mem_vir;
                break;
        }
  }
  if (memp >= &mem_chunks[NR_MEMS])
  {
		panic("reserve_proc_mem: can't find map in mem_chunks: 0x%lx",
			map_ptr[T].mem_phys);
  }
} 

/*===========================================================================*
 *                              vm_isokendpt                           	     *
 *===========================================================================*/
PUBLIC int vm_isokendpt(endpoint_t endpoint, int *proc)
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


struct proc mytmpproc;

/*===========================================================================*
 *                              get_stack_ptr                                *
 *===========================================================================*/
PUBLIC int get_stack_ptr(proc_nr_e, sp)
int proc_nr_e;                                  /* process to get sp of */   
vir_bytes *sp;                                  /* put stack pointer here */
{
  int s; 
  
  if ((s=sys_getproc(&mytmpproc, proc_nr_e)) != OK)     
        return(s);
  *sp = mytmpproc.p_reg.sp;
  return(OK);
}       

/*===========================================================================*
 *                              do_info                                      *
 *===========================================================================*/
PUBLIC int do_info(message *m)
{
	struct vm_stats_info vsi;
	struct vm_usage_info vui;
	static struct vm_region_info vri[MAX_VRI_COUNT];
	struct vmproc *vmp;
	vir_bytes addr, size, next, ptr;
	int r, pr, dummy, count;

	if (vm_isokendpt(m->m_source, &pr) != OK)
		return EINVAL;
	vmp = &vmproc[pr];

	ptr = (vir_bytes) m->VMI_PTR;

	switch(m->VMI_WHAT) {
	case VMIW_STATS:
		vsi.vsi_pagesize = VM_PAGE_SIZE;
		vsi.vsi_total = total_pages;
		memstats(&dummy, &vsi.vsi_free, &vsi.vsi_largest);

		addr = (vir_bytes) &vsi;
		size = sizeof(vsi);

		break;

	case VMIW_USAGE:
		if (vm_isokendpt(m->VMI_EP, &pr) != OK)
			return EINVAL;

		get_usage_info(&vmproc[pr], &vui);

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
	r = handle_memory(vmp, arch_vir2map(vmp, ptr), size, 1 /*wrflag*/);
	if (r != OK) return r;

	/* Now that we know the copy out will succeed, perform the actual copy
	 * operation.
	 */
	return sys_datacopy(SELF, addr,
		(vir_bytes) vmp->vm_endpoint, ptr, size);
}

/*===========================================================================*
 *				swap_proc	     			     *
 *===========================================================================*/
PUBLIC int swap_proc(endpoint_t src_e, endpoint_t dst_e)
{
	struct vmproc *src_vmp, *dst_vmp;
	struct vmproc orig_src_vmproc, orig_dst_vmproc;
	int src_p, dst_p, r;
	struct vir_region *vr;

	/* Lookup slots for source and destination process. */
	if(vm_isokendpt(src_e, &src_p) != OK) {
		printf("swap_proc: bad src endpoint %d\n", src_e);
		return EINVAL;
	}
	src_vmp = &vmproc[src_p];
	if(vm_isokendpt(dst_e, &dst_p) != OK) {
		printf("swap_proc: bad dst endpoint %d\n", dst_e);
		return EINVAL;
	}
	dst_vmp = &vmproc[dst_p];

#if SWAP_PROC_DEBUG
	printf("swap_proc: swapping %d (%d, %d) and %d (%d, %d)\n",
	    src_vmp->vm_endpoint, src_p, src_vmp->vm_slot,
	    dst_vmp->vm_endpoint, dst_p, dst_vmp->vm_slot);

	printf("swap_proc: map_printmap for source before swapping:\n");
	map_printmap(src_vmp);
	printf("swap_proc: map_printmap for destination before swapping:\n");
	map_printmap(dst_vmp);
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

	/* Preserve vir_region's parents. */
	for(vr = src_vmp->vm_regions; vr; vr = vr->next) {
		vr->parent = src_vmp;
	}
	for(vr = dst_vmp->vm_regions; vr; vr = vr->next) {
		vr->parent = dst_vmp;
	}

	/* Adjust page tables. */
	if(src_vmp->vm_flags & VMF_HASPT)
		pt_bind(&src_vmp->vm_pt, src_vmp);
	if(dst_vmp->vm_flags & VMF_HASPT)
		pt_bind(&dst_vmp->vm_pt, dst_vmp);
	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("swap_proc: VMCTL_FLUSHTLB failed: %d", r);
	}

#if SWAP_PROC_DEBUG
	printf("swap_proc: swapped %d (%d, %d) and %d (%d, %d)\n",
	    src_vmp->vm_endpoint, src_p, src_vmp->vm_slot,
	    dst_vmp->vm_endpoint, dst_p, dst_vmp->vm_slot);

	printf("swap_proc: map_printmap for source after swapping:\n");
	map_printmap(src_vmp);
	printf("swap_proc: map_printmap for destination after swapping:\n");
	map_printmap(dst_vmp);
#endif

	return OK;
}

