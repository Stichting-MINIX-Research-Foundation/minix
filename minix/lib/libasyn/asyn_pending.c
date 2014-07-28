/*	asyn_pending() - any results pending?		Author: Kees J. Bot
 *								7 Jul 1997
 */
#include "asyn.h"

int asyn_pending(asynchio_t *asyn, int fd, int op)
/* Check if a result of an operation is pending.  (This is easy with
 * select(2), because no operation is actually happening.)
 */
{
	if ((unsigned) fd >= FD_SETSIZE) { errno= EBADF; return -1; }

	return 0;
}
