/* The kernel call implemented in this file:
 *   m_type:	SYS_UMAP
 *
 * The parameters for this kernel call are:
 *    m5_i1:	CP_SRC_PROC_NR	(process number)
 *    m5_s1:	UMAP_SEG	(segment where address is: T, D, or S)
 *    m5_l1:	CP_SRC_ADDR	(virtual address)
 *    m5_l2:	CP_DST_ADDR	(returns physical address)
 *    m5_l3:	CP_NR_BYTES	(size of datastructure)
 */

#include "kernel/system.h"

#include <minix/endpoint.h>

#if USE_UMAP

#if ! USE_UMAP_REMOTE
#undef do_umap_remote
#endif

/*==========================================================================*
 *				do_umap					    *
 *==========================================================================*/
int do_umap(struct proc * caller, message * m_ptr)
{
  int seg_index = m_ptr->UMAP_SEG & SEGMENT_INDEX;
  int endpt = (int) m_ptr->CP_SRC_ENDPT;

  /* This call is a subset of umap_remote, it allows mapping virtual addresses
   * in the caller's address space and grants where the caller is specified as
   * grantee; after the security check we simply invoke do_umap_remote
   */
  if (seg_index != MEM_GRANT && endpt != SELF) return EPERM;
  m_ptr->CP_DST_ENDPT = SELF;
  return do_umap_remote(caller, m_ptr);
}

#endif /* USE_UMAP */
