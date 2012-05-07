/* The kernel call implemented in this file:
 *   m_type:	SYS_READBIOS
 *
 * The parameters for this kernel call are:
 *    m2_i1:	RDB_SIZE		number of bytes to copy
 *    m2_l1:	RDB_ADDR		absolute address in BIOS area
 *    m2_p1:	RDB_BUF			buffer address in requesting process
 */

#include "kernel/system.h"
#include <minix/type.h>

/*===========================================================================*
 *				do_readbios				     *
 *===========================================================================*/
int do_readbios(struct proc * caller, message * m_ptr)
{
  struct vir_addr src, dst;
  vir_bytes len = m_ptr->RDB_SIZE, limit;

  src.offset = m_ptr->RDB_ADDR;
  dst.offset = (vir_bytes) m_ptr->RDB_BUF;
  src.proc_nr_e = NONE;
  dst.proc_nr_e = m_ptr->m_source;      

  limit = src.offset + len - 1;

#define VINRANGE(v, a, b) ((a) <= (v) && (v) <= (b))
#define SUBRANGE(a,b,c,d) (VINRANGE((a), (c), (d)) && VINRANGE((b),(c),(d)))
#define USERRANGE(a, b) SUBRANGE(src.offset, limit, (a), (b))

  if(!USERRANGE(BIOS_MEM_BEGIN, BIOS_MEM_END) &&
     !USERRANGE(BASE_MEM_TOP, UPPER_MEM_END))
  	return EPERM;

  return virtual_copy_vmcheck(caller, &src, &dst, m_ptr->RDB_SIZE);
}
