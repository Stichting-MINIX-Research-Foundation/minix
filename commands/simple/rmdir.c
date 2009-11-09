/*	rmdir - remove directory.		Author:  Kees J. Bot
 */
#define nil 0
#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

void tell(char *what)
{
	write(2, what, strlen(what));
}

void report(char *label)
{
	char *err= strerror(errno);

	tell("rmdir: ");
	tell(label);
	tell(": ");
	tell(err);
	tell("\n");
}

int main(int argc, char **argv)
{
	int i, ex= 0;

	if (argc < 2) {
		tell("Usage: rmdir directory ...\n");
		exit(1);
	}

	i=1;
	do {
		if (rmdir(argv[i]) < 0) {
			report(argv[i]);
			ex= 1;
		}
	} while (++i < argc);

	exit(ex);
}
/* Kees J. Bot  27-12-90. */
