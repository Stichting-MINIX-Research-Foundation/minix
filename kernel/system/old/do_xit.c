/*===========================================================================*
 *				do_xit					     *
 *===========================================================================*/
PUBLIC int do_xit(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_exit. A user process has exited (the MM sent the request).
 */
  register struct proc *rp, *rc;
  struct proc *np, *xp;
  int exit_proc_nr;				

  /* Get a pointer to the process that exited. */
  exit_proc_nr = m_ptr->PR_PROC_NR;	
  if (exit_proc_nr == SELF) exit_proc_nr = m_ptr->m_source;
  if (! isokprocn(exit_proc_nr)) return(EINVAL);
  rc = proc_addr(exit_proc_nr);

  /* If this is a user process and the MM passed in a valid parent process, 
   * accumulate the child times at the parent. 
   */
  if (isuserp(rc) && isokprocn(m_ptr->PR_PPROC_NR)) {
      rp = proc_addr(m_ptr->PR_PPROC_NR);
      lock();
      rp->child_utime += rc->user_time + rc->child_utime;
      rp->child_stime += rc->sys_time + rc->child_stime;
      unlock();
  }

  /* Now call the routine to clean up of the process table slot. This cancels
   * outstanding timers, possibly removes the process from the message queues,
   * and resets important process table fields.
   */
  clear_proc(exit_proc_nr);
  return(OK);				/* tell MM that cleanup succeeded */
}


