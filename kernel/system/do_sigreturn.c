/* The kernel call that is implemented in this file:
 *   m_type:	SYS_SIGRETURN
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process returning from handler
 *     m2_p1:	SIG_CTXT_PTR 	# pointer to sigcontext structure
 *
 */

#include "../system.h"
#include <string.h>
#include <signal.h>
#include <ibm/cpu.h>
#include <sys/sigcontext.h>

#if USE_SIGRETURN 

/*===========================================================================*
 *			      do_sigreturn				     *
 *===========================================================================*/
PUBLIC int do_sigreturn(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* POSIX style signals require sys_sigreturn to put things in order before 
 * the signalled process can resume execution
 */
  struct sigcontext sc;
  register struct proc *rp;
  int proc, r;

  if (! isokendpt(m_ptr->SIG_ENDPT, &proc)) return(EINVAL);
  if (iskerneln(proc)) return(EPERM);
  rp = proc_addr(proc);

  /* Copy in the sigcontext structure. */
  if((r=data_copy(m_ptr->SIG_ENDPT, (vir_bytes) m_ptr->SIG_CTXT_PTR,
	SYSTEM, (vir_bytes) &sc, sizeof(struct sigcontext))) != OK)
	return r;

  /* Restore user bits of psw from sc, maintain system bits from proc. */
  sc.sc_psw  =  (sc.sc_psw & X86_FLAGS_USER) |
                (rp->p_reg.psw & ~X86_FLAGS_USER);

#if (_MINIX_CHIP == _CHIP_INTEL)
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
#if _MINIX_CHIP == _CHIP_POWERPC
  memcpy(&rp->p_reg, &sc.sc_regs, sizeof(struct stackframe_s));
#else
  memcpy(&rp->p_reg, &sc.sc_regs, sizeof(struct sigregs));
#endif
  return(OK);
}
#endif /* USE_SIGRETURN */

