/* The kernel call that is implemented in this file:
 *   m_type:	SYS_SIGRETURN
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process returning from handler
 *     m2_p1:	SIG_CTXT_PTR 	# pointer to sigcontext structure
 *
 */

#include "kernel/system.h"
#include <string.h>
#include <machine/cpu.h>

#if USE_SIGRETURN 

/*===========================================================================*
 *			      do_sigreturn				     *
 *===========================================================================*/
int do_sigreturn(struct proc * caller, message * m_ptr)
{
/* POSIX style signals require sys_sigreturn to put things in order before 
 * the signalled process can resume execution
 */
  struct sigcontext sc;
  register struct proc *rp;
  int proc_nr, r;

  if (! isokendpt(m_ptr->SIG_ENDPT, &proc_nr)) return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);
  rp = proc_addr(proc_nr);

  /* Copy in the sigcontext structure. */
  if((r=data_copy(m_ptr->SIG_ENDPT, (vir_bytes) m_ptr->SIG_CTXT_PTR,
	KERNEL, (vir_bytes) &sc, sizeof(struct sigcontext))) != OK)
	return r;

#if defined(__i386__)
  /* Restore user bits of psw from sc, maintain system bits from proc. */
  sc.sc_psw  =  (sc.sc_psw & X86_FLAGS_USER) |
                (rp->p_reg.psw & ~X86_FLAGS_USER);
#endif

#if defined(__i386__)
  /* Don't panic kernel if user gave bad selectors. */
  sc.sc_cs = rp->p_reg.cs;
  sc.sc_ds = rp->p_reg.ds;
  sc.sc_es = rp->p_reg.es;
  sc.sc_ss = rp->p_reg.ss;
  sc.sc_fs = rp->p_reg.fs;
  sc.sc_gs = rp->p_reg.gs;
#endif

  /* Restore the registers. */
  arch_proc_setcontext(rp, &sc.sc_regs, 1, sc.trap_style);
#if defined(__i386__)
  if(sc.sc_flags & MF_FPU_INITIALIZED)
  {
	memcpy(rp->p_seg.fpu_state, &sc.sc_fpu_state, FPU_XFP_SIZE);
	rp->p_misc_flags |=  MF_FPU_INITIALIZED; /* Restore math usage flag. */
	/* force reloading FPU */
	release_fpu(rp);
  }
#endif

  return(OK);
}
#endif /* USE_SIGRETURN */

