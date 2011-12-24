#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>

int futimens(int fd, const struct timespec tv[2])
{
  message m;
  static const struct timespec now[2] = { {0, UTIME_NOW}, {0, UTIME_NOW} };

  if (tv == NULL) tv = now;

  m.m2_i1 = fd;
  m.m2_l1 = tv[0].tv_sec;
  m.m2_l2 = tv[1].tv_sec;
  m.m2_i2 = tv[0].tv_nsec;
  m.m2_i3 = tv[1].tv_nsec;
  m.m2_p1 = NULL;
  m.m2_s1 = 0;

  return(_syscall(VFS_PROC_NR, UTIMENS, &m));
}
