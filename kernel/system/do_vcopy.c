/* The system call implemented in this file:
 *   m_type:	SYS_VIRVCOPY, SYS_PHYSVCOPY 
 *
 * The parameters for this system call are:
 *    m5_c1:	CP_SRC_SPACE
 *    m5_l1:	CP_SRC_ADDR
 *    m5_i1:	CP_SRC_PROC_NR	
 *    m5_c2:	CP_DST_SPACE
 *    m5_l2:	CP_DST_ADDR	
 *    m5_i2:	CP_DST_PROC_NR	
 *    m5_l3:	CP_NR_BYTES
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
/* Handle sys_virvcopy(). Handle virtual copy requests from vector. */
  int nr_req;
  int caller_pid;
  vir_bytes caller_vir;
  phys_bytes caller_phys;
  phys_bytes kernel_phys;
  phys_bytes bytes;
  int i,s;
  struct vir_cp_req *req;

  /* Check if request vector size is ok. */
  nr_req = (unsigned) m_ptr->VCP_VEC_SIZE;
  if (nr_req > VCOPY_VEC_SIZE) return(EINVAL);
  bytes = nr_req * sizeof(struct vir_cp_req);

  /* Calculate physical addresses and copy (port,value)-pairs from user. */
  caller_pid = (int) m_ptr->m_source; 
  caller_vir = (vir_bytes) m_ptr->VCP_VEC_ADDR;
  caller_phys = umap_local(proc_addr(caller_pid), D, caller_vir, bytes);
  if (0 == caller_phys) return(EFAULT);
  kernel_phys = vir2phys(vir_cp_req);
  phys_copy(caller_phys, kernel_phys, (phys_bytes) bytes);

  /* Assume vector with requests is correct. Try to copy everything. */
  for (i=0; i<nr_req; i++) {

      req = &vir_cp_req[i];

      /* Check if physical addressing is used without SYS_PHYSVCOPY. */
      if (((req->src.segment | req->dst.segment) & PHYS_SEG) &&
              m_ptr->m_type != SYS_PHYSVCOPY) 
          return(EPERM);
      if ((s=virtual_copy(&req->src, &req->dst, req->count)) != OK) 
          return(s);
  }
  return(OK);
}

#endif /* (USE_VIRVCOPY || USE_PHYSVCOPY) */

