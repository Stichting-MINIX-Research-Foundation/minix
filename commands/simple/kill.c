/* kill - send a signal to a process	Author: Adri Koppes  */

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void usage, (void));

/* Table of signal names. */
struct signames {
	char *name;
	int sig;
} signames[] = {
	{ "HUP",	SIGHUP		},
	{ "INT",	SIGINT		},
	{ "QUIT",	SIGQUIT		},
	{ "ILL",	SIGILL		},
	{ "TRAP",	SIGTRAP		},
	{ "ABRT",	SIGABRT		},
	{ "IOT",	SIGIOT		},
	{ "FPE",	SIGFPE		},
	{ "KILL",	SIGKILL		},
	{ "USR1",	SIGUSR1		},
	{ "SEGV",	SIGSEGV		},
	{ "USR2",	SIGUSR2		},
	{ "PIPE",	SIGPIPE		},
	{ "ALRM",	SIGALRM		},
	{ "TERM",	SIGTERM		},
	{ "EMT",	SIGEMT		},
	{ "BUS",	SIGBUS		},
	{ "CHLD",	SIGCHLD		},
	{ "CONT",	SIGCONT		},
	{ "STOP",	SIGSTOP		},
	{ "TSTP",	SIGTSTP		},
	{ "TTIN",	SIGTTIN		},
	{ "TTOU",	SIGTTOU		},
#ifdef SIGWINCH
	{ "WINCH",	SIGWINCH	},
#endif
	{ NULL,		0		}
};

int main(argc, argv)
int argc;
char **argv;
{
  pid_t proc;
  int ex = 0, sig = SIGTERM;
  char *end;
  long l;
  unsigned long ul;
  struct sigaction sa;
  int i, doit;
  struct signames *snp;

  if (argc > 1 && argv[1][0] == '-') {
	sig = -1;
	for (snp = signames; snp->name != NULL; snp++) {	/* symbolic? */
		if (strcmp(snp->name, argv[1] + 1) == 0) {
			sig = snp->sig;
			break;
		}
	}
	if (sig < 0) {						/* numeric? */
		ul = strtoul(argv[1] + 1, &end, 10);
		if (end == argv[1] + 1 || *end != 0 || ul > _NSIG) usage();
		sig = ul;
	}
	argv++;
	argc--;
  }
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = SIG_IGN;		/* try not to kill yourself */
  (void) sigaction(sig, &sa, (struct sigaction *) NULL);

  for (doit = 0; doit <= 1; doit++) {
	for (i = 1; i < argc; i++) {
		l = strtoul(argv[i], &end, 10);
		if (end == argv[i] || *end != 0 || (pid_t) l != l) usage();
		proc = l;
		if (doit && kill(proc, sig) < 0) {
			fprintf(stderr, "kill: %d: %s\n",
				proc, strerror(errno));
			ex = 1;
		}
	}
  }
  return(ex);
}

void usage()
{
  fprintf(stderr, "Usage: kill [-sig] pid\n");
  exit(1);
}
