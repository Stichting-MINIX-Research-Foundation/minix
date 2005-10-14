/* The kernel call implemented in this file:
 *   m_type:	SYS_NEWMAP
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_PROC_NR		(install new map for this process)
 *    m1_p1:	PR_MEM_PTR		(pointer to the new memory map)
 */
#include "../system.h"

#if USE_NEWMAP

/*===========================================================================*
 *				do_newmap				     *
 *===========================================================================*/
PUBLIC int do_newmap(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_newmap().  Fetch the memory map from PM. */
  register struct proc *rp;	/* process whose map is to be loaded */
  int caller;			/* whose space has the new map (usually PM) */
  struct mem_map *map_ptr;	/* virtual address of map inside caller (PM) */
  phys_bytes src_phys;		/* physical address of map at the PM */
  int old_flags;		/* value of flags before modification */

  /* Extract message parameters and copy new memory map from PM. */
  caller = m_ptr->m_source;
  map_ptr = (struct mem_map *) m_ptr->PR_MEM_PTR;
  if (! isokprocn(m_ptr->PR_PROC_NR)) return(EINVAL);
  if (iskerneln(m_ptr->PR_PROC_NR)) return(EPERM);
  rp = proc_addr(m_ptr->PR_PROC_NR);

  /* Copy the map from PM. */
  src_phys = umap_local(proc_addr(caller), D, (vir_bytes) map_ptr, 
      sizeof(rp->p_memmap));
  if (src_phys == 0) return(EFAULT);
  phys_copy(src_phys,vir2phys(rp->p_memmap),(phys_bytes)sizeof(rp->p_memmap));

#if (CHIP != M68000)
  alloc_segments(rp);
#else
  pmmu_init_proc(rp);
#endif
  old_flags = rp->p_rts_flags;	/* save the previous value of the flags */
  rp->p_rts_flags &= ~NO_MAP;
  if (old_flags != 0 && rp->p_rts_flags == 0) lock_enqueue(rp);

  return(OK);
}
#endif /* USE_NEWMAP */

