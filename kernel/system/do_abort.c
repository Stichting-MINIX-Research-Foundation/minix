/* The kernel call implemented in this file:
 *   m_type:	SYS_ABORT
 *
 * The parameters for this kernel call are:
 *    m1_i1:	ABRT_HOW 	(how to abort, possibly fetch monitor params)	
 *    m1_i2:	ABRT_MON_PROC 	(proc nr to get monitor params from)	
 *    m1_i3:	ABRT_MON_LEN	(length of monitor params)
 *    m1_p1:	ABRT_MON_ADDR 	(virtual address of params)	
 */

#include "../system.h"
#include <unistd.h>

#if USE_ABORT

/*===========================================================================*
 *				do_abort				     *
 *===========================================================================*/
PUBLIC int do_abort(m_ptr)
message *m_ptr;			/* pointer to request message */
{
  /* Handle sys_abort. MINIX is unable to continue. This can originate in the
   * PM (normal abort or panic) or TTY (after CTRL-ALT-DEL). 
   */
  int how = m_ptr->ABRT_HOW;
  int proc_nr;
  int length;
  phys_bytes src_phys;

  /* See if the monitor is to run the specified instructions. */
  if (how == RBT_MONITOR) {

      proc_nr = m_ptr->ABRT_MON_PROC;
      if (! isokprocn(proc_nr)) return(EINVAL);
      length = m_ptr->ABRT_MON_LEN + 1;
      if (length > kinfo.params_size) return(E2BIG);
      src_phys = numap_local(proc_nr,(vir_bytes)m_ptr->ABRT_MON_ADDR,length);
      if (! src_phys) return(EFAULT);

      /* Parameters seem ok, copy them and prepare shutting down. */
      phys_copy(src_phys, kinfo.params_base, (phys_bytes) length);
  }

  /* Now prepare to shutdown MINIX. */
  prepare_shutdown(how);
  return(OK);				/* pro-forma (really EDISASTER) */
}

#endif /* USE_ABORT */

