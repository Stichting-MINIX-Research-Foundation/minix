#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#ifdef __weak_alias
__weak_alias(lutimes, __lutimes50)
#endif

int lutimes(const char *name, const struct timeval tv[2])
{
  message m;

  if (name == NULL) return EINVAL;
  if (name[0] == '\0') return ENOENT; /* X/Open requirement */
  m.m2_i1 = strlen(name) + 1;
  m.m2_p1 = (char *) __UNCONST(name);
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
  m.m2_s1 = AT_SYMLINK_NOFOLLOW;

  return(_syscall(VFS_PROC_NR, UTIMENS, &m));
}
