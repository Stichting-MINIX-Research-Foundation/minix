/*	asyn_init()					Author: Kees J. Bot
 *								7 Jul 1997
 */
#include "asyn.h"

void asyn_init(asynchio_t *asyn)
{
	memset(asyn, 0, sizeof(*asyn));
}
