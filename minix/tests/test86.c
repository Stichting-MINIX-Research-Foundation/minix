#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "common.h"

/*
 * Test for dynamic executables with no read permissions.  This test relies on
 * being linked dynamically.
 */
int
main(int argc, char ** argv)
{
	char *executable, cp_cmd[PATH_MAX + 9];
	int status;

	if (strcmp(argv[0], "DO CHECK") == 0)
		exit(EXIT_SUCCESS);

	start(86);

	/* Make a copy of this binary which is executable-only. */
	executable = argv[0];

	snprintf(cp_cmd, sizeof(cp_cmd), "cp ../%s .", executable);
	status = system(cp_cmd);
	if (status < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS) e(0);

	if (chmod(executable, S_IXUSR) != 0) e(0);

	/* Invoke the changed binary in a child process. */
	switch (fork()) {
	case -1:
		e(0);
	case 0:
		execl(executable, "DO CHECK", NULL);

		exit(EXIT_FAILURE);
	default:
		if (wait(&status) <= 0) e(0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
			e(0);
	}

	quit();
	/* NOTREACHED */
}
