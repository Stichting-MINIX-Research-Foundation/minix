#include <lib.h>
#define sigfillset	_sigfillset
#define sigjmp		_sigjmp
#define sigprocmask	_sigprocmask
#define sigreturn	_sigreturn
#include <sys/sigcontext.h>
#include <setjmp.h>
#include <signal.h>

_PROTOTYPE( int sigjmp, (jmp_buf jb, int retval));

#if (_SETJMP_SAVES_REGS == 0)
/* 'sigreturn' using a short format jmp_buf (no registers saved). */
PUBLIC int sigjmp(jb, retval)
jmp_buf jb;
int retval;
{
  struct sigcontext sc;

  sc.sc_flags = jb[0].__flags;
  sc.sc_mask = jb[0].__mask;

#if (CHIP == INTEL)
  sc.sc_pc = (int) jb[0].__pc;
  sc.sc_sp = (int) jb[0].__sp;
  sc.sc_fp = (int) jb[0].__lb;
#endif

#if (CHIP == M68000)
  sc.sc_pc = (long) jb[0].__pc;
  sc.sc_sp = (long) jb[0].__sp;
  sc.sc_fp = (long) jb[0].__lb;
#endif

  sc.sc_retreg = retval;
  return sigreturn(&sc);
}
#endif

PUBLIC int sigreturn(scp)
register struct sigcontext *scp;
{
  sigset_t set;

  /* The message can't be on the stack, because the stack will vanish out
   * from under us.  The send part of sendrec will succeed, but when
   * a message is sent to restart the current process, who knows what will
   * be in the place formerly occupied by the message?
   */
  static message m;

  /* Protect against race conditions by blocking all interrupts. */
  sigfillset(&set);		/* splhi */
  sigprocmask(SIG_SETMASK, &set, (sigset_t *) NULL);

  m.m2_l1 = scp->sc_mask;
  m.m2_i2 = scp->sc_flags;
  m.m2_p1 = (char *) scp;
  return(_syscall(MM, SIGRETURN, &m));	/* normally this doesn't return */
}
