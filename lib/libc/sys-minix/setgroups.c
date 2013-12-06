#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <unistd.h>

int setgroups(int ngroups, const gid_t *gidset)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_GROUPS_PTR = (char *) __UNCONST(gidset);
  m.PM_GROUPS_NUM = ngroups;

  return(_syscall(PM_PROC_NR, PM_SETGROUPS, &m));
}
