/* The kernel call implemented in this file:
 *   m_type:	SYS_READBIOS
 *
 * The parameters for this kernel call are:
 *	m_lsys_krn_readbios.size	number of bytes to copy
 *	m_lsys_krn_readbios.addr	absolute address in BIOS area
 *	m_lsys_krn_readbios.buf		buffer address in requesting process
 */

#include "kernel/system.h"

// Added kernel headers
#include <minix/kernel_types.h> // For k_size_t, k_errno_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *				do_readbios				     *
 *===========================================================================*/
int do_readbios(struct proc * caller, message * m_ptr)
{
  struct vir_addr src, dst;
  k_size_t len = (k_size_t)m_ptr->m_lsys_krn_readbios.size; // MODIFIED size_t and cast
  vir_bytes limit;

  src.offset = m_ptr->m_lsys_krn_readbios.addr;
  dst.offset = m_ptr->m_lsys_krn_readbios.buf;
  src.proc_nr_e = NONE;
  dst.proc_nr_e = m_ptr->m_source;      

  limit = src.offset + len - 1;

#define VINRANGE(v, a, b) ((a) <= (v) && (v) <= (b))
#define SUBRANGE(a,b,c,d) (VINRANGE((a), (c), (d)) && VINRANGE((b),(c),(d)))
#define USERRANGE(a, b) SUBRANGE(src.offset, limit, (a), (b))

  if(!USERRANGE(BIOS_MEM_BEGIN, BIOS_MEM_END) &&
     !USERRANGE(BASE_MEM_TOP, UPPER_MEM_END))
	return EPERM; // EPERM might be undefined

  // The last argument to virtual_copy_vmcheck is size, should be k_size_t
  return virtual_copy_vmcheck(caller, &src, &dst, (k_size_t)m_ptr->m_lsys_krn_readbios.size); // MODIFIED cast
}
