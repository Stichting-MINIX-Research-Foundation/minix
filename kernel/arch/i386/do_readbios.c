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
  int proc_nr;
  struct proc *p;
  phys_bytes address, phys_buf, phys_bios;
  vir_bytes buf;
  size_t size;

  address = m_ptr->RDB_ADDR;
  buf = (vir_bytes)m_ptr->RDB_BUF;
  size = m_ptr->RDB_SIZE;

  okendpt(m_ptr->m_source, &proc_nr);
  p = proc_addr(proc_nr);
  phys_buf = umap_local(p, D, buf, size);
  if (phys_buf == 0)
	return EFAULT;
  phys_bios = umap_bios(p, address, size);
  if (phys_bios == 0)
	return EPERM;
  phys_copy(phys_bios, phys_buf, size);
  return 0;
}
