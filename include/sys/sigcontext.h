#ifndef _SIGCONTEXT_H
#define _SIGCONTEXT_H

/* The sigcontext structure is used by the sigreturn(2) system call.
 * sigreturn() is seldom called by user programs, but it is used internally
 * by the signal catching mechanism.
 */

#ifndef _ANSI_H
#include <ansi.h>
#endif

#ifndef _MINIX_SYS_CONFIG_H
#include <minix/sys_config.h>
#endif

#if !defined(_MINIX_CHIP)
#include "error, configuration is not known"
#endif

/* The following structure should match the stackframe_s structure used
 * by the kernel's context switching code.  Floating point registers should
 * be added in a different struct.
 */
#if (_MINIX_CHIP == _CHIP_INTEL)
#include <sys/stackframe.h>
#include <sys/fpu.h>

typedef struct stackframe_s sigregs;
struct sigframe {		/* stack frame created for signalled process */
  _PROTOTYPE( void (*sf_retadr), (void) );
  int sf_signo;
  int sf_code;
  struct sigcontext *sf_scp;
  int sf_fp;
  _PROTOTYPE( void (*sf_retadr2), (void) );
  struct sigcontext *sf_scpcopy;
};

#else
#include "error, _MINIX_CHIP is not supported"
#endif /* _MINIX_CHIP == _CHIP_INTEL */

struct sigcontext {
  int sc_flags;			/* sigstack state to restore (including
				 * MF_FPU_INITIALIZED)
				 */
  long sc_mask;			/* signal mask to restore */
  sigregs sc_regs;              /* register set to restore */
#if (_MINIX_CHIP == _CHIP_INTEL)
  union fpu_state_u sc_fpu_state;
#endif
};

#if (_MINIX_CHIP == _CHIP_INTEL)
#if _WORD_SIZE == 4
#define sc_gs sc_regs.gs
#define sc_fs sc_regs.fs
#endif /* _WORD_SIZE == 4 */
#define sc_es sc_regs.es
#define sc_ds sc_regs.ds
#define sc_di sc_regs.di
#define sc_si sc_regs.si
#define sc_fp sc_regs.bp
#define sc_st sc_regs.st		/* stack top -- used in kernel */
#define sc_bx sc_regs.bx
#define sc_dx sc_regs.dx
#define sc_cx sc_regs.cx
#define sc_retreg sc_regs.retreg
#define sc_retadr sc_regs.retadr	/* return address to caller of
					save -- used in kernel */
#define sc_pc sc_regs.pc
#define sc_cs sc_regs.cs
#define sc_psw sc_regs.psw
#define sc_sp sc_regs.sp
#define sc_ss sc_regs.ss
#endif /* _MINIX_CHIP == _CHIP_INTEL */

_PROTOTYPE( int sigreturn, (struct sigcontext *_scp)			);

#endif /* _SIGCONTEXT_H */
