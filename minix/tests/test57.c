
/* This test tests whether registers are correctly restored after a
 * signal handler is executed. The assembly file (test57loop.S) puts
 * 'random' values in the registers, and the C code checks whether
 * these values are the same, before and after the signal handler.
 */

#define _POSIX_SOURCE 1

#include <stdio.h>
#include <signal.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#define SIGNAL SIGUSR1

volatile int remaining_invocations = 2, handler_level = 0;

void check_context_loop(void);

#define REGS 8	/* how many registers pusha and popa save. */

#define ESP 3	/* where is esp saved? */

unsigned long newstate[REGS], origstate[REGS];

static void handler(int signal)
{
	int st;
	sigset_t set, oset;
	handler_level++;
	remaining_invocations--;
	if(remaining_invocations < 1)
		return;
	sigemptyset(&set);
	sigaddset(&set, SIGNAL);
	sigprocmask(SIG_UNBLOCK, &set,  &oset);
	wait(&st);
	handler_level--;
}

int main(int argc, char *argv[])
{
	pid_t child_pid;

	printf("Test 57 ");

	if(signal(SIGNAL, handler) == SIG_ERR)
		err(1, "signal");

	fflush(NULL);

	if((child_pid=fork()) < 0)
		err(1, "fork");

	if(child_pid == 0) {
		pid_t ppid = 0;

		/* Keep signaling the parent until
		 * it disappears.
		 */
		while((ppid = getppid()) > 1) {
			if(kill(ppid, SIGNAL) < 0)
				err(1, "kill");
			sleep(1);
		}

		exit(0);
	} else {
		int i;
		int err = 0;

		check_context_loop();

		/* correct 2nd esp for 'pusha' difference. */
		newstate[ESP] += REGS*4;

		for(i = 0; i < REGS; i++) {
#if 0
			printf("%d %08lx %08lx diff  ",
				i, newstate[i], origstate[i]);
#endif
			if(newstate[i] != origstate[i]) {
				fprintf(stderr, "reg %d changed; "
					"found 0x%lx, expected 0x%lx\n",
					i, newstate[i], origstate[i]);
				err = 1;
			}
		}

		if(!err) printf("ok\n");

		exit(err);
	}

	return 0;
}

