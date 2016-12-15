/* trace(1) - the MINIX3 system call tracer - by D.C. van Moolenbroek */

#include "inc.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <err.h>

/* Global variables, used only for a subset of the command line options. */
int timestamps;		 /* 0 = none, 1 = time w/o usecs, 2 = time w/usecs */
int allnames;		 /* FALSE = structure field names, TRUE = all names */
unsigned int valuesonly; /* 0 = normal, 1 = no symbols, 2 = no structures */
unsigned int verbose;	 /* 0 = essentials, 1 = elaborate, 2 = everything */

/* Local variables, for signal handling. */
static int got_signal, got_info;

/*
 * Signal handler for signals that are supposed to make us terminate.  Let the
 * main loop do the actual work, since it might be in the middle of processing
 * a process status change right now.
 */
static void
sig_handler(int __unused sig)
{

	got_signal = TRUE;

}

/*
 * Signal handler for the SIGINFO signal.  Let the main loop report on all
 * processes currenty being traced.  Since SIGINFO is sent to the current
 * process group, traced children may get the signal as well.  This is both
 * intentional and impossible to prevent.
 */
static void
info_handler(int __unused sig)
{

	got_info = TRUE;
}

/*
 * Print a list of traced processes and their call status.  We must not
 * interfere with actual process output, so perform out-of-band printing
 * (with info lines rather than lines prefixed by each process's PID).
 */
static void
list_info(void)
{
	struct trace_proc *proc;
	int no_call, in_call;

	put_newline();

	for (proc = proc_next(NULL); proc != NULL; proc = proc_next(proc)) {
		/*
		 * When attaching to an existing process, there is no way to
		 * find out whether the process is in a system call or not.
		 */
		no_call = (proc->trace_flags & TF_NOCALL);
		in_call = (proc->trace_flags & TF_INCALL);
		assert(!in_call || !no_call);

		put_fmt(NULL, "Tracing %s (pid %d), %s%s%s", proc->name,
		    proc->pid, no_call ? "call status unknown" :
		    (in_call ? "in a " : "not in a call"),
		    in_call ? call_name(proc) : "",
		    in_call ? " call" : "");
		put_newline();
	}
}

/*
 * Either we have just started or attached to the given process, it the process
 * has performed a successful execve() call.  Obtain the new process name, and
 * print a banner for it.
 */
static void
new_exec(struct trace_proc * proc)
{

	/* Failure to obtain the process name is worrisome, but not fatal.. */
	if (kernel_get_name(proc->pid, proc->name, sizeof(proc->name)) < 0)
		strlcpy(proc->name, "<unknown>", sizeof(proc->name));

	put_newline();
	put_fmt(proc, "Tracing %s (pid %d)", proc->name, proc->pid);
	put_newline();
}

/*
 * We have started or attached to a process.  Set the appropriate flags, and
 * print a banner showing that we are now tracing it.
 */
static void
new_proc(struct trace_proc * proc, int follow_fork)
{
	int fl;

	/* Set the desired tracing options. */
	fl = TO_ALTEXEC;
	if (follow_fork) fl |= TO_TRACEFORK;

	(void)ptrace(T_SETOPT, proc->pid, 0, fl);

	/*
	 * When attaching to an arbitrary process, this process might be in the
	 * middle of an execve().  Now that we have enabled TO_ALTEXEC, we may
	 * now get a SIGSTOP signal next.  Guard against this by marking the
	 * first system call as a possible execve().
	 */
	if ((proc->trace_flags & (TF_ATTACH | TF_STOPPING)) == TF_ATTACH)
		proc->trace_flags |= TF_EXEC;

	new_exec(proc);
}

/*
 * A process has terminated or is being detached.  Print the resulting status.
 */
static void
discard_proc(struct trace_proc * proc, int status)
{
	const char *signame;

	/*
	 * The exit() calls are of type no-return, meaning they are expected
	 * not to return.  However, calls of this type may in fact return an
	 * error, in which case the error must be printed.  Thus, such calls
	 * are not actually finished until the end of the call-leave phase.
	 * For exit() calls, a successful call will never get to the call-leave
	 * phase.  The result is that such calls will end up being shown as
	 * suspended, which is unintuitive.  To counter this, we pretend that a
	 * clean process exit is in fact preceded by a call-leave event, thus
	 * allowing the call to be printed without suspension.  An example:
	 *
	 *        3| exit(0) <..>
	 *        2| setsid() = 2
	 * [A]    3| exit(0)
	 *        3| Process exited normally with code 0
	 *
	 * The [A] line is the result of the following code.
	 */
	if (WIFEXITED(status) && (proc->trace_flags & TF_INCALL))
		call_leave(proc, TRUE /*skip*/);

	put_newline();
	if (WIFEXITED(status)) {
		put_fmt(proc, "Process exited normally with code %d",
		    WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		if ((signame = get_signal_name(WTERMSIG(status))) != NULL)
			put_fmt(proc, "Process terminated from signal %s",
			    signame);
		else
			put_fmt(proc, "Process terminated from signal %d",
			    WTERMSIG(status));
	} else if (WIFSTOPPED(status))
		put_text(proc, "Process detached");
	else
		put_fmt(proc, "Bogus wait result (%04x)", status);
	put_newline();

	proc_del(proc);
}

/*
 * The given process has been stopped on a system call, either entering or
 * leaving that call.
 */
static void
handle_call(struct trace_proc * proc, int show_stack)
{
	reg_t pc, sp;
	int class, skip, new_ctx;

	proc->trace_flags &= ~TF_NOCALL;

	if (proc->trace_flags & TF_SKIP) {
		/* Skip the call leave phase after a successful execve(). */
		proc->trace_flags &= ~(TF_INCALL | TF_SKIP);
	} else if (!(proc->trace_flags & TF_INCALL)) {
		/*
		 * The call_enter call returns the class of the call:
		 * TC_NORMAL, TC_EXEC, or TC_SIGRET.  TC_EXEC means that an
		 * execve() call is being performed.  This means that if a
		 * SIGSTOP follows for the current process, the process has
		 * successfully started a different executable.  TC_SIGRET
		 * means that if successful, the call will have a bogus return
		 * value.  TC_NORMAL means that the call requires no exception.
		 */
		class = call_enter(proc, show_stack);

		switch (class) {
		case TC_NORMAL:
			break;
		case TC_EXEC:
			proc->trace_flags |= TF_EXEC;
			break;
		case TC_SIGRET:
			proc->trace_flags |= TF_CTX_SKIP;
			break;
		default:
			assert(0);
		}

		/* Save the current program counter and stack pointer. */
		if (!kernel_get_context(proc->pid, &pc, &sp, NULL /*fp*/)) {
			proc->last_pc = pc;
			proc->last_sp = sp;
		} else
			proc->last_pc = proc->last_sp = 0;

		proc->trace_flags |= TF_INCALL;
	} else {
		/*
		 * Check if the program counter or stack pointer have changed
		 * during the system call.  If so, this is a strong indication
		 * that a sigreturn call has succeeded, and thus its result
		 * must be skipped, since the result register will not contain
		 * the result of the call.
		 */
		new_ctx = (proc->last_pc != 0 &&
		    !kernel_get_context(proc->pid, &pc, &sp, NULL /*fp*/) &&
		    (pc != proc->last_pc || sp != proc->last_sp));

		skip = ((proc->trace_flags & TF_CTX_SKIP) && new_ctx);

		call_leave(proc, skip);

		/*
		 * On such context changes, also print a short dashed line.
		 * This helps in identifying signal handler invocations,
		 * although it is not reliable for that purpose: no dashed line
		 * will be printed if a signal handler is invoked while the
		 * process is not making a system call.
		 */
		if (new_ctx) {
			put_text(proc, "---");
			put_newline();
		}

		proc->trace_flags &= ~(TF_INCALL | TF_CTX_SKIP | TF_EXEC);
	}
}

/*
 * The given process has received the given signal.  Report the receipt.  Due
 * to the way that signal handling with traced processes works, the signal may
 * in fact be delivered to the process much later, or never--a problem inherent
 * to the way signals are handled in PM right now (namely, deferring signal
 * delivery would let the traced process block signals meant for the tracer).
 */
static void
report_signal(struct trace_proc * proc, int sig, int show_stack)
{
	const char *signame;

	/*
	 * Print a stack trace only if we are not in a call; otherwise, we
	 * would simply get the same stack trace twice and mess up the output
	 * in the process, because call suspension is not expected if we are
	 * tracing a single process only.
	 * FIXME: the check should be for whether we actually print the call..
	 */
	if (show_stack && !(proc->trace_flags & TF_INCALL))
		kernel_put_stacktrace(proc);

	/*
	 * If this process is in the middle of a call, the signal will be
	 * printed within the call.  This will always happen on the call split,
	 * that is, between the call's entering (out) and leaving (in) phases.
	 * This also means that the recording of the call-enter phase may be
	 * replayed more than once, and the call may be suspended more than
	 * once--after all, a signal is not necessarily followed immediately
	 * by the call result.  If the process is not in the middle of a call,
	 * the signal will end up on a separate line.  In both cases, multiple
	 * consecutive signals may be printed right after one another.  The
	 * following scenario shows a number of possible combinations:
	 *
	 *       2| foo(<..>
	 *       3| ** SIGHUP ** ** SIGUSR1 **
	 *       3| bar() = <..>
	 *       2|*foo(** SIGUSR1 ** ** SIGUSR2 ** <..>
	 *       3|*bar() = ** SIGCHLD ** 0
	 *       2|*foo(** SIGINT ** &0xef852000) = -1 [EINTR]
	 *       3| kill(3, SIGTERM) = ** SIGTERM ** <..>
	 *       3| Process terminated from signal SIGTERM
	 */

	call_replay(proc);

	if (!valuesonly && (signame = get_signal_name(sig)) != NULL)
		put_fmt(proc, "** %s **", signame);
	else
		put_fmt(proc, "** SIGNAL %d **", sig);

	put_space(proc);

	output_flush();
}

/*
 * Wait for the given process ID to stop on the given signal.  Upon success,
 * the function will return zero.  Upon failure, it will return -1, and errno
 * will be either set to an error code, or to zero in order to indicate that
 * the process exited instead.
 */
static int
wait_sig(pid_t pid, int sig)
{
	int status;

	for (;;) {
		if (waitpid(pid, &status, 0) == -1) {
			if (errno == EINTR) continue;

			return -1;
		}

		if (!WIFSTOPPED(status)) {
			/* The process terminated just now. */
			errno = 0;

			return -1;
		}

		if (WSTOPSIG(status) == sig)
			break;

		(void)ptrace(T_RESUME, pid, 0, WSTOPSIG(status));
	}

	return 0;
}

/*
 * Attach to the given process, and wait for the resulting SIGSTOP signal.
 * Other signals may arrive first; we pass these on to the process without
 * reporting them, thus logically modelling them as having arrived before we
 * attached to the process.  The process might also exit in the meantime,
 * typically as a result of a lethal signal; following the same logical model,
 * we pretend the process did not exist in the first place.  Since the SIGSTOP
 * signal will be pending right after attaching to the process, this procedure
 * will never block.
 */
static int
attach(pid_t pid)
{

	if (ptrace(T_ATTACH, pid, 0, 0) != 0) {
		warn("Unable to attach to pid %d", pid);

		return -1;
	}

	if (wait_sig(pid, SIGSTOP) != 0) {
		/* If the process terminated, report it as not found. */
		if (errno == 0)
			errno = ESRCH;

		warn("Unable to attach to pid %d", pid);

		return -1;
	}

	/* Verify that we can read values from the kernel at all. */
	if (kernel_check(pid) == FALSE) {
		(void)ptrace(T_DETACH, pid, 0, 0);

		warnx("Kernel magic check failed, recompile trace(1)");

		return -1;
	}

	/*
	 * System services are managed by RS, which prevents them from
	 * being traced properly by PM.  Attaching to a service could
	 * therefore cause problems, so we should detach immediately.
	 */
	if (kernel_is_service(pid) == TRUE) {
		(void)ptrace(T_DETACH, pid, 0, 0);

		warnx("Cannot attach to system services!");

		return -1;
	}

	return 0;
}

/*
 * Detach from all processes, knowning that they were all processes to which we
 * attached explicitly (i.e., not started by us) and are all currently stopped.
 */
static void
detach_stopped(void)
{
	struct trace_proc *proc;

	for (proc = proc_next(NULL); proc != NULL; proc = proc_next(proc))
		(void)ptrace(T_DETACH, proc->pid, 0, 0);
}

/*
 * Start detaching from all processes to which we previously attached.  The
 * function is expected to return before detaching is completed, and the caller
 * must deal with the new situation appropriately.  Do not touch any processes
 * started by us (to allow graceful termination), unless force is set, in which
 * case those processes are killed.
 */
static void
detach_running(int force)
{
	struct trace_proc *proc;

	for (proc = proc_next(NULL); proc != NULL; proc = proc_next(proc)) {
		if (proc->trace_flags & TF_ATTACH) {
			/* Already detaching?  Then do nothing. */
			if (proc->trace_flags & TF_DETACH)
				continue;

			if (!(proc->trace_flags & TF_STOPPING))
				(void)kill(proc->pid, SIGSTOP);

			proc->trace_flags |= TF_DETACH | TF_STOPPING;
		} else {
			/*
			 * The child processes may be ignoring SIGINTs, so upon
			 * the second try, force them to terminate.
			 */
			if (force)
				(void)kill(proc->pid, SIGKILL);
		}
	}
}

/*
 * Print command usage.
 */
static void __dead
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-fgNstVv] [-o file] [-p pid] "
	    "[command]\n", getprogname());

	exit(EXIT_FAILURE);
}

/*
 * The main function of the system call tracer.
 */
int
main(int argc, char * argv[])
{
	struct trace_proc *proc;
	const char *output_file;
	int status, sig, follow_fork, show_stack, grouping, first_signal;
	pid_t pid, last_pid;
	int c, error;

	setprogname(argv[0]);

	proc_init();

	follow_fork = FALSE;
	show_stack = FALSE;
	grouping = FALSE;
	output_file = NULL;

	timestamps = 0;
	allnames = FALSE;
	verbose = 0;
	valuesonly = 0;

	while ((c = getopt(argc, argv, "fgNstVvo:p:")) != -1) {
		switch (c) {
		case 'f':
			follow_fork = TRUE;
			break;
		case 'g':
			grouping = TRUE;
			break;
		case 'N':
			allnames = TRUE;
			break;
		case 's':
			show_stack = TRUE;
			break;
		case 't':
			timestamps++;
			break;
		case 'V':
			valuesonly++;
			break;
		case 'v':
			verbose++;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'p':
			pid = atoi(optarg);
			if (pid <= 0)
				usage();

			if (proc_get(pid) == NULL && proc_add(pid) == NULL)
				err(EXIT_FAILURE, NULL);

			break;
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	first_signal = TRUE;
	got_signal = FALSE;
	got_info = FALSE;

	signal(SIGINT, sig_handler);
	signal(SIGINFO, info_handler);

	/* Attach to any processes for which PIDs were given. */
	for (proc = proc_next(NULL); proc != NULL; proc = proc_next(proc)) {
		if (attach(proc->pid) != 0) {
			/*
			 * Detach from the processes that we have attached to
			 * so far, i.e. the ones with the TF_ATTACH flag.
			 */
			detach_stopped();

			return EXIT_FAILURE;
		}

		proc->trace_flags = TF_ATTACH | TF_NOCALL;
	}

	/* If a command is given, start a child that executes the command. */
	if (argc >= 1) {
		pid = fork();

		switch (pid) {
		case -1:
			warn("Unable to fork");

			detach_stopped();

			return EXIT_FAILURE;

		case 0:
			(void)ptrace(T_OK, 0, 0, 0);

			(void)execvp(argv[0], argv);

			err(EXIT_FAILURE, "Unable to start %s", argv[0]);

		default:
			break;
		}

		/*
		 * The first signal will now be SIGTRAP from the execvp(),
		 * unless that fails, in which case the child will terminate.
		 */
		if (wait_sig(pid, SIGTRAP) != 0) {
			/*
			 * If the child exited, the most likely cause is a
			 * failure to execute the command.  Let the child
			 * report the error, and do not say anything here.
			 */
			if (errno != 0)
				warn("Unable to start process");

			detach_stopped();

			return EXIT_FAILURE;
		}

		/* If we haven't already, perform the kernel magic check. */
		if (proc_count() == 0 && kernel_check(pid) == FALSE) {
			warnx("Kernel magic check failed, recompile trace(1)");

			(void)kill(pid, SIGKILL);

			detach_stopped();

			return EXIT_FAILURE;
		}

		if ((proc = proc_add(pid)) == NULL) {
			warn(NULL);

			(void)kill(pid, SIGKILL);

			detach_stopped();

			return EXIT_FAILURE;
		}

		proc->trace_flags = 0;
	} else
		pid = -1;

	/* The user will have to give us at least one process to trace. */
	if (proc_count() == 0)
		usage();

	/*
	 * Open an alternative output file if needed.  After that, standard
	 * error should no longer be used directly, and all output has to go
	 * through the output module.
	 */
	if (output_init(output_file) < 0) {
		warn("Unable to open output file");

		if (pid > 0)
			(void)kill(pid, SIGKILL);

		detach_stopped();

		return EXIT_FAILURE;
	}

	/*
	 * All the traced processes are currently stopped.  Initialize, report,
	 * and resume them.
	 */
	for (proc = proc_next(NULL); proc != NULL; proc = proc_next(proc)) {
		new_proc(proc, follow_fork);

		(void)ptrace(T_SYSCALL, proc->pid, 0, 0);
	}

	/*
	 * Handle events until there are no traced processes left.
	 */
	last_pid = 0;
	error = FALSE;

	for (;;) {
		/* If an output error occurred, exit as soon as possible. */
		if (!error && output_error()) {
			detach_running(TRUE /*force*/);

			error = TRUE;
		}

		/*
		 * If the user pressed ^C once, start detaching the processes
		 * that we did not start, if any.  If the user pressed ^C
		 * twice, kill the process that we did start, if any.
		 */
		if (got_signal) {
			detach_running(!first_signal);

			got_signal = FALSE;
			first_signal = FALSE;
		}

		/* Upon getting SIGINFO, print a list of traced processes. */
		if (got_info) {
			list_info();

			got_info = FALSE;
		}

		/*
		 * Block until something happens to a traced process.  If
		 * enabled from the command line, first try waiting for the
		 * last process for which we got results, so as to reduce call
		 * suspensions a bit.
		 */
		if (grouping && last_pid > 0 &&
		    waitpid(last_pid, &status, WNOHANG) > 0)
			pid = last_pid;
		else
		    if ((pid = waitpid(-1, &status, 0)) <= 0) {
			if (pid == -1 && errno == EINTR) continue;
			if (pid == -1 && errno == ECHILD) break; /* all done */

			put_fmt(NULL, "Unexpected waitpid failure: %s",
			    (pid == 0) ? "No result" : strerror(errno));
			put_newline();

			/*
			 * We need waitpid to function correctly in order to
			 * detach from any attached processes, so we can do
			 * little more than just exit, effectively killing all
			 * traced processes.
			 */
			return EXIT_FAILURE;
		}

		last_pid = 0;

		/* Get the trace data structure for the process. */
		if ((proc = proc_get(pid)) == NULL) {
			/*
			 * The waitpid() call returned the status of a process
			 * that we have not yet seen.  This must be a newly
			 * forked child.  If it is not stopped, it must have
			 * died immediately, and we choose not to report it.
			 */
			if (!WIFSTOPPED(status))
				continue;

			if ((proc = proc_add(pid)) == NULL) {
				put_fmt(NULL,
				    "Error attaching to new child %d: %s",
				    pid, strerror(errno));
				put_newline();

				/*
				 * Out of memory allocating a new child object!
				 * We can not trace this child, so just let it
				 * run free by detaching from it.
				 */
				if (WSTOPSIG(status) != SIGSTOP) {
					(void)ptrace(T_RESUME, pid, 0,
					    WSTOPSIG(status));

					if (wait_sig(pid, SIGSTOP) != 0)
						continue; /* it died.. */
				}

				(void)ptrace(T_DETACH, pid, 0, 0);

				continue;
			}

			/*
			 * We must specify TF_ATTACH here, even though it may
			 * be a child of a process we started, in which case it
			 * should be killed when we exit.  We do not keep track
			 * of ancestry though, so better safe than sorry.
			 */
			proc->trace_flags = TF_ATTACH | TF_STOPPING;

			new_proc(proc, follow_fork);

			/* Repeat entering the fork call for the child. */
			handle_call(proc, show_stack);
		}

		/* If the process died, report its status and clean it up. */
		if (!WIFSTOPPED(status)) {
			discard_proc(proc, status);

			continue;
		}

		sig = WSTOPSIG(status);

		if (sig == SIGSTOP && (proc->trace_flags & TF_STOPPING)) {
			/* We expected the process to be stopped; now it is. */
			proc->trace_flags &= ~TF_STOPPING;

			if (proc->trace_flags & TF_DETACH) {
				if (ptrace(T_DETACH, proc->pid, 0, 0) == 0)
					discard_proc(proc, status);

				/*
				 * If detaching failed, the process must have
				 * died, and we'll get notified through wait().
				 */
				continue;
			}

			sig = 0;
		} else if (sig == SIGSTOP && (proc->trace_flags & TF_EXEC)) {
			/* The process has performed a successful execve(). */
			call_leave(proc, TRUE /*skip*/);

			put_text(proc, "---");

			new_exec(proc);

			/*
			 * A successful execve() has no result, in the sense
			 * that there is no reply message.  We should therefore
			 * not even try to copy in the reply message from the
			 * original location, because it will be invalid.
			 * Thus, we skip the exec's call leave phase entirely.
			 */
			proc->trace_flags &= ~TF_EXEC;
			proc->trace_flags |= TF_SKIP;

			sig = 0;
		} else if (sig == SIGTRAP) {
			/* The process is entering or leaving a system call. */
			if (!(proc->trace_flags & TF_DETACH))
				handle_call(proc, show_stack);

			sig = 0;
		} else {
			/* The process has received a signal. */
			report_signal(proc, sig, show_stack);

			/*
			 * Only in this case do we pass the signal to the
			 * traced process.
			 */
		}

		/*
		 * Resume process execution.  If this call fails, the process
		 * has probably died.  We will find out soon enough.
		 */
		(void)ptrace(T_SYSCALL, proc->pid, 0, sig);

		last_pid = proc->pid;
	}

	return (error) ? EXIT_FAILURE : EXIT_SUCCESS;
}
