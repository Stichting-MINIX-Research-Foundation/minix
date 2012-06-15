/* dumpcore - create core file of running process */

#include <fcntl.h>
#include <unistd.h>	
#include <minix/config.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/const.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <timers.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <machine/archtypes.h>
#include "kernel/proc.h"

#define CLICK_WORDS (CLICK_SIZE / sizeof(unsigned long))

int main(int argc, char *argv[])
{
	pid_t pid;
	int r, status;

	if(argc != 2) {
		printf("usage: %s <pid>\n", argv[0]);
		return 1;
	}

	pid = atoi(argv[1]);

	if (ptrace(T_ATTACH, pid, 0, 0) != 0) {
		perror("ptrace(T_ATTACH)");
		return 1;
	}

	if (waitpid(pid, &status, 0) != pid) {
		perror("waitpid");
		return 1;
	}

	while (WIFSTOPPED(status) && WSTOPSIG(status) != SIGSTOP) {
		/* whatever happens here is fine */
		ptrace(T_RESUME, pid, 0, WSTOPSIG(status));

		if (waitpid(pid, &status, 0) != pid) {
			perror("waitpid");
			return 1;
		}
	}

	if (!WIFSTOPPED(status)) {
		fprintf(stderr, "process died while attaching\n");
		return 1;
	}

	if (ptrace(T_DUMPCORE, pid, 0, 0) != 0) {
		fprintf(stderr, "warning, dumpcore failed (%s)\n",
			strerror(errno));
	}

	if (ptrace(T_DETACH, pid, 0, 0)) {
		fprintf(stderr, "warning, detaching failed (%s)\n",
			strerror(errno));
	}

	return r;
}
