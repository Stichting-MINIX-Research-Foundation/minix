/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#if	defined(_POSIX_SOURCE)
#include	<sys/types.h>
#endif
#include	<signal.h>

int _kill(int pid, int sig);
pid_t _getpid(void);

int
raise(int sig)
{
	if (sig < 0 || sig >= _NSIG)
		return -1;
	return _kill(_getpid(), sig);
}
