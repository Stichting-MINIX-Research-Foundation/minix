#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <fcntl.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(dup2, _dup2)
#endif

int dup2(fd, fd2)
int fd, fd2;
{
/* The behavior of dup2 is defined by POSIX in 6.2.1.2 as almost, but not
 * quite the same as fcntl.
 */

  if (fd2 < 0 || fd2 > OPEN_MAX) {
	errno = EBADF;
	return(-1);
  }

  /* Check to see if fildes is valid. */
  if (fcntl(fd, F_GETFL) < 0) {
	/* 'fd' is not valid. */
	return(-1);
  } else {
	/* 'fd' is valid. */
	if (fd == fd2) return(fd2);
	close(fd2);
	return(fcntl(fd, F_DUPFD, fd2));
  }
}
