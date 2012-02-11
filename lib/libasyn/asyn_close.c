/*	asyn_close() - forcefully forget about a file descriptor
 *							Author: Kees J. Bot
 *								7 Jul 1997
 */
#include "asyn.h"

int asyn_close(asynchio_t *asyn, int fd)
/* Stop caring about any async operations on this file descriptor. */
{
	asynfd_t *afd;
	int op;

	if ((unsigned) fd >= FD_SETSIZE) { errno= EBADF; return -1; }

	afd= &asyn->asyn_afd[fd];

	for (op= 0; op < SEL_NR; op++) {
		afd->afd_state[op]= IDLE;
		FD_CLR(fd, &asyn->asyn_fdset[op]);
	}
	afd->afd_seen= 0;
	asyn->asyn_more++;
	return 0;
}
