#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(_Exit, _exit)
#endif

__dead void _exit(status)
int status;
{
  void (*suicide)(void);
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_exit.status = status;
  _syscall(PM_PROC_NR, PM_EXIT, &m);

  /* If exiting nicely through PM fails for some reason, try to
   * commit suicide. E.g., message to PM might fail due to deadlock.
   */
  suicide = (void (*)(void)) -1;
  suicide();

  /* If committing suicide fails for some reason, hang. */
  for(;;) { }
}

