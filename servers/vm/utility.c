
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
#include <sys/param.h>

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
/* Remove server memory from the free memory list.
 */
  struct memory *memp;
  for (memp = mem_chunks; memp < &mem_chunks[NR_MEMS]; memp++) {
		if(memp->base <= map_ptr[T].mem_phys 
			&& memp->base+memp->size >= map_ptr[T].mem_phys)
		{
			if (memp->base == map_ptr[T].mem_phys) {
					memp->base += map_ptr[T].mem_len + map_ptr[S].mem_vir;
					memp->size -= map_ptr[T].mem_len + map_ptr[S].mem_vir;
			} else {
				struct memory *mempr;
				/* have to split mem_chunks */
				if(mem_chunks[NR_MEMS-1].size>0)
					panic("reserve_proc_mem: can't find free mem_chunks to map: 0x%lx",
						map_ptr[T].mem_phys);
				for(mempr=&mem_chunks[NR_MEMS-1];mempr>memp;mempr--) {
					*mempr=*(mempr-1);
				}
				assert(memp < &mem_chunks[NR_MEMS-1]);
				(memp+1)->base = map_ptr[T].mem_phys + map_ptr[T].mem_len + map_ptr[S].mem_vir;
				(memp+1)->size = memp->base + memp->size 
					- (map_ptr[T].mem_phys + map_ptr[T].mem_len + map_ptr[S].mem_vir);
				memp->size = map_ptr[T].mem_phys - memp->base;
			}
			break;
		}
  }
  if (memp >= &mem_chunks[NR_MEMS]) {
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
 *				swap_proc_slot	     			     *
 *===========================================================================*/
PUBLIC int swap_proc_slot(struct vmproc *src_vmp, struct vmproc *dst_vmp)
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

	/* Preserve yielded blocks. */
	src_vmp->vm_yielded_blocks = orig_src_vmproc.vm_yielded_blocks;
	dst_vmp->vm_yielded_blocks = orig_dst_vmproc.vm_yielded_blocks;

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
PUBLIC int swap_proc_dyn_data(struct vmproc *src_vmp, struct vmproc *dst_vmp)
{
	struct vir_region *vr;
	int is_vm;
	int r;

	is_vm = (dst_vmp->vm_endpoint == VM_PROC_NR);

        /* For VM, transfer memory regions above the stack first. */
        if(is_vm) {
#if LU_DEBUG
		printf("VM: swap_proc_dyn_data: tranferring regions above the stack from old VM (%d) to new VM (%d)\n",
			src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif
		assert(src_vmp->vm_stacktop == dst_vmp->vm_stacktop);
		r = pt_map_in_range(src_vmp, dst_vmp,
			arch_vir2map(src_vmp, src_vmp->vm_stacktop), 0);
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
	for(vr = src_vmp->vm_regions; vr; vr = vr->next) {
		USE(vr, vr->parent = src_vmp;);
	}
	for(vr = dst_vmp->vm_regions; vr; vr = vr->next) {
		USE(vr, vr->parent = dst_vmp;);
	}

	/* For regular processes, transfer regions above the stack now.
	 * In case of rollback, we need to skip this step. To sandbox the
	 * new instance and prevent state corruption on rollback, we share all
	 * the regions between the two instances as COW.
	 */
	if(!is_vm && (dst_vmp->vm_flags & VMF_HASPT)) {
		vr = map_lookup(dst_vmp, arch_vir2map(dst_vmp, dst_vmp->vm_stacktop));
		if(vr && !map_lookup(src_vmp, arch_vir2map(src_vmp, src_vmp->vm_stacktop))) {
#if LU_DEBUG
			printf("VM: swap_proc_dyn_data: tranferring regions above the stack from %d to %d\n",
				src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif
			assert(src_vmp->vm_stacktop == dst_vmp->vm_stacktop);
			r = map_proc_copy_from(src_vmp, dst_vmp, vr);
			if(r != OK) {
				return r;
			}
		}
	}

	return OK;
}

