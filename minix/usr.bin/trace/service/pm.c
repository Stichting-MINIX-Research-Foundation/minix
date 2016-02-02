
#include "inc.h"

#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/reboot.h>
#include <minix/profile.h>

static int
pm_exit_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "status", "%d", m_out->m_lc_pm_exit.status);

	return CT_NORETURN;
}

static const struct flags wait4_options[] = {
	FLAG(WNOHANG),
	FLAG(WUNTRACED),
	FLAG(WALTSIG),
	FLAG(WALLSIG),
	FLAG(WNOWAIT),
	FLAG(WNOZOMBIE),
	FLAG(WOPTSCHECKED),
};

static void
put_wait4_status(struct trace_proc * proc, const char * name, int status)
{
	const char *signame;
	int sig;

	/*
	 * There is no suitable set of macros to be used here, so we're going
	 * to invent our own: W_EXITED, W_SIGNALED, and W_STOPPED.  Hopefully
	 * they are sufficiently clear even though they don't actually exist.
	 * The code below is downright messy, but it also ensures that no bits
	 * are set unexpectedly in the status.
	 */
	if (!valuesonly && WIFEXITED(status) &&
	    status == W_EXITCODE(WEXITSTATUS(status), 0)) {
		put_value(proc, name, "W_EXITED(%d)",
		    WEXITSTATUS(status));

		return;
	}

	/* WCOREDUMP() actually returns WCOREFLAG or 0, but better safe.. */
	if (!valuesonly && WIFSIGNALED(status) && status == (W_EXITCODE(0,
	    WTERMSIG(status)) | (WCOREDUMP(status) ? WCOREFLAG : 0))) {
		sig = WTERMSIG(status);

		if ((signame = get_signal_name(sig)) != NULL)
			put_value(proc, name, "W_SIGNALED(%s)", signame);
		else
			put_value(proc, name, "W_SIGNALED(%u)", sig);

		if (WCOREDUMP(status))
			put_text(proc, "|WCOREDUMP");

		return;
	}

	if (!valuesonly && WIFSTOPPED(status) &&
	    status == W_STOPCODE(WSTOPSIG(status))) {
		sig = WSTOPSIG(status);

		if ((signame = get_signal_name(sig)) != NULL)
			put_value(proc, name, "W_STOPPED(%s)", signame);
		else
			put_value(proc, name, "W_STOPPED(%u)", sig);

		return;
	}

	/*
	 * If we get here, either valuesonly is enabled or the resulting status
	 * is not one we recognize, for example because extra bits are set.
	 */
	put_value(proc, name, "0x%04x", status);
}

static int
pm_wait4_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "pid", "%d", m_out->m_lc_pm_wait4.pid);

	return CT_NOTDONE;
}

static void
put_struct_rusage(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct rusage ru;

	if (!put_open_struct(proc, name, flags, addr, &ru, sizeof(ru)))
		return;

	put_struct_timeval(proc, "ru_utime", PF_LOCADDR,
	    (vir_bytes)&ru.ru_utime);
	put_struct_timeval(proc, "ru_stime", PF_LOCADDR,
	    (vir_bytes)&ru.ru_stime);

	if (verbose > 0) {
		put_value(proc, "ru_maxrss", "%ld", ru.ru_maxrss);
		put_value(proc, "ru_minflt", "%ld", ru.ru_minflt);
		put_value(proc, "ru_majflt", "%ld", ru.ru_majflt);
	}

	put_close_struct(proc, verbose > 0);
}

static void
pm_wait4_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	/*
	 * If the result is zero, there is no status to show.  Also, since the
	 * status is returned in the result message, we cannot print the user-
	 * given pointer.  Instead, upon failure we show "&.." to indicate an
	 * unknown pointer.
	 */
	if (!failed && m_in->m_type > 0)
		put_wait4_status(proc, "status", m_in->m_pm_lc_wait4.status);
	else
		put_field(proc, "status", "&..");
	put_flags(proc, "options", wait4_options, COUNT(wait4_options),
	    "0x%x", m_out->m_lc_pm_wait4.options);
	put_struct_rusage(proc, "rusage", failed, m_out->m_lc_pm_wait4.addr);
	put_equals(proc);
	put_result(proc);
}

static void
pm_getpid_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	put_result(proc);
	if (!failed) {
		put_open(proc, NULL, 0, "(", ", ");
		put_value(proc, "ppid", "%d", m_in->m_pm_lc_getpid.parent_pid);
		put_close(proc, ")");
	}
}

/* This function is shared between setuid and seteuid. */
static int
pm_setuid_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "uid", "%u", m_out->m_lc_pm_setuid.uid);

	return CT_DONE;
}

static void
pm_getuid_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	put_result(proc);
	if (!failed) {
		put_open(proc, NULL, 0, "(", ", ");
		put_value(proc, "euid", "%u", m_in->m_pm_lc_getuid.euid);
		put_close(proc, ")");
	}
}

static int
pm_stime_out(struct trace_proc * proc, const message * m_out)
{

	put_time(proc, "time", m_out->m_lc_pm_time.sec);

	return CT_DONE;
}

static void
put_signal(struct trace_proc * proc, const char * name, int sig)
{
	const char *signame;

	if (!valuesonly && (signame = get_signal_name(sig)) != NULL)
		put_field(proc, name, signame);
	else
		put_value(proc, name, "%d", sig);
}

static void
put_ptrace_req(struct trace_proc * proc, const char * name, int req)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (req) {
		TEXT(T_STOP);
		TEXT(T_OK);
		TEXT(T_ATTACH);
		TEXT(T_DETACH);
		TEXT(T_RESUME);
		TEXT(T_STEP);
		TEXT(T_SYSCALL);
		TEXT(T_EXIT);
		TEXT(T_GETINS);
		TEXT(T_GETDATA);
		TEXT(T_GETUSER);
		TEXT(T_SETINS);
		TEXT(T_SETDATA);
		TEXT(T_SETUSER);
		TEXT(T_SETOPT);
		TEXT(T_GETRANGE);
		TEXT(T_SETRANGE);
		TEXT(T_READB_INS);
		TEXT(T_WRITEB_INS);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", req);
}

static void
put_struct_ptrace_range(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct ptrace_range pr;

	if (!put_open_struct(proc, name, flags, addr, &pr, sizeof(pr)))
		return;

	if (!valuesonly && pr.pr_space == TS_INS)
		put_field(proc, "pr_space", "TS_INS");
	else if (!valuesonly && pr.pr_space == TS_DATA)
		put_field(proc, "pr_space", "TS_DATA");
	else
		put_value(proc, "pr_space", "%d", pr.pr_space);
	put_value(proc, "pr_addr", "0x%lx", pr.pr_addr);
	put_ptr(proc, "pr_ptr", (vir_bytes)pr.pr_ptr);
	put_value(proc, "pr_size", "%zu", pr.pr_size);

	put_close_struct(proc, TRUE /*all*/);
}

static int
pm_ptrace_out(struct trace_proc * proc, const message * m_out)
{

	put_ptrace_req(proc, "req", m_out->m_lc_pm_ptrace.req);
	put_value(proc, "pid", "%d", m_out->m_lc_pm_ptrace.pid);

	switch (m_out->m_lc_pm_ptrace.req) {
	case T_GETINS:
	case T_GETDATA:
	case T_GETUSER:
	case T_READB_INS:
		put_value(proc, "addr", "0x%lx", m_out->m_lc_pm_ptrace.addr);
		put_value(proc, "data", "%ld", m_out->m_lc_pm_ptrace.data);
		break;
	case T_SETINS:
	case T_SETDATA:
	case T_SETUSER:
	case T_WRITEB_INS:
		put_value(proc, "addr", "0x%lx", m_out->m_lc_pm_ptrace.addr);
		put_value(proc, "data", "0x%lx", m_out->m_lc_pm_ptrace.data);
		break;
	case T_RESUME:
	case T_STEP:
	case T_SYSCALL:
		put_value(proc, "addr", "%ld", m_out->m_lc_pm_ptrace.addr);
		put_signal(proc, "data", m_out->m_lc_pm_ptrace.data);
		break;
	case T_GETRANGE:
	case T_SETRANGE:
		put_struct_ptrace_range(proc, "addr", 0,
		    m_out->m_lc_pm_ptrace.addr);
		put_value(proc, "data", "%ld", m_out->m_lc_pm_ptrace.data);
		break;
	default:
		put_value(proc, "addr", "%ld", m_out->m_lc_pm_ptrace.addr);
		put_value(proc, "data", "%ld", m_out->m_lc_pm_ptrace.data);
		break;
	}

	return CT_DONE;
}

static void
pm_ptrace_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	if (!failed) {
		switch (m_out->m_lc_pm_ptrace.req) {
		case T_GETINS:
		case T_GETDATA:
		case T_GETUSER:
		case T_READB_INS:
			put_value(proc, NULL, "0x%lx",
			    m_in->m_pm_lc_ptrace.data);
			return;
		}
	}

	put_result(proc);
}

void
put_groups(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, int count)
{
	gid_t groups[NGROUPS_MAX];
	int i;

	if ((flags & PF_FAILED) || valuesonly || count < 0 ||
	    count > NGROUPS_MAX || (count > 0 && mem_get_data(proc->pid, addr,
	    groups, count * sizeof(groups[0])) < 0)) {
		if (flags & PF_LOCADDR)
			put_field(proc, name, "&..");
		else
			put_ptr(proc, name, addr);

		return;
	}

	put_open(proc, name, PF_NONAME, "[", ", ");
	for (i = 0; i < count; i++)
		put_value(proc, NULL, "%u", groups[i]);
	put_close(proc, "]");
}

static int
pm_setgroups_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "ngroups", "%d", m_out->m_lc_pm_groups.num);
	put_groups(proc, "grouplist", 0, m_out->m_lc_pm_groups.ptr,
	    m_out->m_lc_pm_groups.num);

	return CT_DONE;
}

static int
pm_getgroups_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "ngroups", "%d", m_out->m_lc_pm_groups.num);

	return CT_NOTDONE;
}

static void
pm_getgroups_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_groups(proc, "grouplist", failed, m_out->m_lc_pm_groups.ptr,
	    m_in->m_type);
	put_equals(proc);
	put_result(proc);
}

static int
pm_kill_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "pid", "%d", m_out->m_lc_pm_sig.pid);
	put_signal(proc, "sig", m_out->m_lc_pm_sig.nr);

	return CT_DONE;
}

/* This function is shared between setgid and setegid. */
static int
pm_setgid_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "gid", "%u", m_out->m_lc_pm_setgid.gid);

	return CT_DONE;
}

static void
pm_getgid_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	put_result(proc);
	if (!failed) {
		put_open(proc, NULL, 0, "(", ", ");
		put_value(proc, "egid", "%u", m_in->m_pm_lc_getgid.egid);
		put_close(proc, ")");
	}
}

static int
put_frame_string(struct trace_proc * proc, vir_bytes frame, size_t len,
	vir_bytes addr)
{
	vir_bytes stacktop, offset;

	/*
	 * The addresses in the frame assume that the process has already been
	 * changed, and the top of the frame is now located at the new process
	 * stack top, which is a hardcoded system-global value.  In order to
	 * print the strings, we must convert back each address to its location
	 * within the given frame.
	 */
	stacktop = kernel_get_stacktop();

	if (addr >= stacktop)
		return FALSE;
	offset = stacktop - addr;
	if (offset >= len)
		return FALSE;
	addr = frame + len - offset;

	/*
	 * TODO: while using put_buf() is highly convenient, it does require at
	 * least one copy operation per printed string.  The strings are very
	 * likely to be consecutive in memory, so copying in larger chunks at
	 * once would be preferable.  Also, if copying from the frame fails,
	 * put_buf() will print the string address as we corrected it above,
	 * rather than the address as found in the frame.  A copy failure would
	 * always be a case of malice on the traced process's behalf, though.
	 */
	put_buf(proc, NULL, PF_STRING, addr, len - offset);

	return TRUE;
}

/*
 * Print the contents of the exec frame, which includes both pointers and
 * actual string data for the arguments and environment variables to be used.
 * Even though we know that the entire frame is not going to exceed ARG_MAX
 * bytes, this is too large a size for a static buffer, and we'd like to avoid
 * allocating large dynamic buffers as well.  The situation is complicated by
 * the fact that any string in the frame may run up to the end of the frame.
 */
static void
put_exec_frame(struct trace_proc * proc, vir_bytes addr, size_t len)
{
	void *argv[64];
	size_t off, chunk;
	unsigned int i, count, max, argv_max, envp_max;
	int first, ok, nulls;

	if (valuesonly) {
		put_ptr(proc, "frame", addr);
		put_value(proc, "framelen", "%zu", len);

		return;
	}

	if (verbose == 0) {
		argv_max = 16;
		envp_max = 0;
	} else if (verbose == 1)
		argv_max = envp_max = 64;
	else
		argv_max = envp_max = INT_MAX;

	off = sizeof(int); /* skip 'argc' at the start of the frame */
	first = TRUE;
	ok = TRUE;
	nulls = 0;
	count = 0;
	max = argv_max;

	do {
		chunk = sizeof(argv);
		if (chunk > len - off)
			chunk = len - off;

		if (mem_get_data(proc->pid, addr + off, argv, chunk) != 0)
			break;

		if (first) {
			put_open(proc, "argv", PF_NONAME, "[", ", ");

			first = FALSE;
		}

		for (i = 0; i < chunk / sizeof(void *) && ok; i++) {
			if (argv[i] == NULL) {
				if (count > max)
					put_tail(proc, count, max);
				put_close(proc, "]");
				if (nulls++ == 0) {
					put_open(proc, "envp", PF_NONAME, "[",
					    ", ");
					count = 0;
					max = envp_max;
				} else
					break; /* two NULL pointers: done! */
			} else if (count++ < max)
				ok = put_frame_string(proc, addr, len,
				    (vir_bytes)argv[i]);
		}

		off += chunk;
	} while (nulls < 2 && ok);

	/*
	 * Handle failure cases, implied by not reaching the second NULL
	 * in the array.  Successful completion is handled in the loop above.
	 * Note that 'ok' is not always cleared on failure, as it is used only
	 * to break out of the outer loop.
	 */
	if (first) {
		put_ptr(proc, "argv", addr + off);
		put_field(proc, "envp", "&..");
	} else if (nulls < 2) {
		put_tail(proc, 0, 0);
		put_close(proc, "]");
		if (nulls < 1) {
			put_open(proc, "envp", PF_NONAME, "[", ", ");
			put_tail(proc, 0, 0);
			put_close(proc, "]");
		}
	}
}

static int
pm_exec_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_pm_exec.name,
	    m_out->m_lc_pm_exec.namelen);
	put_exec_frame(proc, m_out->m_lc_pm_exec.frame,
	    m_out->m_lc_pm_exec.framelen);

	return CT_NORETURN;
}

/* The idea is that this function may one day print a human-readable time. */
void
put_time(struct trace_proc * proc, const char * name, time_t time)
{

	put_value(proc, name, "%"PRId64, time);
}

void
put_struct_timeval(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct timeval tv;

	/* No field names; they just make things harder to read. */
	if (!put_open_struct(proc, name, flags | PF_NONAME, addr, &tv,
	    sizeof(tv)))
		return;

	if (flags & PF_ALT)
		put_time(proc, "tv_sec", tv.tv_sec);
	else
		put_value(proc, "tv_sec", "%"PRId64, tv.tv_sec);
	put_value(proc, "tv_usec", "%d", tv.tv_usec);

	put_close_struct(proc, TRUE /*all*/);
}

static void
put_struct_itimerval(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct itimerval it;

	/*
	 * This used to pass PF_NONAME, but the layout may not be clear enough
	 * without names.  It does turn simple alarm(1) calls into rather
	 * lengthy output, though.
	 */
	if (!put_open_struct(proc, name, flags, addr, &it, sizeof(it)))
		return;

	put_struct_timeval(proc, "it_interval", PF_LOCADDR,
	    (vir_bytes)&it.it_interval);
	put_struct_timeval(proc, "it_value", PF_LOCADDR,
	    (vir_bytes)&it.it_value);

	put_close_struct(proc, TRUE /*all*/);
}

static void
put_itimer_which(struct trace_proc * proc, const char * name, int which)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (which) {
		TEXT(ITIMER_REAL);
		TEXT(ITIMER_VIRTUAL);
		TEXT(ITIMER_PROF);
		TEXT(ITIMER_MONOTONIC);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", which);
}

static const char *
pm_itimer_name(const message * m_out)
{

	return (m_out->m_lc_pm_itimer.value != 0) ? "setitimer" : "getitimer";
}

static int
pm_itimer_out(struct trace_proc * proc, const message * m_out)
{

	put_itimer_which(proc, "which", m_out->m_lc_pm_itimer.which);
	if (m_out->m_lc_pm_itimer.value != 0) {
		put_struct_itimerval(proc, "value", 0,
		    m_out->m_lc_pm_itimer.value);

		/*
		 * If there will be no old values to print, finish the call
		 * now.  For setitimer only; getitimer may not pass NULL.
		 */
		if (m_out->m_lc_pm_itimer.ovalue == 0) {
			put_ptr(proc, "ovalue", 0);

			return CT_DONE;
		}
	}

	return CT_NOTDONE;
}

static void
pm_itimer_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	if (m_out->m_lc_pm_itimer.value == 0 ||
	    m_out->m_lc_pm_itimer.ovalue != 0) {
		put_struct_itimerval(proc,
		    (m_out->m_lc_pm_itimer.value != 0) ? "ovalue" : "value",
		    failed, m_out->m_lc_pm_itimer.ovalue);
		put_equals(proc);
	}
	put_result(proc);
}

static void
put_struct_mcontext(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	mcontext_t ctx;

	if (!put_open_struct(proc, name, flags, addr, &ctx, sizeof(ctx)))
		return;

	/*
	 * TODO: print actual fields.  Then again, the ones that are saved and
	 * restored (FPU state) are hardly interesting enough to print..
	 */

	put_close_struct(proc, FALSE /*all*/);
}

static int
pm_getmcontext_out(struct trace_proc * proc, const message * m_out)
{

	return CT_NOTDONE;
}

static void
pm_getmcontext_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_struct_mcontext(proc, "mcp", failed, m_out->m_lc_pm_mcontext.ctx);
	put_equals(proc);
	put_result(proc);
}

static int
pm_setmcontext_out(struct trace_proc * proc, const message * m_out)
{

	put_struct_mcontext(proc, "mcp", 0, m_out->m_lc_pm_mcontext.ctx);

	return CT_DONE;
}

static void
put_sigset(struct trace_proc * proc, const char * name, sigset_t set)
{
	const char *signame;
	unsigned int count, unknown;
	int sig, invert;

	/*
	 * First decide whether we should print a normal or an inverted mask.
	 * Unfortunately, depending on the place, a filled set may or may not
	 * have bits outside the 1..NSIG range set.  Therefore, we ignore the
	 * bits outside this range entirely, and use simple heuristics to
	 * decide whether to show an inverted set.  If we know all the signal
	 * names for either set and not the other, show that one; otherwise,
	 * show an inverted mask if at least 3/4th of the bits are set.
	 */
	count = 0;
	unknown = 0;
	for (sig = 1; sig < NSIG; sig++) {
		if (sigismember(&set, sig))
			count++;
		if (get_signal_name(sig) == NULL)
			unknown |= 1 << !!sigismember(&set, sig);
	}
	if (unknown == 1 /*for unset bit*/ || unknown == 2 /*for set bit*/)
		invert = unknown - 1;
	else
		invert = (count >= (NSIG - 1) * 3 / 4);

	put_open(proc, name, PF_NONAME, invert ? "~[" : "[", " ");

	for (sig = 1; sig < NSIG; sig++) {
		/* Note that sigismember() may not strictly return 0 or 1.. */
		if (!sigismember(&set, sig) != invert)
			continue;

		if ((signame = get_signal_name(sig)) != NULL) {
			/* Skip the "SIG" prefix for brevity. */
			if (!strncmp(signame, "SIG", 3))
				put_field(proc, NULL, &signame[3]);
			else
				put_field(proc, NULL, signame);
		} else
			put_value(proc, NULL, "%d", sig);
	}

	put_close(proc, "]");
}

static const struct flags sa_flags[] = {
	FLAG(SA_ONSTACK),
	FLAG(SA_RESTART),
	FLAG(SA_RESETHAND),
	FLAG(SA_NODEFER),
	FLAG(SA_NOCLDSTOP),
	FLAG(SA_NOCLDWAIT),
#ifdef SA_SIGINFO
	FLAG(SA_SIGINFO),
#endif
	FLAG(SA_NOKERNINFO)
};

static void
put_sa_handler(struct trace_proc * proc, const char * name, vir_bytes handler)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch ((int)handler) {
		case (int)SIG_DFL: text = "SIG_DFL"; break;
		case (int)SIG_IGN: text = "SIG_IGN"; break;
		case (int)SIG_HOLD: text = "SIG_HOLD"; break;
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_ptr(proc, name, handler);
}

static void
put_struct_sigaction(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct sigaction sa;

	if (!put_open_struct(proc, name, flags, addr, &sa, sizeof(sa)))
		return;

	put_sa_handler(proc, "sa_handler", (vir_bytes)sa.sa_handler);

	if (verbose > 1)
		put_sigset(proc, "sa_mask", sa.sa_mask);

	/* A somewhat lame attempt to reduce noise a bit. */
	if ((sa.sa_flags & ~(SA_ONSTACK | SA_RESTART | SA_RESETHAND |
	    SA_NODEFER)) != 0 || sa.sa_handler != SIG_DFL || verbose > 0)
		put_flags(proc, "sa_flags", sa_flags, COUNT(sa_flags), "0x%x",
		    sa.sa_flags);

	put_close_struct(proc, verbose > 1);
}

static int
pm_sigaction_out(struct trace_proc * proc, const message * m_out)
{

	put_signal(proc, "signal", m_out->m_lc_pm_sig.nr);
	put_struct_sigaction(proc, "act", 0, m_out->m_lc_pm_sig.act);

	/* If there will be no old values to print, finish the call now. */
	if (m_out->m_lc_pm_sig.oact == 0) {
		put_ptr(proc, "oact", 0);
		return CT_DONE;
	} else
		return CT_NOTDONE;
}

static void
pm_sigaction_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	if (m_out->m_lc_pm_sig.oact != 0) {
		put_struct_sigaction(proc, "oact", failed,
		    m_out->m_lc_pm_sig.oact);
		put_equals(proc);
	}
	put_result(proc);
}

static int
pm_sigsuspend_out(struct trace_proc * proc, const message * m_out)
{

	put_sigset(proc, "set", m_out->m_lc_pm_sigset.set);

	return CT_DONE;
}

static int
pm_sigpending_out(struct trace_proc * __unused proc,
	const message * __unused m_out)
{

	return CT_NOTDONE;
}

static void
pm_sigpending_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_sigset(proc, "set", m_in->m_pm_lc_sigset.set);
	else
		put_field(proc, "set", "&..");
	put_equals(proc);
	put_result(proc);
}

static void
put_sigprocmask_how(struct trace_proc * proc, const char * name, int how)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (how) {
		case SIG_INQUIRE: /* pseudocode, print something else */
		TEXT(SIG_BLOCK);
		TEXT(SIG_UNBLOCK);
		TEXT(SIG_SETMASK);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", how);
}

static int
pm_sigprocmask_out(struct trace_proc * proc, const message * m_out)
{

	put_sigprocmask_how(proc, "how", m_out->m_lc_pm_sigset.how);
	if (m_out->m_lc_pm_sigset.how == SIG_INQUIRE)
		put_ptr(proc, "set", 0);
	else
		put_sigset(proc, "set", m_out->m_lc_pm_sigset.set);

	return CT_NOTDONE;
}

static void
pm_sigprocmask_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_sigset(proc, "oset", m_in->m_pm_lc_sigset.set);
	else
		put_field(proc, "oset", "&..");
	put_equals(proc);
	put_result(proc);
}

static int
pm_sigreturn_out(struct trace_proc * proc, const message * m_out)
{
	struct sigcontext scp;

	if (put_open_struct(proc, "scp", 0, m_out->m_lc_pm_sigset.ctx, &scp,
	    sizeof(scp))) {
		if (verbose == 1) {
#if defined(__i386__)
			put_ptr(proc, "sc_eip", scp.sc_eip);
			put_ptr(proc, "sc_esp", scp.sc_esp);
#elif defined(__arm__)
			put_ptr(proc, "sc_pc", scp.sc_pc);
			put_ptr(proc, "sc_usr_sp", scp.sc_usr_sp);
#endif
		}

		/*
		 * We deliberately print the signal set from the message rather
		 * than from the structure, since in theory they may be
		 * different and PM uses the one from the message only.
		 */
		put_sigset(proc, "sc_mask", m_out->m_lc_pm_sigset.set);

		/*
		 * TODO: print some other fields, although it is probably not
		 * useful to print all registers even with verbose > 1?
		 */
		put_close_struct(proc, FALSE /*all*/);
	}

	return CT_NORETURN;
}

static void
pm_sigreturn_in(struct trace_proc * proc, const message * __unused m_out,
	const message * __unused m_in, int failed)
{

	if (failed) {
		put_equals(proc);
		put_result(proc);
	}
}

static void
put_priority_which(struct trace_proc * proc, const char * name, int which)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (which) {
		TEXT(PRIO_PROCESS);
		TEXT(PRIO_PGRP);
		TEXT(PRIO_USER);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", which);
}

static int
pm_getpriority_out(struct trace_proc * proc, const message * m_out)
{

	put_priority_which(proc, "which", m_out->m_lc_pm_priority.which);
	put_value(proc, "who", "%d", m_out->m_lc_pm_priority.who);

	return CT_DONE;
}

static void
pm_getpriority_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_value(proc, NULL, "%d", m_in->m_type + PRIO_MIN);
	else
		put_result(proc);
}

static int
pm_setpriority_out(struct trace_proc * proc, const message * m_out)
{

	put_priority_which(proc, "which", m_out->m_lc_pm_priority.which);
	put_value(proc, "who", "%d", m_out->m_lc_pm_priority.who);
	put_value(proc, "prio", "%d", m_out->m_lc_pm_priority.prio);

	return CT_DONE;
}

static int
pm_gettimeofday_out(struct trace_proc * __unused proc,
	const message * __unused m_out)
{

	return CT_NOTDONE;
}

static void
put_timespec_as_timeval(struct trace_proc * proc, const char * name,
	time_t sec, long nsec)
{

	/* No field names within the structure. */
	put_open(proc, name, PF_NONAME, "{", ", ");

	put_time(proc, "tv_sec", sec);
	put_value(proc, "tv_usec", "%ld", nsec / 1000);

	put_close(proc, "}");
}

static void
pm_gettimeofday_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed) {
		/*
		 * The system call returns values which do not match the call
		 * being made, so just like libc, we have to correct..
		 */
		put_timespec_as_timeval(proc, "tp", m_in->m_pm_lc_time.sec,
		    m_in->m_pm_lc_time.nsec);
	} else
		put_field(proc, "tp", "&..");
	put_ptr(proc, "tzp", 0); /* not part of the system call (yet) */

	put_equals(proc);
	put_result(proc);
}

static int
pm_getsid_out(struct trace_proc * proc, const message * m_out)
{

	put_value(proc, "pid", "%d", m_out->m_lc_pm_getsid.pid);

	return CT_DONE;
}

static void
put_clockid(struct trace_proc * proc, const char * name, clockid_t clock_id)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (clock_id) {
		TEXT(CLOCK_REALTIME);
#ifdef CLOCK_VIRTUAL
		TEXT(CLOCK_VIRTUAL);
#endif
#ifdef CLOCK_PROF
		TEXT(CLOCK_PROF);
#endif
		TEXT(CLOCK_MONOTONIC);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", clock_id);
}

static void
put_clock_timespec(struct trace_proc * proc, const char * name, int flags,
	time_t sec, long nsec)
{

	if (flags & PF_FAILED) {
		put_field(proc, name, "&..");

		return;
	}

	/* No field names within the structure. */
	put_open(proc, name, PF_NONAME, "{", ", ");

	if (flags & PF_ALT)
		put_time(proc, "tv_sec", sec);
	else
		put_value(proc, "tv_sec", "%"PRId64, sec);
	put_value(proc, "tv_nsec", "%ld", nsec);

	put_close(proc, "}");
}

/* This function is shared between clock_getres and clock_gettime. */
static int
pm_clock_get_out(struct trace_proc * proc, const message * m_out)
{

	put_clockid(proc, "clock_id", m_out->m_lc_pm_time.clk_id);

	return CT_NOTDONE;
}

static void
pm_clock_getres_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	put_clock_timespec(proc, "res", failed, m_in->m_pm_lc_time.sec,
	    m_in->m_pm_lc_time.nsec);
	put_equals(proc);
	put_result(proc);
}

/*
 * Same as pm_clock_getres_in, but different field name and the option to print
 * at least some results as time strings (in the future).
 */
static void
pm_clock_gettime_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{
	int flags;

	flags = failed;
	if (m_out->m_lc_pm_time.clk_id == CLOCK_REALTIME)
		flags |= PF_ALT; /* TODO: make this print a time string. */

	put_clock_timespec(proc, "tp", flags, m_in->m_pm_lc_time.sec,
	    m_in->m_pm_lc_time.nsec);
	put_equals(proc);
	put_result(proc);
}

static const char *
pm_clock_settime_name(const message * m_out)
{

	if (m_out->m_lc_pm_time.now == 0)
		return "adjtime";
	else
		return "clock_settime";
}

static int
pm_clock_settime_out(struct trace_proc * proc, const message * m_out)
{
	int flags;

	/* These two calls just look completely different.. */
	if (m_out->m_lc_pm_time.now == 0) {
		put_timespec_as_timeval(proc, "delta", m_out->m_lc_pm_time.sec,
		    m_out->m_lc_pm_time.nsec);
		put_ptr(proc, "odelta", 0); /* not supported on MINIX3 */
	} else {
		flags = 0;
		if (m_out->m_lc_pm_time.clk_id == CLOCK_REALTIME)
			flags |= PF_ALT;
		put_clockid(proc, "clock_id", m_out->m_lc_pm_time.clk_id);
		put_clock_timespec(proc, "tp", flags, m_out->m_lc_pm_time.sec,
		    m_out->m_lc_pm_time.nsec);
	}

	return CT_DONE;
}

static int
pm_getrusage_out(struct trace_proc * proc, const message * m_out)
{

	if (!valuesonly && m_out->m_lc_pm_rusage.who == RUSAGE_SELF)
		put_field(proc, "who", "RUSAGE_SELF");
	else if (!valuesonly && m_out->m_lc_pm_rusage.who == RUSAGE_CHILDREN)
		put_field(proc, "who", "RUSAGE_CHILDREN");
	else
		put_value(proc, "who", "%d", m_out->m_lc_pm_rusage.who);

	return CT_NOTDONE;
}

static void
pm_getrusage_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_struct_rusage(proc, "rusage", failed, m_out->m_lc_pm_rusage.addr);
	put_equals(proc);
	put_result(proc);
}

static const struct flags reboot_flags[] = {
	FLAG_ZERO(RB_AUTOBOOT),
	FLAG(RB_ASKNAME),
	FLAG(RB_DUMP),
	FLAG_MASK(RB_POWERDOWN, RB_HALT),
	FLAG(RB_POWERDOWN),
	FLAG(RB_INITNAME),
	FLAG(RB_KDB),
	FLAG(RB_NOSYNC),
	FLAG(RB_RDONLY),
	FLAG(RB_SINGLE),
	FLAG(RB_STRING),
	FLAG(RB_USERCONF),
};

static int
pm_reboot_out(struct trace_proc * proc, const message * m_out)
{

	put_flags(proc, "how", reboot_flags, COUNT(reboot_flags), "0x%x",
	    m_out->m_lc_pm_reboot.how);
	put_ptr(proc, "bootstr", 0); /* not supported on MINIX3 */

	return CT_DONE;
}

static int
pm_svrctl_out(struct trace_proc * proc, const message * m_out)
{

	put_ioctl_req(proc, "request", m_out->m_lc_svrctl.request,
	    TRUE /*is_svrctl*/);
	return put_ioctl_arg_out(proc, "arg", m_out->m_lc_svrctl.request,
	    m_out->m_lc_svrctl.arg, TRUE /*is_svrctl*/);
}

static void
pm_svrctl_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_ioctl_arg_in(proc, "arg", failed, m_out->m_lc_svrctl.request,
	    m_out->m_lc_svrctl.arg, TRUE /*is_svrctl*/);
}

static int
pm_sprof_out(struct trace_proc * proc, const message * m_out)
{
	int freq;

	if (!valuesonly && m_out->m_lc_pm_sprof.action == PROF_START)
		put_field(proc, "action", "PROF_START");
	else if (!valuesonly && m_out->m_lc_pm_sprof.action == PROF_STOP)
		put_field(proc, "action", "PROF_STOP");
	else
		put_value(proc, "action", "%d", m_out->m_lc_pm_sprof.action);

	put_value(proc, "size", "%zu", m_out->m_lc_pm_sprof.mem_size);

	freq = m_out->m_lc_pm_sprof.freq;
	if (!valuesonly && freq >= 3 && freq <= 15) /* no constants.. */
		put_value(proc, "freq", "%u /*%uHz*/", freq, 1 << (16 - freq));
	else
		put_value(proc, "freq", "%u", freq);

	if (!valuesonly && m_out->m_lc_pm_sprof.intr_type == PROF_RTC)
		put_field(proc, "type", "PROF_RTC");
	else if (!valuesonly && m_out->m_lc_pm_sprof.intr_type == PROF_NMI)
		put_field(proc, "type", "PROF_NMI");
	else
		put_value(proc, "type", "%d", m_out->m_lc_pm_sprof.intr_type);

	put_ptr(proc, "ctl_ptr", m_out->m_lc_pm_sprof.ctl_ptr);
	put_ptr(proc, "mem_ptr", m_out->m_lc_pm_sprof.mem_ptr);

	return CT_DONE;
}

#define PM_CALL(c) [((PM_ ## c) - PM_BASE)]

static const struct call_handler pm_map[] = {
	PM_CALL(EXIT) = HANDLER("exit", pm_exit_out, default_in),
	PM_CALL(FORK) = HANDLER("fork", default_out, default_in),
	PM_CALL(WAIT4) = HANDLER("wait4", pm_wait4_out, pm_wait4_in),
	PM_CALL(GETPID) = HANDLER("getpid", default_out, pm_getpid_in),
	PM_CALL(SETUID) = HANDLER("setuid", pm_setuid_out, default_in),
	PM_CALL(GETUID) = HANDLER("getuid", default_out, pm_getuid_in),
	PM_CALL(STIME) = HANDLER("stime", pm_stime_out, default_in),
	PM_CALL(PTRACE) = HANDLER("ptrace", pm_ptrace_out, pm_ptrace_in),
	PM_CALL(SETGROUPS) = HANDLER("setgroups", pm_setgroups_out,
	    default_in),
	PM_CALL(GETGROUPS) = HANDLER("getgroups", pm_getgroups_out,
	    pm_getgroups_in),
	PM_CALL(KILL) = HANDLER("kill", pm_kill_out, default_in),
	PM_CALL(SETGID) = HANDLER("setgid", pm_setgid_out, default_in),
	PM_CALL(GETGID) = HANDLER("getgid", default_out, pm_getgid_in),
	PM_CALL(EXEC) = HANDLER("execve", pm_exec_out, default_in),
	PM_CALL(SETSID) = HANDLER("setsid", default_out, default_in),
	PM_CALL(GETPGRP) = HANDLER("getpgrp", default_out, default_in),
	PM_CALL(ITIMER) = HANDLER_NAME(pm_itimer_name, pm_itimer_out,
	    pm_itimer_in),
	PM_CALL(GETMCONTEXT) = HANDLER("getmcontext", pm_getmcontext_out,
	    pm_getmcontext_in),
	PM_CALL(SETMCONTEXT) = HANDLER("setmcontext", pm_setmcontext_out,
	    default_in),
	PM_CALL(SIGACTION) = HANDLER("sigaction", pm_sigaction_out,
	    pm_sigaction_in),
	PM_CALL(SIGSUSPEND) = HANDLER("sigsuspend", pm_sigsuspend_out,
	    default_in),
	PM_CALL(SIGPENDING) = HANDLER("sigpending", pm_sigpending_out,
	    pm_sigpending_in),
	PM_CALL(SIGPROCMASK) = HANDLER("sigprocmask", pm_sigprocmask_out,
	    pm_sigprocmask_in),
	PM_CALL(SIGRETURN) = HANDLER("sigreturn", pm_sigreturn_out,
	    pm_sigreturn_in),
	PM_CALL(GETPRIORITY) = HANDLER("getpriority", pm_getpriority_out,
	    pm_getpriority_in),
	PM_CALL(SETPRIORITY) = HANDLER("setpriority", pm_setpriority_out,
	    default_in),
	PM_CALL(GETTIMEOFDAY) = HANDLER("gettimeofday", pm_gettimeofday_out,
	    pm_gettimeofday_in),
	PM_CALL(SETEUID) = HANDLER("seteuid", pm_setuid_out, default_in),
	PM_CALL(SETEGID) = HANDLER("setegid", pm_setgid_out, default_in),
	PM_CALL(ISSETUGID) = HANDLER("issetugid", default_out, default_in),
	PM_CALL(GETSID) = HANDLER("getsid", pm_getsid_out, default_in),
	PM_CALL(CLOCK_GETRES) = HANDLER("clock_getres", pm_clock_get_out,
	    pm_clock_getres_in),
	PM_CALL(CLOCK_GETTIME) = HANDLER("clock_gettime", pm_clock_get_out,
	    pm_clock_gettime_in),
	PM_CALL(CLOCK_SETTIME) = HANDLER_NAME(pm_clock_settime_name,
	    pm_clock_settime_out, default_in),
	PM_CALL(GETRUSAGE) = HANDLER("getrusage", pm_getrusage_out,
	    pm_getrusage_in),
	PM_CALL(REBOOT) = HANDLER("reboot", pm_reboot_out, default_in),
	PM_CALL(SVRCTL) = HANDLER("pm_svrctl", pm_svrctl_out, pm_svrctl_in),
	PM_CALL(SPROF) = HANDLER("sprofile", pm_sprof_out, default_in),
};

const struct calls pm_calls = {
	.endpt = PM_PROC_NR,
	.base = PM_BASE,
	.map = pm_map,
	.count = COUNT(pm_map)
};
