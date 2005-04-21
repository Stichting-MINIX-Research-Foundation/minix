/* The system call implemented in this file:
 *   m_type:	SYS_NEWMAP
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR		(install new map for this process)
 *    m1_p1:	PR_MEM_PTR		(pointer to memory map)
 */

#include "../kernel.h"
#include "../system.h"
INIT_ASSERT

/*===========================================================================*
 *				do_newmap				     *
 *===========================================================================*/
PUBLIC int do_newmap(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_newmap().  Fetch the memory map from MM. */

  register struct proc *rp;
  phys_bytes src_phys;
  int caller;			/* whose space has the new map (usually MM) */
  int k;			/* process whose map is to be loaded */
  int old_flags;		/* value of flags before modification */
  struct mem_map *map_ptr;	/* virtual address of map inside caller (MM) */

  /* Extract message parameters and copy new memory map from MM. */
  caller = m_ptr->m_source;
  k = m_ptr->PR_PROC_NR;
  map_ptr = (struct mem_map *) m_ptr->PR_MEM_PTR;
  if (!isokprocn(k)) return(EINVAL);
  rp = proc_addr(k);		/* ptr to entry of user getting new map */

  /* Copy the map from MM. */
  src_phys = umap_local(proc_addr(caller), D, (vir_bytes) map_ptr, sizeof(rp->p_map));
  assert(src_phys != 0);
  phys_copy(src_phys, vir2phys(rp->p_map), (phys_bytes) sizeof(rp->p_map));

#if (CHIP != M68000)
  alloc_segments(rp);
#else
  pmmu_init_proc(rp);
#endif
  old_flags = rp->p_flags;	/* save the previous value of the flags */
  rp->p_flags &= ~NO_MAP;
  if (old_flags != 0 && rp->p_flags == 0) lock_ready(rp);

  return(OK);
}


