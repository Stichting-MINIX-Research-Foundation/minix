/* The kernel call implemented in this file:
 *   m_type:	SYS_EXIT
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT		(slot number of exiting process)
 */

#include "../system.h"

#include <minix/endpoint.h>

#if USE_EXIT

FORWARD _PROTOTYPE( void clear_proc, (register struct proc *rc));

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC int do_exit(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_exit. A user process has exited or a system process requests 
 * to exit. Only the PM can request other process slots to be cleared.
 * The routine to clean up a process table slot cancels outstanding timers, 
 * possibly removes the process from the message queues, and resets certain 
 * process table fields to the default values.
 */
  int exit_e;				

  /* Determine what process exited. User processes are handled here. */
  if (PM_PROC_NR == who_p) {
      if (m_ptr->PR_ENDPT != SELF) { 		/* PM tries to exit self */
          if(!isokendpt(m_ptr->PR_ENDPT, &exit_e)) /* get exiting process */
	     return EINVAL;
          clear_proc(proc_addr(exit_e));	/* exit a user process */
          return(OK);				/* report back to PM */
      }
  } 

  /* The PM or some other system process requested to be exited. */
  clear_proc(proc_addr(who_p));
  return(EDONTREPLY);
}

/*===========================================================================*
 *			         clear_proc				     *
 *===========================================================================*/
PRIVATE void clear_proc(rc)
register struct proc *rc;		/* slot of process to clean up */
{
  register struct proc *rp;		/* iterate over process table */
  register struct proc **xpp;		/* iterate over caller queue */
  int i;
  int sys_id;
  char saved_rts_flags;

  /* Don't clear if already cleared. */
  if(isemptyp(rc)) return;

  /* Remove the process' ability to send and receive messages */
  clear_endpoint(rc);

  /* Turn off any alarm timers at the clock. */   
  reset_timer(&priv(rc)->s_alarm_timer);

  /* Make sure that the exiting process is no longer scheduled. */
  if (rc->p_rts_flags == 0) lock_dequeue(rc);

  /* Check the table with IRQ hooks to see if hooks should be released. */
  for (i=0; i < NR_IRQ_HOOKS; i++) {
      int proc;
      if (rc->p_endpoint == irq_hooks[i].proc_nr_e) { 
        rm_irq_handler(&irq_hooks[i]);	/* remove interrupt handler */
        irq_hooks[i].proc_nr_e = NONE;	/* mark hook as free */
      }
  }

  /* Release the process table slot. If this is a system process, also
   * release its privilege structure.  Further cleanup is not needed at
   * this point. All important fields are reinitialized when the 
   * slots are assigned to another, new process. 
   */
  saved_rts_flags = rc->p_rts_flags;
  rc->p_rts_flags = SLOT_FREE;		
  if (priv(rc)->s_flags & SYS_PROC) priv(rc)->s_proc_nr = NONE;

  /* Clean up virtual memory */
  if (rc->p_misc_flags & MF_VM)
  	vm_map_default(rc);
}

#endif /* USE_EXIT */

