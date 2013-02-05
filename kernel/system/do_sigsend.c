/* The kernel call that is implemented in this file:
 *   m_type:	SYS_SIGSEND
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process to call signal handler
 *     m2_p1:	SIG_CTXT_PTR 	# pointer to sigcontext structure
 *     m2_i3:	SIG_FLAGS    	# flags for S_SIGRETURN call	
 *
 */

#include "kernel/system.h"
#include <signal.h>
#include <string.h>

#if USE_SIGSEND

/*===========================================================================*
 *			      do_sigsend				     *
 *===========================================================================*/
int do_sigsend(struct proc * caller, message * m_ptr)
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
  if((r=data_copy_vmcheck(caller, caller->p_endpoint,
		(vir_bytes) m_ptr->SIG_CTXT_PTR, KERNEL, (vir_bytes) &smsg,
		(phys_bytes) sizeof(struct sigmsg))) != OK)
	return r;

  /* Compute the user stack pointer where sigcontext will be stored. */
  smsg.sm_stkptr = arch_get_sp(rp);
  scp = (struct sigcontext *) smsg.sm_stkptr - 1;

  /* Copy the registers to the sigcontext structure. */
  memcpy(&sc.sc_regs, (char *) &rp->p_reg, sizeof(sigregs));

#if defined(__i386__)
  sc.trap_style = rp->p_seg.p_kern_trap_style;

  if(sc.trap_style == KTS_NONE) {
  	printf("do_sigsend: sigsend an unsaved process\n");
	return EINVAL;
  }

    if(proc_used_fpu(rp)) {
	    /* save the FPU context before saving it to the sig context */
	    save_fpu(rp);
	    memcpy(&sc.sc_fpu_state, rp->p_seg.fpu_state, FPU_XFP_SIZE);
    }
#endif

  /* Finish the sigcontext initialization. */
  sc.sc_mask = smsg.sm_mask;
  sc.sc_flags = rp->p_misc_flags & MF_FPU_INITIALIZED;

  /* Copy the sigcontext structure to the user's stack. */
  if((r=data_copy_vmcheck(caller, KERNEL, (vir_bytes) &sc, m_ptr->SIG_ENDPT,
	(vir_bytes) scp, (vir_bytes) sizeof(struct sigcontext))) != OK)
      return r;

  /* Initialize the sigframe structure. */
  frp = (struct sigframe *) scp - 1;
  fr.sf_scpcopy = scp;
  fr.sf_retadr2= (void (*)()) rp->p_reg.pc;
  fr.sf_fp = rp->p_reg.fp;
  rp->p_reg.fp = (reg_t) &frp->sf_fp;
  fr.sf_scp = scp;

  fpu_sigcontext(rp, &fr, &sc);

  fr.sf_signo = smsg.sm_signo;
  fr.sf_retadr = (void (*)()) smsg.sm_sigreturn;

#if defined(__arm__)
  /* use the ARM link register to set the return address from the signal
   * handler
   */
  rp->p_reg.lr = (reg_t) fr.sf_retadr;
  if(rp->p_reg.lr & 1) { printf("sigsend: LSB LR makes no sense.\n"); }

  /* pass signal handler parameters in registers */
  rp->p_reg.retreg = (reg_t) fr.sf_signo;
  rp->p_reg.r1 = (reg_t) fr.sf_code;
  rp->p_reg.r2 = (reg_t) fr.sf_scp;
  rp->p_misc_flags |= MF_CONTEXT_SET;
#endif

  /* Copy the sigframe structure to the user's stack. */
  if((r=data_copy_vmcheck(caller, KERNEL, (vir_bytes) &fr,
	m_ptr->SIG_ENDPT, (vir_bytes) frp, 
      (vir_bytes) sizeof(struct sigframe))) != OK)
      return r;

  /* Reset user registers to execute the signal handler. */
  rp->p_reg.sp = (reg_t) frp;
  rp->p_reg.pc = (reg_t) smsg.sm_sighandler;

  /* Signal handler should get clean FPU. */
  rp->p_misc_flags &= ~MF_FPU_INITIALIZED;

  if(!RTS_ISSET(rp, RTS_PROC_STOP)) {
	printf("system: warning: sigsend a running process\n");
	printf("caller stack: ");
	proc_stacktrace(caller);
  }

  return(OK);
}

#endif /* USE_SIGSEND */

