/* The kernel call implemented in this file:
 *   m_type:	SYS_VIRVCOPY, SYS_PHYSVCOPY 
 *
 * The parameters for this kernel call are:
 *    m1_i3:	VCP_VEC_SIZE		size of copy request vector 
 *    m1_p1:	VCP_VEC_ADDR		address of vector at caller 
 *    m1_i2:	VCP_NR_OK		number of successfull copies	
 */

#include "../system.h"
#include <minix/type.h>

#if (USE_VIRVCOPY || USE_PHYSVCOPY)

/* Buffer to hold copy request vector from user. */
PRIVATE struct vir_cp_req vir_cp_req[VCOPY_VEC_SIZE];

/*===========================================================================*
 *				do_vcopy					     *
 *===========================================================================*/
PUBLIC int do_vcopy(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_virvcopy() and sys_physvcopy() that pass a vector with copy
 * requests. Although a single handler function is used, there are two
 * different kernel calls so that permissions can be checked.
 */
  int nr_req, r;
  vir_bytes caller_vir;
  phys_bytes bytes;
  int i,s;
  struct vir_cp_req *req;
  struct vir_addr src, dst;
  struct proc *pr;

  { static int first=1;
	if (first)
	{
		first= 0;
		kprintf("do_vcopy: got request from %d\n", m_ptr->m_source);
	}
  }

  if(!(pr = endpoint_lookup(who_e)))
	minix_panic("do_vcopy: caller doesn't exist", who_e);

  /* Check if request vector size is ok. */
  nr_req = (unsigned) m_ptr->VCP_VEC_SIZE;
  if (nr_req > VCOPY_VEC_SIZE) return(EINVAL);
  bytes = nr_req * sizeof(struct vir_cp_req);

  /* Calculate physical addresses and copy (port,value)-pairs from user. */
  src.segment = dst.segment = D;
  src.proc_nr_e = who_e;
  dst.proc_nr_e = SYSTEM;
  dst.offset = (vir_bytes) vir_cp_req;
  src.offset = (vir_bytes) m_ptr->VCP_VEC_ADDR;

  if((r=virtual_copy_vmcheck(&src, &dst, bytes)) != OK)
	return r;

  /* Assume vector with requests is correct. Try to copy everything. */
  m_ptr->VCP_NR_OK = 0;
  for (i=0; i<nr_req; i++) {

      req = &vir_cp_req[i];

      /* Check if physical addressing is used without SYS_PHYSVCOPY. */
      if (((req->src.segment | req->dst.segment) & PHYS_SEG) &&
              m_ptr->m_type != SYS_PHYSVCOPY) return(EPERM);
      if ((s=virtual_copy_vmcheck(&req->src, &req->dst, req->count)) != OK) 
          return(s);
      m_ptr->VCP_NR_OK ++;
  }
  return(OK);
}

#endif /* (USE_VIRVCOPY || USE_PHYSVCOPY) */

