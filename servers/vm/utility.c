
/* This file contains some utility routines for VM.  */

#define _SYSTEM 1

#define _MINIX 1	/* To get the brk() prototype (as _brk()). */
#define brk _brk	/* Our brk() must redefine _brk(). */

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

#include "proto.h"
#include "glo.h"
#include "util.h"

#include <archconst.h>
#include <archtypes.h>
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"

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
        vm_panic("couldn't obtain memory chunks", NO_NUM); 
   
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
        vm_panic("reserve_proc_mem: can't find map in mem_chunks ",
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
                return EDEADSRCDST;
        if(*proc >= 0 && !(vmproc[*proc].vm_flags & VMF_INUSE))
                return EDEADSRCDST;
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
 *                              _brk                                         *
 *===========================================================================*/
extern char *_brksize;
PUBLIC int brk(brk_addr)
char *brk_addr;
{
        int r;
	struct vmproc *vmm = &vmproc[VM_PROC_NR];

/* VM wants to call brk() itself. */
        if((r=real_brk(vmm, (vir_bytes) brk_addr)) != OK)
		vm_panic("VM: brk() on myself failed\n", NO_NUM);
        _brksize = brk_addr;
        return 0;
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

