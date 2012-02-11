#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(_exit, __exit)
#endif

__dead void _exit(status)
int status;
{
  void (*suicide)(void);
  message m;

  m.m1_i1 = status;
  _syscall(PM_PROC_NR, EXIT, &m);

  /* If exiting nicely through PM fails for some reason, try to
   * commit suicide. E.g., message to PM might fail due to deadlock.
   */
  suicide = (void (*)(void)) -1;
  suicide();

  /* If committing suicide fails for some reason, hang. */
  for(;;) { }
}

