/* FPU state corruption test. This used to be able to crash the kernel. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <machine/fpu.h>

int max_error = 1;
#include "common.h"


double state = 2.0;
static int count;

static void use_fpu(int n)
{
  state += (double) n * 0.5;
}

static void crashed(int sig)
{
  exit(EXIT_SUCCESS);
}

static void handler(int sig, int code, struct sigcontext *sc)
{
  memset(&sc->sc_fpu_state, count, sizeof(sc->sc_fpu_state));
}

int main(void)
{
  int status;

  start(62);
  subtest = 0;

  signal(SIGUSR1, (void (*)(int)) handler);

  /* Initialize the FPU state. This state is inherited, too. */
  use_fpu(-1);

  for (count = 0; count <= 255; count++) {
	switch (fork()) {
	case -1:
		e(1);

		break;

	case 0:
		signal(SIGFPE, crashed);

		/* Load bad state into the kernel. */
		if (kill(getpid(), SIGUSR1)) e(2);

		/* Let the kernel restore the state. */
		use_fpu(count);

		exit(EXIT_SUCCESS);

	default:
		/* We cannot tell exactly whether what happened is correct or
		 * not -- certainly not in a platform-independent way. However,
		 * if the whole system keeps running, that's good enough.
		 */
		(void) wait(&status);
	}
  }

  if (state <= 1.4 || state >= 1.6) e(3);

  quit();

  return 0;
}
