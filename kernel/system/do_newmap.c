/* The kernel call implemented in this file:
 *   m_type:	SYS_NEWMAP
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT		(install new map for this process)
 *    m1_p1:	PR_MEM_PTR		(pointer to the new memory map)
 */
#include "kernel/system.h"
#include <minix/endpoint.h>

#if USE_NEWMAP

/*===========================================================================*
 *				do_newmap				     *
 *===========================================================================*/
PUBLIC int do_newmap(struct proc * caller, message * m_ptr)
{
/* Handle sys_newmap().  Fetch the memory map. */
  struct proc *rp;	/* process whose map is to be loaded */
  struct mem_map *map_ptr;	/* virtual address of map inside caller */
  int proc_nr;

/*XXX*/vmmcall(0x12345604, 0, 100);
  map_ptr = (struct mem_map *) m_ptr->PR_MEM_PTR;
/*XXX*/vmmcall(0x12345604, 0, 101);
  if (! isokendpt(m_ptr->PR_ENDPT, &proc_nr)) return(EINVAL);
/*XXX*/vmmcall(0x12345604, 0, 102);
  if (iskerneln(proc_nr)) return(EPERM);
/*XXX*/vmmcall(0x12345604, 0, 103);
  rp = proc_addr(proc_nr);

/*XXX*/vmmcall(0x12345604, 0, 104);
  return newmap(caller, rp, map_ptr);
}


/*===========================================================================*
 *				newmap					     *
 *===========================================================================*/
PUBLIC int newmap(struct proc *caller, struct proc *rp, struct mem_map *map_ptr)
{
  int r;
/* Fetch the memory map. */
/*XXX*/vmmcall(0x12345604, 0, 105);
  if((r=data_copy(caller->p_endpoint, (vir_bytes) map_ptr,
	KERNEL, (vir_bytes) rp->p_memmap, sizeof(rp->p_memmap))) != OK) {
	printf("newmap: data_copy failed! (%d)\n", r);
	return r;
  }

/*XXX*/vmmcall(0x12345604, 0, 106);
  alloc_segments(rp);
/*XXX*/vmmcall(0x12345604, 0, 107);

  return(OK);
}
#endif /* USE_NEWMAP */

