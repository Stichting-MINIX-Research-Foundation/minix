/* The kernel call that is implemented in this file:
 *   m_type:	SYS_SIGSEND
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process to call signal handler
 *     m2_p1:	SIG_CTXT_PTR 	# pointer to sigcontext structure
 *     m2_i3:	SIG_FLAGS    	# flags for S_SIGRETURN call	
 *
 */

#include "../system.h"
#include "../vm.h"
#include <signal.h>
#include <string.h>
#include <sys/sigcontext.h>

#if USE_SIGSEND

/*===========================================================================*
 *			      do_sigsend				     *
 *===========================================================================*/
PUBLIC int do_sigsend(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_sigsend, POSIX-style signal handling. */

  struct sigmsg smsg;
  register struct proc *rp;
  struct sigcontext sc, *scp;
  struct sigframe fr, *frp;
  int proc_nr, r;

  if (!isokendpt(m_ptr->SIG_ENDPT, &proc_nr)) return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);
  rp = proc_addr(proc_nr);

  /* Get the sigmsg structure into our address space.  */
  if((r=data_copy_vmcheck(who_e, (vir_bytes) m_ptr->SIG_CTXT_PTR,
	SYSTEM, (vir_bytes) &smsg, (phys_bytes) sizeof(struct sigmsg))) != OK)
	return r;

  /* Compute the user stack pointer where sigcontext will be stored. */
  scp = (struct sigcontext *) smsg.sm_stkptr - 1;

  /* Copy the registers to the sigcontext structure. */
  memcpy(&sc.sc_regs, (char *) &rp->p_reg, sizeof(struct sigregs));

  /* Finish the sigcontext initialization. */
  sc.sc_flags = 0;	/* unused at this time */
  sc.sc_mask = smsg.sm_mask;

  /* Copy the sigcontext structure to the user's stack. */
  if((r=data_copy_vmcheck(SYSTEM, (vir_bytes) &sc, m_ptr->SIG_ENDPT,
	(vir_bytes) scp, (vir_bytes) sizeof(struct sigcontext))) != OK)
      return r;

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
  if((r=data_copy_vmcheck(SYSTEM, (vir_bytes) &fr,
	m_ptr->SIG_ENDPT, (vir_bytes) frp, 
      (vir_bytes) sizeof(struct sigframe))) != OK)
      return r;

  /* Reset user registers to execute the signal handler. */
  rp->p_reg.sp = (reg_t) frp;
  rp->p_reg.pc = (reg_t) smsg.sm_sighandler;

  if(!RTS_ISSET(rp, PROC_STOP)) {
	struct proc *caller;
	caller = proc_addr(who_p);
	kprintf("system: warning: sigsend a running process\n");
	kprintf("caller stack: ");
	proc_stacktrace(caller);
  }

  return(OK);
}

#endif /* USE_SIGSEND */

