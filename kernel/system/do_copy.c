/* The system call implemented in this file:
 *   m_type:	SYS_COPY
 *
 * The parameters for this system call are:
 *    m5_c1:	CP_SRC_SPACE
 *    m5_i1:	CP_SRC_PROC_NR	
 *    m5_l1:	CP_SRC_ADDR	
 *    m5_c2:	CP_DST_SPACE	
 *    m5_i2:	CP_DST_PROC_NR	
 *    m5_l2:	CP_DST_ADDR	
 *    m5_l3:	CP_NR_BYTES
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *				do_copy					     *
 *===========================================================================*/
PUBLIC int do_copy(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_copy().  Copy data by using virtual or physical addressing. */

  int src_proc, dst_proc, src_space, dst_space;
  vir_bytes src_vir, dst_vir;
  phys_bytes src_phys, dst_phys, bytes;

  /* Dismember the command message. */
  src_proc = m_ptr->CP_SRC_PROC_NR;
  dst_proc = m_ptr->CP_DST_PROC_NR;
  src_space = m_ptr->CP_SRC_SPACE;
  dst_space = m_ptr->CP_DST_SPACE;
  src_vir = (vir_bytes) m_ptr->CP_SRC_ADDR;
  dst_vir = (vir_bytes) m_ptr->CP_DST_ADDR;
  bytes = (phys_bytes) m_ptr->CP_NR_BYTES;

  /* Check if process number was given implicitly with SELF. */
  if (src_proc == SELF) src_proc = m_ptr->m_source;
  if (dst_proc == SELF) dst_proc = m_ptr->m_source;

  /* Compute the source and destination addresses and do the copy. */
  if (src_proc == ABS) {
	src_phys = (phys_bytes) m_ptr->CP_SRC_ADDR;
  } else {
	if (bytes != (vir_bytes) bytes) {
		/* This would happen for 64K segments and 16-bit vir_bytes.
		 * It would happen a lot for do_fork except MM uses ABS
		 * copies for that case.
		 */
		panic("overflow in count in do_copy", NO_NUM);
	}
	src_phys = umap_local(proc_addr(src_proc), src_space, src_vir,
			(vir_bytes) bytes);
  }

  if (dst_proc == ABS) {
	dst_phys = (phys_bytes) m_ptr->CP_DST_ADDR;
  } else {
	dst_phys = umap_local(proc_addr(dst_proc), dst_space, dst_vir,
			(vir_bytes) bytes);
  }

  if (src_phys == 0 || dst_phys == 0) return(EFAULT);
  phys_copy(src_phys, dst_phys, bytes);
  return(OK);
}


