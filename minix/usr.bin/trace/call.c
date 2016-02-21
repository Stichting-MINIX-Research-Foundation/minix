
#include "inc.h"

#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>

static const struct calls *call_table[] = {
	&pm_calls,
	&vfs_calls,
	&rs_calls,
	&mib_calls,
	&vm_calls,
	&ipc_calls,
};

/*
 * Find a call handler for the given endpoint, call number pair.  Return NULL
 * if no call handler for this call exists.
 */
static const struct call_handler *
find_handler(endpoint_t endpt, int call_nr)
{
	unsigned int i, index;

	for (i = 0; i < COUNT(call_table); i++) {
		if (call_table[i]->endpt != ANY &&
		    call_table[i]->endpt != endpt)
			continue;

		if ((unsigned int)call_nr < call_table[i]->base)
			continue;

		index = (unsigned int)call_nr - call_table[i]->base;

		if (index >= call_table[i]->count)
			continue;

		if (call_table[i]->map[index].outfunc == NULL)
			continue;

		return &call_table[i]->map[index];
	}

	return NULL;
}

/*
 * Print an endpoint.
 */
void
put_endpoint(struct trace_proc * proc, const char * name, endpoint_t endpt)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (endpt) {
		TEXT(ASYNCM);
		TEXT(IDLE);
		TEXT(CLOCK);
		TEXT(SYSTEM);
		TEXT(KERNEL);
		TEXT(PM_PROC_NR);
		TEXT(VFS_PROC_NR);
		TEXT(RS_PROC_NR);
		TEXT(MEM_PROC_NR);
		TEXT(SCHED_PROC_NR);
		TEXT(TTY_PROC_NR);
		TEXT(DS_PROC_NR);
		TEXT(MIB_PROC_NR);
		TEXT(VM_PROC_NR);
		TEXT(PFS_PROC_NR);
		TEXT(ANY);
		TEXT(NONE);
		TEXT(SELF);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", endpt);
}

/*
 * Print a message structure.  The source field will be printed only if the
 * PF_ALT flag is given.
 */
static void
put_message(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	message m;

	if (!put_open_struct(proc, name, flags, addr, &m, sizeof(m)))
		return;

	if (flags & PF_ALT)
		put_endpoint(proc, "m_source", m.m_source);

	put_value(proc, "m_type", "0x%x", m.m_type);

	put_close_struct(proc, FALSE /*all*/);
}

/*
 * Print the call's equals sign, which also implies that the parameters part of
 * the call has been fully printed and the corresponding closing parenthesis
 * may have to be printed, if it has not been printed already.
 */
void
put_equals(struct trace_proc * proc)
{

	/*
	 * Do not allow multiple equals signs on a single line.  This check is
	 * protection against badly written handlers.  It does not work for the
	 * no-return type, but such calls are rare and less error prone anyway.
	 */
	assert((proc->call_flags & (CF_DONE | CF_NORETURN)) != CF_DONE);

	/*
	 * We allow (and in fact force) handlers to call put_equals in order to
	 * indicate that the call's parameters block has ended, so we must end
	 * the block here, if we hadn't done so before.
	 */
	if (!(proc->call_flags & CF_DONE)) {
		put_close(proc, ") ");

		proc->call_flags |= CF_DONE;
	}

	put_align(proc);
	put_text(proc, "= ");

	format_set_sep(proc, NULL);
}

/*
 * Print the primary result of a call, after the equals sign.  It is always
 * possible that this is an IPC-level or other low-level error, in which case
 * this takes precedence, which is why this function must be called to print
 * the result if the call failed in any way at all; it may or may not be used
 * if the call succeeded.  For regular call results, default MINIX3/POSIX
 * semantics are used: if the return value is negative, the actual call failed
 * with -1 and the negative return value is the call's error code.  The caller
 * may consider other cases a failure (e.g., waitpid() returning 0), but
 * negative return values *not* signifying an error are currently not supported
 * since they are not present in MINIX3.
 */
void
put_result(struct trace_proc * proc)
{
	const char *errname;
	int value;

	/* This call should always be preceded by a put_equals call. */
	assert(proc->call_flags & CF_DONE);

	/*
	 * If we failed to copy in the result register or message, print a
	 * basic error and nothing else.
	 */
	if (proc->call_flags & (CF_REG_ERR | CF_MSG_ERR)) {
		put_text(proc, "<fault>");

		return;
	}

	/*
	 * If we are printing a system call rather than an IPC call, and an
	 * error occurred at the IPC level, prefix the output with "<ipc>" to
	 * indicate the IPC failure.  If we are printing an IPC call, an IPC-
	 * level result is implied, so we do not print this.
	 */
	if (proc->call_handler != NULL && (proc->call_flags & CF_IPC_ERR))
		put_text(proc, "<ipc> ");

	value = proc->call_result;

	if (value >= 0)
		put_fmt(proc, "%d", value);
	else if (!valuesonly && (errname = get_error_name(-value)) != NULL)
		put_fmt(proc, "-1 [%s]", errname);
	else
		put_fmt(proc, "-1 [%d]", -value);

	format_set_sep(proc, " ");
}

/*
 * The default enter-call (out) printer, which prints no parameters and is thus
 * immediately done with printing parameters.
 */
int
default_out(struct trace_proc * __unused proc, const message * __unused m_out)
{

	return CT_DONE;
}

/*
 * The default leave-call (in) printer, which simply prints the call result,
 * possibly preceded by an equals sign if none was printed yet.  For obvious
 * reasons, if the handler's out printer returned CT_NOTDONE, this default
 * printer must not be used.
 */
void
default_in(struct trace_proc * proc, const message * __unused m_out,
	const message * __unused m_in, int __unused failed)
{

	if ((proc->call_flags & (CF_DONE | CF_NORETURN)) != CF_DONE)
		put_equals(proc);
	put_result(proc);
}

/*
 * Prepare a sendrec call, by copying in the request message, determining
 * whether it is one of the calls that the tracing engine should know about,
 * searching for a handler for the call, and returning a name for the call.
 */
static const char *
sendrec_prepare(struct trace_proc * proc, endpoint_t endpt, vir_bytes addr,
	int * trace_class)
{
	const char *name;
	int r;

	r = mem_get_data(proc->pid, addr, &proc->m_out, sizeof(proc->m_out));

	if (r == 0) {
		if (endpt == PM_PROC_NR) {
			if (proc->m_out.m_type == PM_EXEC)
				*trace_class = TC_EXEC;
			else if (proc->m_out.m_type == PM_SIGRETURN)
				*trace_class = TC_SIGRET;
		}

		proc->call_handler = find_handler(endpt, proc->m_out.m_type);
	} else
		proc->call_handler = NULL;

	if (proc->call_handler != NULL) {
		if (proc->call_handler->namefunc != NULL)
			name = proc->call_handler->namefunc(&proc->m_out);
		else
			name = proc->call_handler->name;

		assert(name != NULL);
	} else
		name = "ipc_sendrec";

	return name;
}

/*
 * Print the outgoing (request) part of a sendrec call.  If we found a call
 * handler for the call, let the handler generate output.  Otherwise, print the
 * sendrec call at the kernel IPC level.  Return the resulting call flags.
 */
static unsigned int
sendrec_out(struct trace_proc * proc, endpoint_t endpt, vir_bytes addr)
{

	if (proc->call_handler != NULL) {
		return proc->call_handler->outfunc(proc, &proc->m_out);
	} else {
		put_endpoint(proc, "src_dest", endpt);
		/*
		 * We have already copied in the message, but if we used m_out
		 * and PF_LOCADDR here, a copy failure would cause "&.." to be
		 * printed rather than the actual message address.
		 */
		put_message(proc, "m_ptr", 0, addr);

		return CT_DONE;
	}
}

/*
 * Print the incoming (reply) part of a sendrec call.  Copy in the reply
 * message, determine whether the call is considered to have failed, and let
 * the call handler do the rest.  If no call handler was found, print an
 * IPC-level result.
 */
static void
sendrec_in(struct trace_proc * proc, int failed)
{
	message m_in;

	if (failed) {
		/* The call failed at the IPC level. */
		memset(&m_in, 0, sizeof(m_in)); /* not supposed to be used */
		assert(proc->call_flags & CF_IPC_ERR);
	} else if (mem_get_data(proc->pid, proc->m_addr, &m_in,
	    sizeof(m_in)) != 0) {
		/* The reply message is somehow unavailable to us. */
		memset(&m_in, 0, sizeof(m_in)); /* not supposed to be used */
		proc->call_result = EGENERIC; /* not supposed to be used */
		proc->call_flags |= CF_MSG_ERR;
		failed = PF_FAILED;
	} else {
		/* The result is for the actual call. */
		proc->call_result = m_in.m_type;
		failed = (proc->call_result < 0) ? PF_FAILED : 0;
	}

	if (proc->call_handler != NULL)
		proc->call_handler->infunc(proc, &proc->m_out, &m_in, failed);
	else
		put_result(proc);
}

/*
 * Perform preparations for printing a system call.  Return two things: the
 * name to use for the call, and the trace class of the call.
 * special treatment).
 */
static const char *
call_prepare(struct trace_proc * proc, reg_t reg[3], int * trace_class)
{

	switch (proc->call_type) {
	case SENDREC:
		return sendrec_prepare(proc, (endpoint_t)reg[1],
		    (vir_bytes)reg[2], trace_class);

	case SEND:
		return "ipc_send";

	case SENDNB:
		return "ipc_sendnb";

	case RECEIVE:
		return "ipc_receive";

	case NOTIFY:
		return "ipc_notify";

	case SENDA:
		return "ipc_senda";

	case MINIX_KERNINFO:
		return "minix_kerninfo";

	default:
		/*
		 * It would be nice to include the call number here, but we
		 * must return a string that will last until the entire call is
		 * finished.  Adding another buffer to the trace_proc structure
		 * is an option, but it seems overkill..
		 */
		return "ipc_unknown";
	}
}

/*
 * Print the outgoing (request) part of a system call.  Return the resulting
 * call flags.
 */
static unsigned int
call_out(struct trace_proc * proc, reg_t reg[3])
{

	switch (proc->call_type) {
	case SENDREC:
		proc->m_addr = (vir_bytes)reg[2];

		return sendrec_out(proc, (endpoint_t)reg[1],
		    (vir_bytes)reg[2]);

	case SEND:
	case SENDNB:
		put_endpoint(proc, "dest", (endpoint_t)reg[1]);
		put_message(proc, "m_ptr", 0, (vir_bytes)reg[2]);

		return CT_DONE;

	case RECEIVE:
		proc->m_addr = (vir_bytes)reg[2];

		put_endpoint(proc, "src", (endpoint_t)reg[1]);

		return CT_NOTDONE;

	case NOTIFY:
		put_endpoint(proc, "dest", (endpoint_t)reg[1]);

		return CT_DONE;

	case SENDA:
		put_ptr(proc, "table", (vir_bytes)reg[2]);
		put_value(proc, "count", "%zu", (size_t)reg[1]);

		return CT_DONE;

	case MINIX_KERNINFO:
	default:
		return CT_DONE;
	}
}

/*
 * Print the incoming (reply) part of a call.
 */
static void
call_in(struct trace_proc * proc, int failed)
{

	switch (proc->call_type) {
	case SENDREC:
		sendrec_in(proc, failed);

		break;

	case RECEIVE:
		/* Print the source as well. */
		put_message(proc, "m_ptr", failed | PF_ALT, proc->m_addr);
		put_equals(proc);
		put_result(proc);

		break;

	case MINIX_KERNINFO:
		/*
		 * We do not have a platform-independent means to access the
		 * secondary IPC return value, so we cannot print the receive
		 * status or minix_kerninfo address.
		 */
		/* FALLTHROUGH */
	default:
		put_result(proc);

		break;
	}
}

/*
 * Determine whether to skip printing the given call, based on its name.
 */
static int
call_hide(const char * __unused name)
{

	/*
	 * TODO: add support for such filtering, with an strace-like -e command
	 * line option.  For now, we filter nothing, although calls may still
	 * be hidden as the result of a register retrieval error.
	 */
	return FALSE;
}

/*
 * The given process entered a system call.  Return the trace class of the
 * call: TC_EXEC for an execve() call, TC_SIGRET for a sigreturn() call, or
 * TC_NORMAL for a call that requires no exceptions in the trace engine.
 */
int
call_enter(struct trace_proc * proc, int show_stack)
{
	const char *name;
	reg_t reg[3];
	int trace_class, type;

	/* Get the IPC-level type and parameters of the system call. */
	if (kernel_get_syscall(proc->pid, reg) < 0) {
		/*
		 * If obtaining the details of the system call failed, even
		 * though we know the process is stopped on a system call, we
		 * are going to assume that the process got killed somehow.
		 * Thus, the best we can do is ignore the system call entirely,
		 * and hope that the next thing we hear about this process is
		 * its termination.  At worst, we ignore a serious error..
		 */
		proc->call_flags = CF_HIDE;

		return FALSE;
	}

	/*
	 * Obtain the call name that is to be used for this call, and decide
	 * whether we want to print this call at all.
	 */
	proc->call_type = (int)reg[0];
	trace_class = TC_NORMAL;

	name = call_prepare(proc, reg, &trace_class);

	proc->call_name = name;

	if (call_hide(name)) {
		proc->call_flags = CF_HIDE;

		return trace_class;
	}

	/* Only print a stack trace if we are printing the call itself. */
	if (show_stack)
		kernel_put_stacktrace(proc);

	/*
	 * Start a new line, start recording, and print the call name and
	 * opening parenthesis.
	 */
	put_newline();

	format_reset(proc);

	record_start(proc);

	put_text(proc, name);
	put_open(proc, NULL, PF_NONAME, "(", ", ");

	/*
	 * Print the outgoing part of the call, that is, some or all of its
	 * parameters.  This call returns flags indicating how far printing
	 * got, and may be one of the following combinations:
	 * - CT_NOTDONE (0) if printing parameters is not yet complete; after
	 *   the call split, the in handler must print the rest itself;
	 * - CT_DONE (CF_DONE) if printing parameters is complete, and we
	 *   should now print the closing parenthesis and equals sign;
	 * - CT_NORETURN (CF_DONE|CF_NORETURN) if printing parameters is
	 *   complete, but we should not print the equals sign, because the
	 *   call is expected not to return (the no-return call type).
	 */
	type = call_out(proc, reg);
	assert(type == CT_NOTDONE || type == CT_DONE || type == CT_NORETURN);

	/*
	 * Print whatever the handler told us to print for now.
	 */
	if (type & CF_DONE) {
		if (type & CF_NORETURN) {
			put_close(proc, ")");

			put_space(proc);

			proc->call_flags |= type;
		} else {
			/*
			 * The equals sign is printed implicitly for the
			 * CT_DONE type only.  For CT_NORETURN and CT_NOTDONE,
			 * the "in" handler has to do it explicitly.
			 */
			put_equals(proc);
		}
	} else {
		/*
		 * If at least one parameter was printed, print the separator
		 * now.  We know that another parameter will follow (otherwise
		 * the caller would have returned CT_DONE), and this way the
		 * output looks better.
		 */
		format_push_sep(proc);
	}

	/*
	 * We are now at the call split; further printing will be done once the
	 * call returns, through call_leave.  Stop recording; if the call gets
	 * suspended and later resumed, we should replay everything up to here.
	 */
#if DEBUG
	put_text(proc, "|"); /* warning, this may push a space */
#endif

	record_stop(proc);

	output_flush();

	return trace_class;
}

/*
 * The given process left a system call, or if skip is set, the leave phase of
 * the current system call should be ended.
 */
void
call_leave(struct trace_proc * proc, int skip)
{
	reg_t retreg;
	int hide, failed;

	/* If the call is skipped, it must be a no-return type call. */
	assert(!skip || (proc->call_flags & (CF_NORETURN | CF_HIDE)));

	/*
	 * Start by replaying the current call, if necessary.  If the call was
	 * suspended and we are about to print the "in" part, this is obviously
	 * needed.  If the call is hidden, replaying will be a no-op, since
	 * nothing was recorded for this call.  The special case is a skipped
	 * call (which, as established above, must be a no-return call, e.g.
	 * exec), for which replaying has the effect that if the call was
	 * previously suspended, it will now be replayed, without suspension:
	 *
	 *       2| execve("./test", ["./test"], [..(12)]) <..>
	 *       3| sigsuspend([]) = <..>
	 * [A]   2| execve("./test", ["./test"], [..(12)])
	 *       2| ---
	 *       2| Tracing test (pid 2)
	 *
	 * The [A] line is the result of replaying the skipped call.
	 */
	call_replay(proc);

	hide = (proc->call_flags & CF_HIDE);

	if (!hide && !skip) {
		/* Get the IPC-level result of the call. */
		if (kernel_get_retreg(proc->pid, &retreg) < 0) {
			/* This should never happen.  Deal with it anyway. */
			proc->call_flags |= CF_REG_ERR;
			failed = PF_FAILED;
		} else if ((proc->call_result = (int)retreg) < 0) {
			proc->call_flags |= CF_IPC_ERR;
			failed = PF_FAILED;
		} else
			failed = 0;

		/*
		 * Print the incoming part of the call, that is, possibly some
		 * or all of its parameters and the call's closing parenthesis
		 * (if CT_NOTDONE), and the equals sign (if not CT_DONE), then
		 * the call result.
		 */
		call_in(proc, failed);
	}

	if (!hide) {
		/*
		 * The call is complete now, so clear the recording.  This also
		 * implies that no suspension marker will be printed anymore.
		 */
		record_clear(proc);

		put_newline();
	}

	/*
	 * For calls not of the no-return type, an equals sign must have been
	 * printed by now.  This is protection against badly written handlers.
	 */
	assert(proc->call_flags & CF_DONE);

	proc->call_name = NULL;
	proc->call_flags = 0;
}

/*
 * Replay the recorded text, if any, for the enter phase of the given process.
 * If there is no recorded text, start a new line anyway.
 */
void
call_replay(struct trace_proc * proc)
{

	/*
	 * We get TRUE if the recorded call should be replayed, but the
	 * recorded text for the call did not fit in the recording buffer.
	 * In that case, we have to come up with a replacement text for the
	 * call up to the call split.
	 */
	if (record_replay(proc) == TRUE) {
		/*
		 * We basically place a "<..>" suspension marker in the
		 * parameters part of the call, and use its call name and flags
		 * for the rest.  There is a trailing space in all cases.
		 */
		put_fmt(proc, "%s(<..>%s", proc->call_name,
		    !(proc->call_flags & CF_DONE) ? "," :
		    ((proc->call_flags & CF_NORETURN) ? ")" : ") ="));
		put_space(proc);
	}
}

/*
 * Return the human-readable name of the call currently being made by the given
 * process.  The process is guaranteed to be in a call, although the call may
 * be hidden.  Under no circumstances may this function return a NULL pointer.
 */
const char *
call_name(struct trace_proc * proc)
{

	assert(proc->call_name != NULL);

	return proc->call_name;
}

/*
 * Return whether the current call failed due to an error at the system call
 * level, and if so, return the error code as well.  May be called during the
 * leave phase of a call only.
 */
int
call_errno(struct trace_proc * proc, int * err)
{

	if (proc->call_flags & (CF_REG_ERR | CF_MSG_ERR | CF_IPC_ERR))
		return FALSE;

	if (proc->call_result >= 0)
		return FALSE;

	*err = -proc->call_result;
	return TRUE;
}
