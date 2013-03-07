#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <minix/u64.h>
#include <errno.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(lseek, _lseek)
#endif

i32_t __lseek_321(int fd, i32_t offset, int whence);

i32_t __lseek_321(int fd, i32_t offset, int whence)
{
  message m;

  m.m2_i1 = fd;
  m.m2_l1 = offset;
  m.m2_i2 = whence;
  if (_syscall(VFS_PROC_NR, LSEEK_321, &m) < 0) return(-1);
  return( (i32_t) m.m2_l1);
}

off_t
lseek(int fd, off_t offset, int whence)
{
  message m;
  int orig_errno;

  m.m2_i1 = fd;
  m.m2_l1 = ex64lo(offset);
  m.m2_l2 = ex64hi(offset);
  m.m2_i2 = whence;

  orig_errno = errno;
  if (_syscall(VFS_PROC_NR, LSEEK, &m) < 0) {
	if (errno == ENOSYS) {
		/* Old VFS, no support for new lseek */
		if (offset >= INT_MIN && offset <= INT_MAX) {
			/* offset fits in old range, retry */
			errno = orig_errno;
			return (off_t) __lseek_321(fd, (i32_t) offset, whence);
		}

		/* Not going to fit */
		errno = EOVERFLOW;
	}

	return( (off_t) -1);
  }
  return( (off_t) make64(m.m2_l1, m.m2_l2));
}
