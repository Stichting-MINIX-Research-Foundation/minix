/*	asyn_cancel() - cancel an asynch operation	Author: Kees J. Bot
 *								7 Jul 1997
 */
#include "asyn.h"

int asyn_cancel(asynchio_t *asyn, int fd, int op)
/* Cancel an asynchronous operation if one is in progress.  (This is easy with
 * select(2), because no operation is actually happening.)
 */
{
	asynfd_t *afd;

	if ((unsigned) fd >= FD_SETSIZE) { errno= EBADF; return -1; }
	afd= &asyn->asyn_afd[fd];

	if (afd->afd_state[op] == WAITING) {
		afd->afd_state[op]= IDLE;
		FD_CLR(fd, &asyn->asyn_fdset[SEL_READ]);
	}
	return 0;
}
