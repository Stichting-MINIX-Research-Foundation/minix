#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <signal.h>
#include <sys/signal.h>

#ifdef __weak_alias
__weak_alias(sigreturn, _sigreturn)
#endif

int sigreturn(scp)
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
  return(_syscall(PM_PROC_NR, SIGRETURN, &m));	/* normally this doesn't return */
}
