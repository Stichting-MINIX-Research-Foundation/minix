#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <minix/u64.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(ftruncate, _ftruncate)
#endif

static int __ftruncate_321(int _fd, int _length);

static int __ftruncate_321(int _fd, int _length)
{
  message m;
  m.m2_l1 = _length;
  m.m2_i1 = _fd;

  return(_syscall(VFS_PROC_NR, FTRUNCATE_321, &m));
}

int ftruncate(int _fd, off_t _length)
{
  message m;
  int orig_errno, r;

  m.m2_l1 = ex64lo(_length);
  m.m2_l2 = ex64hi(_length);
  m.m2_i1 = _fd;

  orig_errno = errno;
  r = _syscall(VFS_PROC_NR, FTRUNCATE, &m);
  if (r == -1 && errno == ENOSYS) {
	/* Old VFS, no support for new ftruncate */
	if (_length >= INT_MIN && _length <= INT_MAX) {
		errno = orig_errno;
		return __ftruncate_321(_fd, (int) _length);
	}

	/* Not going to fit */
	errno = EOVERFLOW;
  }

  return r;
}
