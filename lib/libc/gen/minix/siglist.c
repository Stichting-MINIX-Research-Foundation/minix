#include <sys/cdefs.h>
#include <signal.h>

static const char *const __siglist14[] = {
	"Signal 0",			/* 0 */
	"Hangup",			/* 1 SIGHUP */
	"Interrupt",			/* 2 SIGINT */
	"Quit",				/* 3 SIGQUIT */
	"Illegal instruction",		/* 4 SIGILL */
	"Trace/BPT trap",		/* 5 SIGTRAP */
	"Abort trap",			/* 6 SIGABRT */
	"Bus error",			/* 7 SIGBUS */
	"Floating point exception",	/* 8 SIGFPE */
	"Killed",			/* 9 SIGKILL */
	"User defined signal 1",	/* 10 SIGUSR1 */
	"Segmentation fault",		/* 11 SIGSEGV */
	"User defined signal 2",	/* 11 SIGUSR2 */
	"Broken pipe",			/* 13 SIGPIPE */
	"Alarm clock",			/* 14 SIGALRM */
	"Terminated",			/* 15 SIGTERM */
	"EMT trap",			/* 16 SIGEMT */
	"Child exited",			/* 17 SIGCHLD */
	"Continued",			/* 18 SIGCONT */
	"Suspended (signal)",		/* 19 SIGSTOP */
	"Suspended",			/* 20 SIGTSTP */
	"Window size changes",		/* 21 SIGWINCH */
	"Stopped (tty input)",		/* 22 SIGTTIN */
	"Stopped (tty output)",		/* 23 SIGTTOU */
	"Virtual timer expired",	/* 24 SIGVTALRM */
	"Profiling timer expired",	/* 25 SIGPROF */
};

const int __sys_nsig14 = sizeof(__siglist14) / sizeof(__siglist14[0]);

const char * const *__sys_siglist14 = __siglist14;
