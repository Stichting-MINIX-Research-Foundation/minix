#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(fork, _fork)
#endif

pid_t fork()
{
  message m;

  return(_syscall(PM_PROC_NR, FORK, &m));
}
