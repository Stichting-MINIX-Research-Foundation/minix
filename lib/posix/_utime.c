/* utime(2) for POSIX		Authors: Terrence W. Holm & Edwin L. Froese */

#include <lib.h>
#define utime	_utime
#include <string.h>
#include <utime.h>

PUBLIC int utime(name, timp)
_CONST char *name;
_CONST struct utimbuf *timp;
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
  m.m2_p1 = (char *) name;
  return(_syscall(FS, UTIME, &m));
}
