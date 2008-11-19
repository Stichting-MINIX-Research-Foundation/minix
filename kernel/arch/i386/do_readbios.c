/* The kernel call implemented in this file:
 *   m_type:	SYS_READBIOS
 *
 * The parameters for this kernel call are:
 *    m2_i1:	RDB_SIZE		number of bytes to copy
 *    m2_l1:	RDB_ADDR		absolute address in BIOS area
 *    m2_p1:	RDB_BUF			buffer address in requesting process
 */

#include "../../system.h"
#include <minix/type.h>

/*===========================================================================*
 *				do_readbios				     *
 *===========================================================================*/
PUBLIC int do_readbios(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  struct vir_addr src, dst;     
        
  src.segment = BIOS_SEG;
  dst.segment = D;
  src.offset = m_ptr->RDB_ADDR;
  dst.offset = (vir_bytes) m_ptr->RDB_BUF;
  src.proc_nr_e = NONE;
  dst.proc_nr_e = m_ptr->m_source;      

  return virtual_copy_vmcheck(&src, &dst, m_ptr->RDB_SIZE);
}
