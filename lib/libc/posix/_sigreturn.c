#include <lib.h>
#define sigfillset	_sigfillset
#define sigprocmask	_sigprocmask
#define sigreturn	_sigreturn
#include <sys/sigcontext.h>
#include <signal.h>

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
