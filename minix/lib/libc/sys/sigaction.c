#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <signal.h>

int __sigreturn(void);

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_sig.nr = sig;
  m.m_lc_pm_sig.act = (vir_bytes)act;
  m.m_lc_pm_sig.oact = (vir_bytes)oact;
  m.m_lc_pm_sig.ret = (vir_bytes)__sigreturn;

  return(_syscall(PM_PROC_NR, PM_SIGACTION, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigaction, __sigaction14)
#endif
