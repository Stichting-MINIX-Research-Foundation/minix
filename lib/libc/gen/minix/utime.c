/* utime(2) for POSIX		Authors: Terrence W. Holm & Edwin L. Froese */

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <utime.h>

int utime(const char *name, const struct utimbuf *timp)
{
  message m;

  if (timp == NULL) {
	m.m2_i1 = 0;		/* name size 0 means NULL `timp' */
	m.m2_i2 = strlen(name) + 1;	/* actual size here */
  } else {
	m.m2_l1 = timp->actime;
	m.m2_l2 = timp->modtime;
	m.m2_i1 = strlen(name) + 1;
  }
  m.m2_p1 = (char *) __UNCONST(name);
  return(_syscall(VFS_PROC_NR, UTIME, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(utime, __utime50)
#endif
