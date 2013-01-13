#ifndef _ARM_SIGNAL_H_
#define _ARM_SIGNAL_H_

#include <sys/featuretest.h>

typedef int sig_atomic_t;

/* The following structure should match the stackframe_s structure used
 * by the kernel's context switching code.  Floating point registers should
 * be added in a different struct.
 */

#include <machine/stackframe.h>

typedef struct stackframe_s sigregs;
struct sigframe {		/* stack frame created for signalled process */
  void (*sf_retadr)(void);
  int sf_signo;
  int sf_code;
  struct sigcontext *sf_scp;
  int sf_fp;
  void (*sf_retadr2)(void);
  struct sigcontext *sf_scpcopy;
};

struct sigcontext {
  int trap_style;		/* how should context be restored? KTS_* */
  int sc_flags;			/* sigstack state to restore (including
				 * MF_FPU_INITIALIZED)
				 */
  long sc_mask;			/* signal mask to restore */
  sigregs sc_regs;              /* register set to restore */
};

#define sc_retreg sc_regs.retreg
#define sc_r1 sc_regs.r1
#define sc_r2 sc_regs.r2
#define sc_r3 sc_regs.r3
#define sc_r4 sc_regs.r4
#define sc_r5 sc_regs.r5
#define sc_r6 sc_regs.r6
#define sc_r7 sc_regs.r7
#define sc_r8 sc_regs.r8
#define sc_r9 sc_regs.r9
#define sc_r10 sc_regs.r10
#define sc_fp sc_regs.fp
#define sc_r12 sc_regs.r12
#define sc_sp sc_regs.sp
#define sc_lr sc_regs.lr
#define sc_pc sc_regs.pc
#define sc_psr sc_regs.psr

#ifdef _MINIX
__BEGIN_DECLS
int sigreturn(struct sigcontext *_scp);
__END_DECLS
#endif /* _MINIX */

#endif	/* !_ARM_SIGNAL_H_ */
