/* The system call implemented in this file:
 *   m_type:	SYS_ABORT
 *
 * The parameters for this system call are:
 *    m1_i1:	ABRT_HOW 	(how to abort, possibly fetch monitor params)	
 *    m1_i2:	ABRT_MON_PROC 	(proc nr to get monitor params from)	
 *    m1_i3:	ABRT_MON_LEN	(length of monitor params)
 *    m1_p1:	ABRT_MON_ADDR 	(virtual address of params)	
 */

#include "../kernel.h"
#include "../system.h"
#include <unistd.h>
INIT_ASSERT

/*===========================================================================*
 *				do_abort				     *
 *===========================================================================*/
PUBLIC int do_abort(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_abort. MINIX is unable to continue. This can originate in the
 * MM (normal abort or panic) or FS (panic), or TTY (a CTRL-ALT-DEL or ESC
 * after debugging dumps).
 */
  register struct proc *rp;
  phys_bytes src_phys;
  vir_bytes len;
  int how = m_ptr->ABRT_HOW;
  
  rp = proc_addr(m_ptr->m_source);

  if (how == RBT_MONITOR) {
	/* The monitor is to run user specified instructions. */
	len = m_ptr->ABRT_MON_LEN + 1;
	assert(len <= mon_parmsize);
	src_phys = numap_local(m_ptr->ABRT_MON_PROC, 
		(vir_bytes) m_ptr->ABRT_MON_ADDR, len);
	assert(src_phys != 0);
	phys_copy(src_phys, mon_params, (phys_bytes) len);
  }
  prepare_shutdown(how);
  return(OK);				/* pro-forma (really EDISASTER) */
}

