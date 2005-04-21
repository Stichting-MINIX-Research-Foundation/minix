/* The system call implemented in this file:
 *   m_type:	SYS_VIRCOPY
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

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *				do_vircopy					     *
 *===========================================================================*/
PUBLIC int do_vircopy(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_vircopy().  Copy data by using virtual addressing. */
  struct vir_addr vir_addr[2];	/* virtual source and destination address */
  vir_bytes bytes;		/* number of bytes to copy */
  int i;

  /* Dismember the command message. */
  vir_addr[_SRC_].proc_nr = m_ptr->CP_SRC_PROC_NR;
  vir_addr[_SRC_].segment = m_ptr->CP_SRC_SPACE;
  vir_addr[_SRC_].offset = (vir_bytes) m_ptr->CP_SRC_ADDR;
  vir_addr[_DST_].proc_nr = m_ptr->CP_DST_PROC_NR;
  vir_addr[_DST_].segment = m_ptr->CP_DST_SPACE;
  vir_addr[_DST_].offset = (vir_bytes) m_ptr->CP_DST_ADDR;
  bytes = (phys_bytes) m_ptr->CP_NR_BYTES;

  /* Now do some checks for both the source and destination virtual address.
   * This is done once for _SRC_, then once for _DST_. 
   */
  for (i=_SRC_; i<=_DST_; i++) {

      /* Check if process number was given implictly with SELF and is valid. */
      if (vir_addr[i].proc_nr == SELF) vir_addr[i].proc_nr = m_ptr->m_source;
      if (! isokprocn(vir_addr[i].proc_nr)) {
          kprintf("do_vircopy: illegal proc nr\n",NO_ARG);
          return(EINVAL); 
      }

      /* Copying from or to special segments can only done by the owner. */
      if ((vir_addr[i].segment & SEGMENT_TYPE) != LOCAL_SEG &&
          vir_addr[i].proc_nr != m_ptr->m_source) {
          kprintf("do_vircopy: special seg permission denied\n", NO_ARG);
          return(EPERM); 
      }
  }

  /* Check for overflow. This would happen for 64K segments and 16-bit 
   * vir_bytes. Especially copying by the MM on do_fork() is affected. 
   */
  if (bytes != (vir_bytes) bytes) {
	kprintf("do_vircopy: overflow\n", NO_ARG);
	return(E2BIG);
  }

  /* Now try to make the actual virtual copy. */
  return( virtual_copy(&vir_addr[_SRC_], &vir_addr[_DST_], bytes) );
}


