/* Tests for PM signal handling robustness - by D.C. van Moolenbroek */
/*
 * The signal handling code must not rely on priorities assigned to services,
 * and so, this test (like any test!) must also pass if PM and/or VFS are not
 * given a fixed high priority.  A good way to verify this is to let PM and VFS
 * be scheduled by SCHED rather than KERNEL, and to give them the same priority
 * as (or slightly lower than) normal user processes.  Note that if VFS is
 * configured to use a priority *far lower* than user processes, starvation may
 * cause this test not to complete in some scenarios.  In that case, Ctrl+C
 * should still be able to kill the test.
 */
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/utsname.h>

#define ITERATIONS 1

#include "common.h"

#define NR_SIGNALS	20000

#define MAX_SIGNALERS	3

static const int signaler_sig[MAX_SIGNALERS] = { SIGUSR1, SIGUSR2, SIGHUP };
static pid_t signaler_pid[MAX_SIGNALERS];
static int sig_counter;

enum {
	JOB_RUN = 0,
	JOB_CALL_PM,
	JOB_CALL_VFS,
	JOB_SET_MASK,
	JOB_BLOCK_PM,
	JOB_BLOCK_VFS,
	JOB_CALL_PM_VFS,
	JOB_FORK,
	NR_JOBS
};

#define OPT_NEST	0x1
#define OPT_ALARM	0x2
#define OPT_ALL		0x3

struct link {
	pid_t pid;
	int sndfd;
	int rcvfd;
};

/*
 * Spawn a child process, with a pair of pipes to talk to it bidirectionally.
 */
static void
spawn(struct link *link, void (*proc)(struct link *))
{
	int up[2], dn[2];

	fflush(stdout);
	fflush(stderr);

	if (pipe(up) != 0) e(0);
	if (pipe(dn) != 0) e(0);

	link->pid = fork();

	switch (link->pid) {
	case 0:
		close(up[1]);
		close(dn[0]);

		link->rcvfd = up[0];
		link->sndfd = dn[1];

		errct = 0;

		proc(link);

		/* Close our pipe FDs on exit, so that we can make zombies. */
		exit(errct);
	case -1:
		e(0);
		break;
	}

	close(up[0]);
	close(dn[1]);

	link->sndfd = up[1];
	link->rcvfd = dn[0];
}

/*
 * Wait for a child process to terminate, and clean up.
 */
static void
collect(struct link *link)
{
	int status;

	close(link->sndfd);
	close(link->rcvfd);

	if (waitpid(link->pid, &status, 0) <= 0) e(0);

	if (!WIFEXITED(status)) e(0);
	else errct += WEXITSTATUS(status);
}

/*
 * Forcibly terminate a child process, and clean up.
 */
static void
terminate(struct link *link)
{
	int status;

	if (kill(link->pid, SIGKILL) != 0) e(0);

	close(link->sndfd);
	close(link->rcvfd);

	if (waitpid(link->pid, &status, 0) <= 0) e(0);

	if (WIFSIGNALED(status)) {
		if (WTERMSIG(status) != SIGKILL) e(0);
	} else {
		if (!WIFEXITED(status)) e(0);
		else errct += WEXITSTATUS(status);
	}
}

/*
 * Send an integer value to the child or parent.
 */
static void
snd(struct link *link, int val)
{
	if (write(link->sndfd, (void *) &val, sizeof(val)) != sizeof(val))
		e(0);
}

/*
 * Receive an integer value from the child or parent, or -1 on EOF.
 */
static int
rcv(struct link *link)
{
	int r, val;

	if ((r = read(link->rcvfd, (void *) &val, sizeof(val))) == 0)
		return -1;

	if (r != sizeof(val)) e(0);

	return val;
}

/*
 * Set a signal handler for a particular signal, blocking either all or no
 * signals when the signal handler is invoked.
 */
static void
set_handler(int sig, void (*proc)(int), int block)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	if (block) sigfillset(&act.sa_mask);
	act.sa_handler = proc;

	if (sigaction(sig, &act, NULL) != 0) e(0);
}

/*
 * Generic signal handler for the worker process.
 */
static void
worker_handler(int sig)
{
	int i;

	switch (sig) {
	case SIGUSR1:
	case SIGUSR2:
	case SIGHUP:
		for (i = 0; i < MAX_SIGNALERS; i++) {
			if (signaler_sig[i] != sig) continue;

			if (signaler_pid[i] == -1) e(0);
			else if (kill(signaler_pid[i], SIGUSR1) != 0) e(0);
			break;
		}
		if (i == MAX_SIGNALERS) e(0);
		break;
	case SIGTERM:
		exit(errct);
		break;
	case SIGALRM:
		/* Do nothing. */
		break;
	default:
		e(0);
	}
}

/*
 * Procedure for the worker process.  Sets up its own environment using
 * information sent to it by the parent, sends an acknowledgement to the
 * parent, and loops executing the job given to it until a SIGTERM comes in.
 */
static void __dead
worker_proc(struct link *parent)
{
	struct utsname name;
	struct itimerval it;
	struct timeval tv;
	sigset_t set, oset;
	uid_t uid;
	int i, job, options;

	job = rcv(parent);
	options = rcv(parent);

	for (i = 0; i < MAX_SIGNALERS; i++) {
		set_handler(signaler_sig[i], worker_handler,
		    !(options & OPT_NEST));

		signaler_pid[i] = rcv(parent);
	}

	set_handler(SIGTERM, worker_handler, 1 /* block */);
	set_handler(SIGALRM, worker_handler, !(options & OPT_NEST));

	snd(parent, 0);

	if (options & OPT_ALARM) {
		/* The timer would kill wimpy platforms such as ARM. */
		if (uname(&name) < 0) e(0);
		if (strcmp(name.machine, "arm")) {
			it.it_value.tv_sec = 0;
			it.it_value.tv_usec = 1;
			it.it_interval.tv_sec = 0;
			it.it_interval.tv_usec = 1;
			if (setitimer(ITIMER_REAL, &it, NULL) != 0) e(0);
		}
	}

	switch (job) {
	case JOB_RUN:
		for (;;);
		break;
	case JOB_CALL_PM:
		/*
		 * Part of the complication of the current system in PM comes
		 * from the fact that when a process is being stopped, it might
		 * already have started sending a message.  That message will
		 * arrive at its destination regardless of the process's run
		 * state.  PM must avoid setting up a signal handler (and
		 * changing the process's signal mask as part of that) if such
		 * a message is still in transit, because that message might,
		 * for example, query (or even change) the signal mask.
		 */
		for (;;) {
			if (sigprocmask(SIG_BLOCK, NULL, &set) != 0) e(0);
			if (sigismember(&set, SIGUSR1)) e(0);
		}
		break;
	case JOB_CALL_VFS:
		for (;;) {
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			select(0, NULL, NULL, NULL, &tv);
		}
		break;
	case JOB_SET_MASK:
		for (;;) {
			sigfillset(&set);
			if (sigprocmask(SIG_SETMASK, &set, &oset) != 0) e(0);
			if (sigprocmask(SIG_SETMASK, &oset, NULL) != 0) e(0);
		}
		break;
	case JOB_BLOCK_PM:
		for (;;) {
			sigemptyset(&set);
			sigsuspend(&set);
		}
		break;
	case JOB_BLOCK_VFS:
		for (;;)
			select(0, NULL, NULL, NULL, NULL);
		break;
	case JOB_CALL_PM_VFS:
		uid = getuid();
		for (;;)
			setuid(uid);
		break;
	case JOB_FORK:
		/*
		 * The child exits immediately; the parent kills the child
		 * immediately.  The outcome mostly depends on scheduling.
		 * Varying process priorities may yield different tests.
		 */
		for (;;) {
			pid_t pid = fork();
			switch (pid) {
			case 0:
				exit(0);
			case -1:
				e(1);
				break;
			default:
				kill(pid, SIGKILL);
				if (wait(NULL) != pid) e(0);
			}
		}
		break;
	default:
		e(0);
		exit(1);
	}
}

/*
 * Signal handler procedure for the signaler processes, counting the number of
 * signals received from the worker process.
 */
static void
signaler_handler(int sig)
{
	sig_counter++;
}

/*
 * Procedure for the signaler processes.  Gets the pid of the worker process
 * and the signal to use, and then repeatedly sends that signal to the worker
 * process, waiting for a SIGUSR1 signal back from the worker before
 * continuing.  This signal ping-pong is repeated for a set number of times.
 */
static void
signaler_proc(struct link *parent)
{
	sigset_t set, oset;
	pid_t pid;
	int i, sig, nr;

	pid = rcv(parent);
	sig = rcv(parent);
	nr = rcv(parent);
	sig_counter = 0;

	sigfillset(&set);
	if (sigprocmask(SIG_SETMASK, &set, &oset) != 0) e(0);

	set_handler(SIGUSR1, signaler_handler, 1 /*block*/);

	for (i = 0; nr == 0 || i < nr; i++) {
		if (sig_counter != i) e(0);

		if (kill(pid, sig) != 0 && nr > 0) e(0);

		sigsuspend(&oset);
	}

	if (sig_counter != nr) e(0);
}

/*
 * Set up the worker and signaler processes, wait for the signaler processes to
 * do their work and terminate, and then terminate the worker process.
 */
static void
sub79a(int job, int signalers, int options)
{
	struct link worker, signaler[MAX_SIGNALERS];
	int i;

	spawn(&worker, worker_proc);

	snd(&worker, job);
	snd(&worker, options);

	for (i = 0; i < signalers; i++) {
		spawn(&signaler[i], signaler_proc);

		snd(&worker, signaler[i].pid);
	}
	for (; i < MAX_SIGNALERS; i++)
		snd(&worker, -1);

	if (rcv(&worker) != 0) e(0);

	for (i = 0; i < signalers; i++) {
		snd(&signaler[i], worker.pid);
		snd(&signaler[i], signaler_sig[i]);
		snd(&signaler[i], NR_SIGNALS);
	}

	for (i = 0; i < signalers; i++)
		collect(&signaler[i]);

	if (kill(worker.pid, SIGTERM) != 0) e(0);

	collect(&worker);
}

/*
 * Stress test for signal handling.  One worker process gets signals from up to
 * three signaler processes while performing one of a number of jobs.  It
 * replies to each signal by signaling the source, thus creating a ping-pong
 * effect for each of the signaler processes.  The signal ping-ponging is
 * supposed to be reliable, and the most important aspect of the test is that
 * no signals get lost.  The test is performed a number of times, varying the
 * job executed by the worker process, the number of signalers, whether signals
 * are blocked while executing a signal handler in the worker, and whether the
 * worker process has a timer running at high frequency.
 */
static void
test79a(void)
{
	int job, signalers, options;

	subtest = 1;

	for (options = 0; options <= OPT_ALL; options++)
		for (signalers = 1; signalers <= MAX_SIGNALERS; signalers++)
			for (job = 0; job < NR_JOBS; job++)
				sub79a(job, signalers, options);
}

/*
 * Set up the worker process and optionally a signaler process, wait for a
 * predetermined amount of time, and then kill all the child processes.
 */
static void
sub79b(int job, int use_signaler, int options)
{
	struct link worker, signaler;
	struct timeval tv;
	int i;

	spawn(&worker, worker_proc);

	snd(&worker, job);
	snd(&worker, options);

	if ((i = use_signaler) != 0) {
		spawn(&signaler, signaler_proc);

		snd(&worker, signaler.pid);
	}
	for (; i < MAX_SIGNALERS; i++)
		snd(&worker, -1);

	if (rcv(&worker) != 0) e(0);

	if (use_signaler) {
		snd(&signaler, worker.pid);
		snd(&signaler, signaler_sig[0]);
		snd(&signaler, 0);
	}

	/* Use select() so that we can verify we don't get signals. */
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	if (select(0, NULL, NULL, NULL, &tv) != 0) e(0);

	terminate(&worker);

	if (use_signaler)
		terminate(&signaler);
}

/*
 * This test is similar to the previous one, except that we now kill the worker
 * process after a while.  This should trigger various process transitions to
 * the exiting state.  Not much can be verified from this test program, but we
 * intend to trigger as many internal state verification statements of PM
 * itself as possible this way.  A signaler process is optional in this test,
 * and if used, it will not stop after a predetermined number of signals.
 */
static void
test79b(void)
{
	int job, signalers, options;

	subtest = 2;

	for (options = 0; options <= OPT_ALL; options++)
		for (signalers = 0; signalers <= 1; signalers++)
			for (job = 0; job < NR_JOBS; job++)
				sub79b(job, signalers, options);

}

/*
 * PM signal handling robustness test program.
 */
int
main(int argc, char **argv)
{
	int i, m;

	start(79);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x01) test79a();
		if (m & 0x02) test79b();
	}

	quit();
}
