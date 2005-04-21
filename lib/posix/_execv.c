/*	execv() - execute with prepared arguments	Author: Kees J. Bot
 *								21 Jan 1994
 */
#define execv _execv
#define execve _execve
#include <unistd.h>

extern char * const **_penviron;	/* The default environment. */

int execv(const char *path, char * const *argv)
{
	return execve(path, argv, *_penviron);
}
