/* The system call implemented in this file:
 *   m_type:	SYS_GETMAP
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR		(process to get map of)
 *    m1_p1:	PR_MEM_PTR		(copy the memory map here)	
 */

#include "../kernel.h"
#include "../system.h"
INIT_ASSERT

/*===========================================================================*
 *				do_getmap				     *
 *===========================================================================*/
PUBLIC int do_getmap(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_getmap().  Report the memory map to MM. */

  register struct proc *rp;
  phys_bytes dst_phys;
  int caller;			/* where the map has to be stored */
  int k;			/* process whose map is to be loaded */
  struct mem_map *map_ptr;	/* virtual address of map inside caller (MM) */

  /* Extract message parameters and copy new memory map to MM. */
  caller = m_ptr->m_source;
  k = m_ptr->PR_PROC_NR;
  map_ptr = (struct mem_map *) m_ptr->PR_MEM_PTR;

  assert(isokprocn(k));		/* unlikely: MM sends a bad proc nr. */

  rp = proc_addr(k);		/* ptr to entry of the map */

  /* Copy the map to MM. */
  dst_phys = umap_local(proc_addr(caller), D, (vir_bytes) map_ptr, sizeof(rp->p_map));
  assert(dst_phys != 0);
  phys_copy(vir2phys(rp->p_map), dst_phys, sizeof(rp->p_map));

  return(OK);
}


