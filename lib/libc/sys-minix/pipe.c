#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(pipe, _pipe)
#endif

int pipe(fild)
int fild[2];
{
  message m;

  if (_syscall(VFS_PROC_NR, PIPE, &m) < 0) return(-1);
  fild[0] = m.m1_i1;
  fild[1] = m.m1_i2;
  return(0);
}
