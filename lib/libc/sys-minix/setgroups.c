#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(setgroups, _setgroups)
#endif

#include <unistd.h>

int setgroups(int ngroups, const gid_t *gidset)
{
  message m;

  m.m1_p1 = (char *) __UNCONST(gidset);
  m.m1_i1 = ngroups;

  return(_syscall(PM_PROC_NR, SETGROUPS, &m));
}


