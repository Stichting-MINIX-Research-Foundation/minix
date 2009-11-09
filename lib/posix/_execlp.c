/*	execlp() - execute with PATH search		Author: Kees J. Bot
 *								22 Jan 1994
 */
#define execlp _execlp
#define execvp _execvp
#include <unistd.h>

int execlp(const char *file, const char *arg1, ...)
/* execlp("sh", "sh", "-c", "example", (char *) 0); */
{
	return execvp(file, (char * const *) &arg1);
}
