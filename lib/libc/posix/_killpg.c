#include <lib.h>
#define killpg	_killpg
#define kill	_kill
#include <signal.h>

PUBLIC int killpg(pgrp, sig)
int pgrp;			/* which process group is to be sent the
				 * signal
				 */
int sig;			/* signal number */
{
  return kill(-pgrp, sig);
}
