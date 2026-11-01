#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <signal.h>

/* sigprocmask.c: signals owned by a signalfd are never really blocked */
void __libc_sfd_suspend_mask(const sigset_t *app, sigset_t *real);

int sigsuspend(set)
const sigset_t *set;
{
  message m;

  memset(&m, 0, sizeof(m));
  __libc_sfd_suspend_mask(set, &m.m_lc_pm_sigset.set);
  return(_syscall(PM_PROC_NR, PM_SIGSUSPEND, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigsuspend, __sigsuspend14)
#endif
