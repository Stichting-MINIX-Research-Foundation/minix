/*	env 1.1 - Set environment for command		Author: Kees J. Bot
 *								17 Dec 1997
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv)
{
	int i;
	int iflag= 0;
	int aflag= 0;
	extern char **environ;

	i= 1;
	while (i < argc && argv[i][0] == '-') {
		char *opt= argv[i++] + 1;

		if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

		if (opt[0] == 0) iflag= 1;			/* - */

		while (*opt != 0) switch (*opt++) {
		case 'i':
			iflag= 1;	/* Clear environment. */
			break;
		case 'a':		/* Specify arg 0 separately. */
			aflag= 1;
			break;
		default:
			fprintf(stderr,
		"Usage: env [-ia] [name=value] ... [utility [argument ...]]\n");
			exit(1);
		}
	}

	/* Clear the environment if -i. */
	if (iflag) *environ= nil;

	/* Set the new environment strings. */
	while (i < argc && strchr(argv[i], '=') != nil) {
		if (putenv(argv[i]) != 0) {
			fprintf(stderr, "env: Setting '%s' failed: %s\n",
				argv[i], strerror(errno));
			exit(1);
		}
		i++;
	}

	/* Environment settings and command may be separated with '--'.
	 * This is for compatibility with other envs, we don't advertise it.
	 */
	if (i < argc && strcmp(argv[i], "--") == 0) i++;

	if (i >= argc) {
		/* No utility given; print environment. */
		char **ep;

		for (ep= environ; *ep != nil; ep++) {
			if (puts(*ep) == EOF) {
				fprintf(stderr, "env: %s\n", strerror(errno));
				exit(1);
			}
		}
		return 0;
	} else {
		char *util, **args;
		int err;

		util= argv[i];
		args= argv + i;
		if (aflag) args++;
		(void) execvp(util, args);
		err= errno;
		fprintf(stderr, "env: Can't execute %s: %s\n",
			util, strerror(err));
		return err == ENOENT ? 127 : 126;
	}
}
