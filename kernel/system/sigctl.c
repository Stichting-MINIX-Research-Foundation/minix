/* The system call that is implemented in this file:
 *     SYS_SIGCTL	# signal handling functionality 
 *
 * The parameters and types for this system call are:
 *     SIG_REQUEST 	# request to perform			(long)
 *     SIG_PROC  	# process to signal/ pending		(int)
 *     SIG_CTXT_PTR 	# pointer to sigcontext structure	(pointer)	
 *     SIG_FLAGS    	# flags for S_SIGRETURN call		(int)	
 *     SIG_MAP		# bit map with pending signals		(long)	
 *     SIG_NUMBER	# signal number to send to process	(int)	
 *
 * Supported request types are in the parameter SIG_REQUEST:
 *     S_GETSIG		# get a pending kernel signal
 *     S_ENDSIG		# signal has been processed 
 *     S_SENDSIG	# deliver a POSIX-style signal 
 *     S_SIGRETURN	# return from a POSIX-style signal 
 *     S_KILL		# send a signal to a process 
 */

#include "../kernel.h"
#include "../system.h"
#include <signal.h>
#include <sys/sigcontext.h>

/* PM is ready to accept signals and repeatedly does a system call to get 
 * one. Find a process with pending signals. If no signals are available, 
 * return NONE in the process number field.
 */
PUBLIC int do_getsig(m_ptr)
message *m_ptr;			/* pointer to request message */
{
  register struct proc *rp;

  /* Find the next process with pending signals. */
  for (rp = BEG_USER_ADDR; rp < END_PROC_ADDR; rp++) {
      if (rp->p_rts_flags & SIGNALED) {
          m_ptr->SIG_PROC = rp->p_nr;
          m_ptr->SIG_MAP = rp->p_pending;
          sigemptyset(&rp->p_pending); 	/* ball is in PM's court */
          rp->p_rts_flags &= ~SIGNALED;	/* blocked by SIG_PENDING */
          return(OK);
      }
  }

  /* No process with pending signals was found. */
  m_ptr->SIG_PROC = NONE; 
  return(OK);
}

PUBLIC int do_endsig(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Finish up after a kernel type signal, caused by a SYS_KILL message or a 
 * call to cause_sig by a task. This is called by the PM after processing a
 * signal it got with SYS_GETSIG.
 */
  register struct proc *rp;

  rp = proc_addr(m_ptr->SIG_PROC);
  if (isemptyp(rp)) return(EINVAL);		/* process already dead? */

  /* PM has finished one kernel signal. Perhaps process is ready now? */
  if (! (rp->p_rts_flags & SIGNALED)) 		/* new signal arrived */
     if ((rp->p_rts_flags &= ~SIG_PENDING)==0)	/* remove pending flag */
         lock_ready(rp);			/* ready if no flags */
  return(OK);
}

PUBLIC int do_sigsend(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_sigsend, POSIX-style signal handling. */

  struct sigmsg smsg;
  register struct proc *rp;
  phys_bytes src_phys, dst_phys;
  struct sigcontext sc, *scp;
  struct sigframe fr, *frp;

  rp = proc_addr(m_ptr->SIG_PROC);

  /* Get the sigmsg structure into our address space.  */
  src_phys = umap_local(proc_addr(PM_PROC_NR), D, (vir_bytes) 
      m_ptr->SIG_CTXT_PTR, (vir_bytes) sizeof(struct sigmsg));
  if (src_phys == 0) return(EFAULT);
  phys_copy(src_phys,vir2phys(&smsg),(phys_bytes) sizeof(struct sigmsg));

  /* Compute the user stack pointer where sigcontext will be stored. */
  scp = (struct sigcontext *) smsg.sm_stkptr - 1;

  /* Copy the registers to the sigcontext structure. */
  kmemcpy(&sc.sc_regs, &rp->p_reg, sizeof(struct sigregs));

  /* Finish the sigcontext initialization. */
  sc.sc_flags = SC_SIGCONTEXT;
  sc.sc_mask = smsg.sm_mask;

  /* Copy the sigcontext structure to the user's stack. */
  dst_phys = umap_local(rp, D, (vir_bytes) scp,
      (vir_bytes) sizeof(struct sigcontext));
  if (dst_phys == 0) return(EFAULT);
  phys_copy(vir2phys(&sc), dst_phys, (phys_bytes) sizeof(struct sigcontext));

  /* Initialize the sigframe structure. */
  frp = (struct sigframe *) scp - 1;
  fr.sf_scpcopy = scp;
  fr.sf_retadr2= (void (*)()) rp->p_reg.pc;
  fr.sf_fp = rp->p_reg.fp;
  rp->p_reg.fp = (reg_t) &frp->sf_fp;
  fr.sf_scp = scp;
  fr.sf_code = 0;	/* XXX - should be used for type of FP exception */
  fr.sf_signo = smsg.sm_signo;
  fr.sf_retadr = (void (*)()) smsg.sm_sigreturn;

  /* Copy the sigframe structure to the user's stack. */
  dst_phys = umap_local(rp, D, (vir_bytes) frp, 
      (vir_bytes) sizeof(struct sigframe));
  if (dst_phys == 0) return(EFAULT);
  phys_copy(vir2phys(&fr), dst_phys, (phys_bytes) sizeof(struct sigframe));

  /* Reset user registers to execute the signal handler. */
  rp->p_reg.sp = (reg_t) frp;
  rp->p_reg.pc = (reg_t) smsg.sm_sighandler;

  return(OK);
}

PUBLIC int do_sigreturn(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* POSIX style signals require sys_sigreturn to put things in order before 
 * the signalled process can resume execution
 */
  struct sigcontext sc;
  register struct proc *rp;
  phys_bytes src_phys;

  rp = proc_addr(m_ptr->SIG_PROC);

  /* Copy in the sigcontext structure. */
  src_phys = umap_local(rp, D, (vir_bytes) m_ptr->SIG_CTXT_PTR,
      (vir_bytes) sizeof(struct sigcontext));
  if (src_phys == 0) return(EFAULT);
  phys_copy(src_phys, vir2phys(&sc), (phys_bytes) sizeof(struct sigcontext));

  /* Make sure that this is not just a jmp_buf. */
  if ((sc.sc_flags & SC_SIGCONTEXT) == 0) return(EINVAL);

  /* Fix up only certain key registers if the compiler doesn't use
   * register variables within functions containing setjmp.
   */
  if (sc.sc_flags & SC_NOREGLOCALS) {
      rp->p_reg.retreg = sc.sc_retreg;
      rp->p_reg.fp = sc.sc_fp;
      rp->p_reg.pc = sc.sc_pc;
      rp->p_reg.sp = sc.sc_sp;
      return(OK);
  }
  sc.sc_psw  = rp->p_reg.psw;

#if (CHIP == INTEL)
  /* Don't panic kernel if user gave bad selectors. */
  sc.sc_cs = rp->p_reg.cs;
  sc.sc_ds = rp->p_reg.ds;
  sc.sc_es = rp->p_reg.es;
#if _WORD_SIZE == 4
  sc.sc_fs = rp->p_reg.fs;
  sc.sc_gs = rp->p_reg.gs;
#endif
#endif

  /* Restore the registers. */
  kmemcpy(&rp->p_reg, (char *)&sc.sc_regs, sizeof(struct sigregs));
  return(OK);
}

/*===========================================================================*
 *			      do_sigctl					     *
 *===========================================================================*/

PUBLIC int do_kill(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_kill(). Cause a signal to be sent to a process via PM.
 * Note that this has nothing to do with the kill (2) system call, this
 * is how the FS (and possibly other servers) get access to cause_sig. 
 */
  cause_sig(m_ptr->SIG_PROC, m_ptr->SIG_NUMBER);
  return(OK);
}


