/*	asyn_synch() - step back to synch		Author: Kees J. Bot
 *								7 Jul 1997
 */
#include "asyn.h"

int asyn_synch(asynchio_t *asyn, int fd)
/* No more asynchronous operations on this file descriptor. */
{
	asynfd_t *afd;
	int op;

	if ((unsigned) fd >= FD_SETSIZE) { errno= EBADF; return -1; }
	afd= &asyn->asyn_afd[fd];

	for (op= 0; op < SEL_NR; op++) {
		if (afd->afd_state[op] != IDLE) {
			errno= EAGAIN;
			return -1;
		}
	}

	/* Make sure the file flags are as they once were. */
	if (afd->afd_seen && fcntl(fd, F_SETFL, afd->afd_flags) < 0) return -1;
	afd->afd_seen= 0;
	return 0;
}
