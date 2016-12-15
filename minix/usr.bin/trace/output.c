
#include "inc.h"

#include <fcntl.h>
#include <unistd.h>

/*
 * The maximum number of bytes that may be buffered before writing the buffered
 * output to the underlying file.  This is a performance optimization only.
 * Writing more than this number of bytes at once will be handled correctly.
 */
#define OUTPUT_BUFSZ	512

static int out_fd;
static char out_buf[OUTPUT_BUFSZ];
static int out_len;
static int out_err;

static pid_t last_pid; /* not a trace_proc pointer; it could become invalid! */
static unsigned int line_off;
static unsigned int prefix_off;
static int print_pid;
static int print_susp;
static int add_space;

/*
 * Initialize the output channel.  Called before any other output functions,
 * but after a child process (to be traced) has already been spawned.  If the
 * given file string is not NULL, it is the path to a file that is to be used
 * to write output to.  If it is NULL, output is written to standard error.
 */
int
output_init(const char * file)
{

	/* Initialize state. */
	out_len = 0;
	out_err = FALSE;

	last_pid = 0;
	line_off = 0;
	prefix_off = 0;
	print_pid = FALSE;
	print_susp = FALSE;
	add_space = FALSE;

	/*
	 * Ignore signals resulting from writing to a closed pipe.  We can
	 * handle write errors properly ourselves.  Setting O_NOSIGPIPE is an
	 * alternative, but that would affect other processes writing to the
	 * same file object, even after we have terminated.
	 */
	signal(SIGPIPE, SIG_IGN);

	/* Initialize the output file descriptor. */
	if (file == NULL) {
		/* No output file given?  Use standard error. */
		out_fd = STDERR_FILENO;

		return 0;
	} else {
		/*
		 * Use a restrictive mask for the output file.  Traces may
		 * contain sensitive information (for security and otherwise),
		 * and the user might not always be careful about the location
		 * of the file.
		 */
		/* The file descriptor is not closed explicitly. */
		out_fd = open(file, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND,
		    0600);

		return (out_fd < 0) ? -1 : 0;
	}
}

/*
 * Write the given data to the given file descriptor, taking into account the
 * possibility of partial writes and write errors.
 */
static void
write_fd(int fd, const char *buf, size_t len)
{
	ssize_t r;

	/* If we got a write error before, do not try to write more. */
	if (out_err)
		return;

	/* Write all output, in chunks if we have to. */
	while (len > 0) {
		r = write(fd, buf, len);

		/*
		 * A write error (and that includes EOF) causes the program to
		 * terminate with an error code.  For obvious reasons we cannot
		 * print an error about this.  Do not even report to standard
		 * error if the output was redirected, because that may mess
		 * with the actual programs being run right now.
		 */
		if (r <= 0) {
			out_err = TRUE;

			break;
		}

		len -= r;
	}
}

/*
 * Return TRUE iff an output error occurred and the program should terminate.
 */
int
output_error(void)
{

	return out_err;
}

/*
 * Print the given null-terminated string to the output channel.  Return the
 * number of characters printed, for alignment purposes.  In the future, this
 * number may end up being different from the number of bytes given to print,
 * due to multibyte encoding or colors or whatnot.
 */
static unsigned int
output_write(const char * text)
{
	size_t len;

	len = strlen(text);

	if (out_len + len > sizeof(out_buf)) {
		write_fd(out_fd, out_buf, out_len);

		out_len = 0;

		/* Write large buffers right away. */
		if (len > sizeof(out_buf)) {
			write_fd(out_fd, text, len);

			return len;
		}
	}

	memcpy(&out_buf[out_len], text, len);

	out_len += len;

	return len;
}

/*
 * Flush any pending output to the output channel.
 */
void
output_flush(void)
{

	if (out_len > 0) {
		write_fd(out_fd, out_buf, out_len);

		out_len = 0;
	}
}

/*
 * Print a prefix for a line of output.  Possibly print a timestamp first.
 * Then, if applicable, print a PID prefix for the given process, or an info
 * prefix if no process (NULL) is given.
 *
 * PIDs are relevant only when multiple processes are traced.  As long as there
 * are multiple processes, each line is prefixed with the PID of the process.
 * As soon as the number of processes has been reduced back to one, one more
 * line is prefixed with the PID of the remaining process (with a "'" instead
 * of a "|") to help the user identify which process remains.  In addition,
 * whenever a preempted call is about to be resumed, a "*" is printed instead
 * of a space, so as to show that it is a continuation of a previous line.  An
 * example of all these cases:
 *
 *   fork() = 3
 *       3| Tracing test (pid 3)
 *       3| fork() = 0
 *       3| read(0, <..>
 *       2| waitpid(-1, <..>
 *    INFO| This is an example info line.
 *       3|*read(0, "", 1024) = 0
 *       3| exit(1)
 *       3| Process exited normally with code 1
 *       2'*waitpid(-1, W_EXITED(1), 0) = 3
 *   exit(0)
 *   Process exited normally with code 0
 */
static void
put_prefix(struct trace_proc * proc, int resuming)
{
	struct timeval tv;
	struct tm *tm;
	char prefix[32];
	unsigned int off, count;

	assert(line_off == 0);

	off = 0;

	if (timestamps > 0) {
		gettimeofday(&tv, NULL);

		tm = gmtime(&tv.tv_sec);

		off = strftime(prefix, sizeof(prefix), "%T", tm);

		if (timestamps > 1)
			off += snprintf(&prefix[off], sizeof(prefix) - off,
			    ".%06u", tv.tv_usec);

		assert(off < sizeof(prefix) - 2);
		prefix[off++] = ' ';
		prefix[off] = '\0';

		off = output_write(prefix);
	}

	count = proc_count();

	/* TODO: add a command line option for always printing the pid. */
	if (print_pid || count > 1 || proc == NULL) {
		/*
		 * TODO: we currently rely on the highest PID having at most
		 * five digits, but this will eventually change.  There are
		 * several ways to deal with that, but none are great.
		 */
		if (proc == NULL)
			snprintf(prefix, sizeof(prefix), "%5s| ", "INFO");
		else
			snprintf(prefix, sizeof(prefix), "%5d%c%c",
			    proc->pid, (count > 1) ? '|' : '\'',
			    resuming ? '*' : ' ');

		off += output_write(prefix);

		last_pid = (proc != NULL ? proc->pid : 0);
	} else
		assert(!resuming);

	prefix_off = off;
	line_off += off;

	/* Remember whether the next line should get prefixed regardless. */
	print_pid = (count > 1 || proc == NULL);
}

/*
 * Add a string to the end of the text recording for the given process.
 * This is used only to record the call-enter output of system calls.
 */
static void
record_add(struct trace_proc * proc, const char * text)
{
	size_t len;

	assert(proc->recording);

	/* If the recording buffer is already full, do not record more. */
	if (proc->outlen == sizeof(proc->outbuf))
		return;

	len = strlen(text);

	/* If nonempty, the recording buffer is always null terminated. */
	if (len < sizeof(proc->outbuf) - proc->outlen - 1) {
		strcpy(&proc->outbuf[proc->outlen], text);

		proc->outlen += len;
	} else
		proc->outlen = sizeof(proc->outbuf); /* buffer exhausted */
}

/*
 * Start recording text for the given process.  Since this marks the start of
 * a call, remember to print a preemption marker when the call gets preempted.
 */
void
record_start(struct trace_proc * proc)
{

	proc->recording = TRUE;

	print_susp = TRUE;
}

/*
 * Stop recording text for the given process.
 */
void
record_stop(struct trace_proc * proc)
{

	proc->recording = FALSE;
}

/*
 * Clear recorded text for the given process.  Since this also marks the end of
 * the entire call, no longer print a supension marker before the next newline.
 */
void
record_clear(struct trace_proc * proc)
{

	assert(!proc->recording);
	proc->outlen = 0;

	if (proc->pid == last_pid)
		print_susp = FALSE;
}

/*
 * Replay the record for the given process on a new line, if the current line
 * does not already have output for this process.  If it does, do nothing.
 * If the process has no recorded output, just start a new line.  Return TRUE
 * iff the caller must print its own replay text due to a recording overflow.
 */
int
record_replay(struct trace_proc * proc)
{
	int space;

	assert(!proc->recording);

	/*
	 * If there is output on the current line, and it is for the current
	 * process, we must assume that it is the original, recorded text, and
	 * thus, we should do nothing.  If output on the current line is for
	 * another process, we must force a new line before replaying.
	 */
	if (line_off > 0) {
		if (proc->pid == last_pid)
			return FALSE;

		put_newline();
	}

	/*
	 * If there is nothing to replay, do nothing further.  This case may
	 * occur when printing signals, in which case the caller still expects
	 * a new line to be started.  This line must not be prefixed with a
	 * "resuming" marker though--after all, nothing is being resumed here.
	 */
	if (proc->outlen == 0)
		return FALSE;

	/*
	 * If there is text to replay, then this does mean we are in effect
	 * resuming the recorded call, even if it is just to print a signal.
	 * Thus, we must print a prefix that shows the call is being resumed.
	 * Similarly, unless the recording is cleared before a newline, we must
	 * suspend the line again, too.
	 */
	put_prefix(proc, TRUE /*resuming*/);

	print_susp = TRUE;

	/*
	 * If the recording buffer was exhausted during recording, the caller
	 * must generate the replay text instead.
	 */
	if (proc->outlen == sizeof(proc->outbuf))
		return TRUE;

	/*
	 * Replay the recording.  If it ends with a space, turn it into a soft
	 * space, because the recording may be followed immediately by a
	 * newline; an example of this is the exit() exception.
	 */
	space = proc->outbuf[proc->outlen - 1] == ' ';
	if (space)
		proc->outbuf[proc->outlen - 1] = 0;

	put_text(proc, proc->outbuf);

	if (space) {
		put_space(proc);

		/* Restore the space, in case another replay takes place. */
		proc->outbuf[proc->outlen - 1] = ' ';
	}

	return FALSE;
}

/*
 * Start a new line, and adjust the local state accordingly.  If nothing has
 * been printed on the current line yet, this function is a no-op.  Otherwise,
 * the output so far may have to be marked as preempted with the "<..>"
 * preemption marker.
 */
void
put_newline(void)
{

	if (line_off == 0)
		return;

	if (print_susp) {
		if (add_space)
			(void)output_write(" ");

		(void)output_write("<..>");
	}

#if DEBUG
	(void)output_write("|");
#endif

	(void)output_write("\n");
	output_flush();

	line_off = 0;
	add_space = FALSE;
	print_susp = FALSE;
	last_pid = 0;
}

/*
 * Print a string as part of the output associated with a process.  If the
 * current line contains output for another process, a newline will be printed
 * first.  If the current line contains output for the same process, then the
 * text will simply continue on the same line.  If the current line is empty,
 * a process PID prefix may have to be printed first.  Either way, after this
 * operation, the current line will contain text for the given process.  If
 * requested, the text may also be recorded for the process, for later replay.
 * As an exception, proc may be NULL when printing general information lines.
 */
void
put_text(struct trace_proc * proc, const char * text)
{

	if (line_off > 0 && (proc == NULL || proc->pid != last_pid)) {
		/*
		 * The current line has not been terminated with a newline yet.
		 * Start a new line.  Note that this means that for lines not
		 * associated to a process, the whole line must be printed at
		 * once.  This can be fixed but is currently not an issue.
		 */
		put_newline();
	}

	/* See if we must add a prefix at the start of the line. */
	if (line_off == 0)
		put_prefix(proc, FALSE /*resuming*/);

	/* If needed, record the given text. */
	if (proc != NULL && proc->recording)
		record_add(proc, text);

	/*
	 * If we delayed printing a space, print one now.  This is never part
	 * of text that must be saved.  In fact, we support these soft spaces
	 * for exactly one case; see put_space() for details.
	 */
	if (add_space) {
		line_off += output_write(" ");

		add_space = FALSE;
	}

	/* Finally, print the actual text. */
	line_off += output_write(text);

	last_pid = (proc != NULL) ? proc->pid : 0;
}

/*
 * Add a space to the output for the given process, but only if and once more
 * text is printed for the process afterwards.  The aim is to ensure that no
 * lines ever end with a space, to prevent needless line wrapping on terminals.
 * The space may have to be remembered for the current line (for preemption,
 * which does not have a process pointer to work with) as well as recorded for
 * later replay, if recording is enabled.  Consider the following example:
 *
 * [A]   3| execve(..) <..>
 *       2| getpid(0) = 2 (ppid=1)
 * [B]   3| execve(..) = -1 [ENOENT]
 * [A]   3| exit(1) <..>
 *       2| getpid(0) = 2 (ppid=1)
 *       3| exit(1)
 *       3| Process exited normally with code 1
 *
 * On the [A] lines, the space between the call's closing parenthesis and the
 * "<..>" preemption marker is the result of add_space being set to TRUE; on
 * the [B] line, the space between the closing parenthesis and the equals sign
 * is the result of the space being recorded.
 */
void
put_space(struct trace_proc * proc)
{

	/* This call must only be used after output for the given process. */
	assert(last_pid == proc->pid);

	/* In case the call does not get preempted. */
	add_space = TRUE;

	/* In case the call does get preempted. */
	if (proc->recording)
		record_add(proc, " ");
}

/*
 * Indent the remainders of the text on the line for this process, such that
 * similar remainders are similarly aligned.  In particular, the remainder is
 * the equals sign of a call, and everything after it.  Of course, alignment
 * can only be used if the call has not already printed beyond the alignment
 * position.  Also, the prefix must not be counted toward the alignment, as it
 * is possible that a line without prefix may be preempted and later continued
 * with prefix.  All things considered, the result would look like this:
 *
 *   getuid()                      = 1 (euid=1)
 *   setuid(0)                     = -1 [EPERM]
 *   write(2, "Permission denied\n", 18) = 18
 *   fork()                        = 3
 *       3| Tracing test (pid 3)
 *       3| fork()                        = 0
 *       3| exit(0)
 *       3| Process exited normally with code 0
 *       2' waitpid(-1, W_EXITED(0), 0)   = 3
 *
 */
void put_align(struct trace_proc * __unused proc)
{

	/*
	 * TODO: add actual support for this.  The following code works,
	 * although not so efficiently.  The difficulty is the default
	 * configuration and corresponding options.

	while (line_off - prefix_off < 20)
		put_text(proc, " ");

	 */
}
