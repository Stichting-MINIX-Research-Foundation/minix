/*	execl() - execute				Author: Kees J. Bot
 *								21 Jan 1994
 */
#define execl _execl
#define execve _execve
#include <unistd.h>

extern char * const **_penviron;	/* The default environment. */

int execl(const char *path, const char *arg1, ...)
/* execl("/bin/sh", "sh", "-c", "example", (char *) 0); */
{
	/* Assumption:  The C-implementation for this machine pushes
	 * function arguments downwards on the stack making a perfect
	 * argument array.  Luckily this is almost always so.
	 */
	return execve(path, (char * const *) &arg1, *_penviron);
}
