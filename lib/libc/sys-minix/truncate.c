#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#ifdef __weak_alias
__weak_alias(truncate, _truncate)
#endif

#include <minix/u64.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static int __truncate_321(const char *_path, int _length);

static int __truncate_321(const char *_path, int _length)
{
  message m;
  m.m2_p1 = (char *) __UNCONST(_path);
  m.m2_i1 = strlen(_path)+1;
  m.m2_l1 = _length;

  return(_syscall(VFS_PROC_NR, TRUNCATE_321, &m));
}

int truncate(const char *_path, off_t _length)
{
  message m;
  int orig_errno, r;

  m.m2_p1 = (char *) __UNCONST(_path);
  m.m2_i1 = strlen(_path)+1;
  m.m2_l1 = ex64lo(_length);
  m.m2_l2 = ex64hi(_length);

  orig_errno = errno;
  r = _syscall(VFS_PROC_NR, TRUNCATE, &m);

  if (r == -1 && errno == ENOSYS) {
	/* Old VFS, no support for new truncate */
	if (_length >= INT_MIN && _length <= INT_MAX) {
		errno = orig_errno;
		return __truncate_321(_path, (int) _length);
	}

	/* Not going to fit */
	errno = EOVERFLOW;
  }

  return r;
}
