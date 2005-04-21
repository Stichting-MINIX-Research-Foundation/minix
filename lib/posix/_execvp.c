/*	execvp() - execute with PATH search and prepared arguments
 *							Author: Kees J. Bot
 *								21 Jan 1994
 */

#define _MINIX_SOURCE

#define nil 0
#define execve _execve
#define execvp _execvp
#define sbrk _sbrk
#define stat _stat
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

extern char * const **_penviron;	/* The default environment. */

int execvp(const char *file, char * const *argv)
/* Execute the file with a path search on $PATH, just like the shell.  The
 * search continues on the errors ENOENT (not there), and EACCES (file not
 * executable or leading directories protected.)
 * Unlike other execvp implementations there is no default path, and no shell
 * is started for scripts.  One is supposed to define $PATH, and use #!/bin/sh.
 */
{
	struct stat sb;
	const char *path;		/* $PATH */
	char *full;			/* Full name to try. */
	char *f;
	size_t full_size;
	int err= ENOENT;		/* Error return on failure. */

	if (strchr(file, '/') != nil || (path= getenv("PATH")) == nil)
		path= "";

	/* Compute the maximum length the full name may have, and align. */
	full_size= strlen(path) + 1 + strlen(file) + 1 + sizeof(char *) - 1;
	full_size&= ~(sizeof(char *) - 1);

	/* Claim space. */
	if ((full= (char *) sbrk(full_size)) == (char *) -1) {
		errno= E2BIG;
		return -1;
	}

	/* For each directory in the path... */
	do {
		f= full;
		while (*path != 0 && *path != ':') *f++= *path++;

		if (f > full) *f++= '/';

		strcpy(f, file);

		/* Stat first, small speed-up, better for ptrace. */
		if (stat(full, &sb) == -1) continue;

		(void) execve(full, argv, *_penviron);

		/* Prefer more interesting errno values then "not there". */
		if (errno != ENOENT) err= errno;

		/* Continue only on some errors. */
		if (err != ENOENT && err != EACCES) break;
	} while (*path++ != 0);

	(void) sbrk(-full_size);
	errno= err;
	return -1;
}
