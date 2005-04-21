/* The system call implemented in this file:
 *   m_type:	SYS_VCOPY
 *
 * The parameters for this system call are:
 *    m1_i1:	VCP_SRC_PROC	(source process number)
 *    m1_i2:	VCP_DST_PROC	(destination process number)
 *    m1_i3:	VCP_VEC_SIZE	(vector size)
 *    m1_p1:	VCP_VEC_ADDR	(pointer to vector)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *				do_vcopy				     *
 *===========================================================================*/
PUBLIC int do_vcopy(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_vcopy(). Copy multiple blocks of memory */

  int src_proc, dst_proc, vect_s, i;
  vir_bytes src_vir, dst_vir, vect_addr;
  phys_bytes src_phys, dst_phys, bytes;
  cpvec_t cpvec_table[CPVEC_NR];

  /* Dismember the command message. */
  src_proc = m_ptr->VCP_SRC_PROC;
  dst_proc = m_ptr->VCP_DST_PROC;
  vect_s = m_ptr->VCP_VEC_SIZE;
  vect_addr = (vir_bytes)m_ptr->VCP_VEC_ADDR;

  if (vect_s > CPVEC_NR) return EDOM;

  src_phys= numap_local(m_ptr->m_source, vect_addr, vect_s * sizeof(cpvec_t));
  if (!src_phys) return EFAULT;
  phys_copy(src_phys, vir2phys(cpvec_table),
				(phys_bytes) (vect_s * sizeof(cpvec_t)));

  for (i = 0; i < vect_s; i++) {
	src_vir= cpvec_table[i].cpv_src;
	dst_vir= cpvec_table[i].cpv_dst;
	bytes= cpvec_table[i].cpv_size;
	src_phys = numap_local(src_proc,src_vir,(vir_bytes)bytes);
	dst_phys = numap_local(dst_proc,dst_vir,(vir_bytes)bytes);
	if (src_phys == 0 || dst_phys == 0) return(EFAULT);
	phys_copy(src_phys, dst_phys, bytes);
  }
  return(OK);
}


