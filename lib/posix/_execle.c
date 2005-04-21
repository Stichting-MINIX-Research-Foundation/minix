/*	execle() - execute with a custom environment	Author: Kees J. Bot
 *								21 Jan 1994
 */
#define nil 0
#define execle _execle
#define execve _execve
#include <unistd.h>
#include <stdarg.h>

int execle(const char *path, const char *arg1, ...)
/* execle("/bin/sh", "sh", "-c", "example", (char *) 0, my_env_array); */
{
	char * const * envp;
	va_list ap;

	va_start(ap, arg1);

	/* Find the end of the argument array. */
	if (arg1 != nil) while (va_arg(ap, const char *) != nil) {}

	envp = va_arg(ap, char * const *);
	va_end(ap);

	return execve(path, (char * const *) &arg1, envp);
}
