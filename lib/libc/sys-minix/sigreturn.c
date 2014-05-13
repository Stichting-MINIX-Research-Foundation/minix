#include "namespace.h"

#include <sys/cdefs.h>
#include <lib.h>

#include <string.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/signal.h>
#include <machine/signal.h>

int sigreturn(struct sigcontext *scp)
{
  sigset_t set;

  /* The message can't be on the stack, because the stack will vanish out
   * from under us.  The send part of ipc_sendrec will succeed, but when
   * a message is sent to restart the current process, who knows what will
   * be in the place formerly occupied by the message?
   */
  static message m;

  /* Protect against race conditions by blocking all interrupts. */
  sigfillset(&set);		/* splhi */
  sigprocmask(SIG_SETMASK, &set, NULL);

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_sigset.set = scp->sc_mask;
  m.m_lc_pm_sigset.ctx = (vir_bytes)scp;
  return(_syscall(PM_PROC_NR, PM_SIGRETURN, &m)); /* normally doesn't return */
}
