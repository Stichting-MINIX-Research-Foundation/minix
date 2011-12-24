#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <sys/time.h>

#ifdef __weak_alias
__weak_alias(futimes, __futimes50)
#endif

int futimes(int fd, const struct timeval tv[2])
{
  message m;

  m.m2_i1 = fd;
  if (tv == NULL) {
	m.m2_l1 = m.m2_l2 = 0;
	m.m2_i2 = m.m2_i3 = UTIME_NOW;
  }
  else {
	m.m2_l1 = tv[0].tv_sec;
	m.m2_l2 = tv[1].tv_sec;
	m.m2_i2 = tv[0].tv_usec * 1000;
	m.m2_i3 = tv[1].tv_usec * 1000;
  }
  m.m2_p1 = NULL;
  m.m2_s1 = 0;

  return(_syscall(VFS_PROC_NR, UTIMENS, &m));
}
