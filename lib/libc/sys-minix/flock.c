/* Library routines
 *
 * Porting to Minix 2.0.0
 * Author:	Giovanni Falzoni <gfalzoni@pointest.com>
 */

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/*
 *	Name:		int flock(int fd, int mode);
 *	Function:	Implements the flock function in Minix.
 */
int flock(int fd, int mode)
{
  struct flock lck;
  register int retcode;

  memset((void *) &lck, 0, sizeof(struct flock));
  lck.l_type = mode & ~LOCK_NB;
  lck.l_pid = getpid();
  if ((retcode = fcntl(fd, mode & LOCK_NB ? F_SETLK : F_SETLKW, &lck)) < 0 && errno == EAGAIN)
	errno = EWOULDBLOCK;
  return retcode;
}

/** flock.c **/
