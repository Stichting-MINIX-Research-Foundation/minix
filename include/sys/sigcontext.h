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
struct sigregs {  
#if _WORD_SIZE == 4
  short sr_gs;
  short sr_fs;
#endif /* _WORD_SIZE == 4 */
  short sr_es;
  short sr_ds;
  int sr_di;
  int sr_si;
  int sr_bp;
  int sr_st;			/* stack top -- used in kernel */
  int sr_bx;
  int sr_dx;
  int sr_cx;
  int sr_retreg;
  int sr_retadr;		/* return address to caller of save -- used
  				 * in kernel */
  int sr_pc;
  int sr_cs;
  int sr_psw;
  int sr_sp;
  int sr_ss;
};

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
#if (_MINIX_CHIP == _CHIP_M68000)
struct sigregs {  
  long sr_retreg;			/* d0 */
  long sr_d1;
  long sr_d2;
  long sr_d3;
  long sr_d4;
  long sr_d5;
  long sr_d6;
  long sr_d7;
  long sr_a0;
  long sr_a1;
  long sr_a2;
  long sr_a3;
  long sr_a4;
  long sr_a5;
  long sr_a6;
  long sr_sp;			/* also known as a7 */
  long sr_pc;
  short sr_psw;
  short sr_dummy;		/* make size multiple of 4 for system.c */
};
#else
#include "error, _MINIX_CHIP is not supported"
#endif
#endif /* _MINIX_CHIP == _CHIP_INTEL */

struct sigcontext {
  int sc_flags;			/* sigstack state to restore */
  long sc_mask;			/* signal mask to restore */
  struct sigregs sc_regs;	/* register set to restore */
};

#if (_MINIX_CHIP == _CHIP_INTEL)
#if _WORD_SIZE == 4
#define sc_gs sc_regs.sr_gs
#define sc_fs sc_regs.sr_fs
#endif /* _WORD_SIZE == 4 */
#define sc_es sc_regs.sr_es
#define sc_ds sc_regs.sr_ds
#define sc_di sc_regs.sr_di
#define sc_si sc_regs.sr_si 
#define sc_fp sc_regs.sr_bp
#define sc_st sc_regs.sr_st		/* stack top -- used in kernel */
#define sc_bx sc_regs.sr_bx
#define sc_dx sc_regs.sr_dx
#define sc_cx sc_regs.sr_cx
#define sc_retreg sc_regs.sr_retreg
#define sc_retadr sc_regs.sr_retadr	/* return address to caller of 
					save -- used in kernel */
#define sc_pc sc_regs.sr_pc
#define sc_cs sc_regs.sr_cs
#define sc_psw sc_regs.sr_psw
#define sc_sp sc_regs.sr_sp
#define sc_ss sc_regs.sr_ss
#endif /* _MINIX_CHIP == _CHIP_INTEL */

#if (_MINIX_CHIP == M68000)
#define sc_retreg sc_regs.sr_retreg
#define sc_d1 sc_regs.sr_d1
#define sc_d2 sc_regs.sr_d2
#define sc_d3 sc_regs.sr_d3
#define sc_d4 sc_regs.sr_d4
#define sc_d5 sc_regs.sr_d5
#define sc_d6 sc_regs.sr_d6
#define sc_d7 sc_regs.sr_d7
#define sc_a0 sc_regs.sr_a0
#define sc_a1 sc_regs.sr_a1
#define sc_a2 sc_regs.sr_a2
#define sc_a3 sc_regs.sr_a3
#define sc_a4 sc_regs.sr_a4
#define sc_a5 sc_regs.sr_a5
#define sc_fp sc_regs.sr_a6
#define sc_sp sc_regs.sr_sp
#define sc_pc sc_regs.sr_pc
#define sc_psw sc_regs.sr_psw
#endif /* _MINIX_CHIP == M68000 */

/* Values for sc_flags.  Must agree with <minix/jmp_buf.h>. */
#define SC_SIGCONTEXT	2	/* nonzero when signal context is included */
#define SC_NOREGLOCALS	4	/* nonzero when registers are not to be
					saved and restored */

_PROTOTYPE( int sigreturn, (struct sigcontext *_scp)			);

#endif /* _SIGCONTEXT_H */
