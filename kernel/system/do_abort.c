/* The system call implemented in this file:
 *   m_type:	SYS_ABORT
 *
 * The parameters for this system call are:
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
 * PM (normal abort or panic) or FS (panic), or TTY (user issued CTRL-ALT-DEL 
 * or ESC after debugging dumps).
 */
  int how = m_ptr->ABRT_HOW;
  timer_t *tp;

  /* See if the monitor is to run the specified instructions. */
  if (how == RBT_MONITOR) {
      int proc_nr = m_ptr->ABRT_MON_PROC;
      int length = m_ptr->ABRT_MON_LEN + 1;
      vir_bytes src_vir = (vir_bytes) m_ptr->ABRT_MON_ADDR;
      phys_bytes src_phys = numap_local(proc_nr, src_vir, length);

      /* Validate length and address of shutdown code before copying. */
      if (length > kinfo.params_size || src_phys == 0)
          phys_copy(vir2phys("delay;boot"), kinfo.params_base, 11);
      else
          phys_copy(src_phys, kinfo.params_base, (phys_bytes) length);
  }

  /* Set a watchdog timer to shut down, so that this call returns first.
   * The timer will expire at the next clock tick, which can be any moment.
   * The CLOCK task is only scheduled when the SYSTEM task is done, though.
   */
  tp = &priv(proc_addr(KERNEL))->s_alarm_timer;
  tmr_arg(tp)->ta_int = how;		/* pass status as timer argument */
  set_timer(tp, get_uptime(), prepare_shutdown);
  return(OK);				/* pro-forma (really EDISASTER) */
}

#endif /* USE_ABORT */

