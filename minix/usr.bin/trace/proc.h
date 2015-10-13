
#include <sys/queue.h>

/*
 * The maximum nesting depth of parentheses/brackets.  The current maximum
 * depth is something like six, for UDS control messages.  This constant can be
 * increased as necessary without any problem.
 */
#define MAX_DEPTH	10

/*
 * The maximum size of text that may be recorded, including null terminator.
 * Increasing this allows longer lines to be recorded and replayed without
 * being cut short (see call_replay), but also increases memory usage.
 */
#define RECORD_BUFSZ	256

struct trace_proc {
	/* identity (public) */
	pid_t pid;

	/* data structure management (proc.c) */
	TAILQ_ENTRY(trace_proc) next;

	/* general process state (trace.c) */
	char name[PROC_NAME_LEN];
	unsigned int trace_flags;
	reg_t last_pc;
	reg_t last_sp;

	/* call enter-to-leave state (call.c) */
	int call_type;
	vir_bytes m_addr;
	message m_out;
	const char *call_name;
	unsigned int call_flags;
	const struct call_handler *call_handler;
	int call_result;

	/* output state (output.c) */
	int recording;
	char outbuf[RECORD_BUFSZ];
	size_t outlen;

	/* formatting state (format.c) */
	const char *next_sep;
	int depth;
	struct {
		const char *sep;
		int name;
	} depths[MAX_DEPTH];

	/* ioctl state (ioctl.c) */
	int ioctl_index;
	unsigned int ioctl_flags;

	/* sysctl state (service/mib.c) */
	uint32_t sctl_flags;
	size_t sctl_size;
	int (*sctl_proc)(struct trace_proc *, const char *, int, const void *,
	    vir_bytes, size_t);
	int sctl_arg;
};

/* Trace flags. */
#define TF_INCALL	0x01	/* the process has entered a system call */
#define TF_SKIP		0x02	/* the system call result is to be skipped */
#define TF_CTX_SKIP	0x04	/* skip call result only if context changes */
#define TF_STOPPING	0x08	/* the process is expecting a SIGSTOP */
#define TF_ATTACH	0x10	/* we have not started this process */
#define TF_DETACH	0x20	/* detach from the process as soon as we can */
#define TF_EXEC		0x40	/* the process may be performing an execve() */
#define TF_NOCALL	0x80	/* no system call seen yet (for info only) */

/* Trace classes, determining how the tracer engine should handle a call. */
#define TC_NORMAL	0	/* normal call, no exceptions required */
#define TC_EXEC		1	/* exec call, success on subsequent SIGSTOP */
#define TC_SIGRET	2	/* sigreturn call, success on context change */

/* Call flags. */
#define CF_DONE		0x01	/* printing the call parameters is done */
#define CF_NORETURN	0x02	/* the call does not return on success */
#define CF_HIDE		0x04	/* do not print the current call */
#define CF_IPC_ERR	0x08	/* a failure occurred at the IPC level */
#define CF_REG_ERR	0x10	/* unable to retrieve the result register */
#define CF_MSG_ERR	0x20	/* unable to copy in the reply message */

/* Call types, determining how much has been printed up to the call split. */
#define CT_NOTDONE	(0)	/* not all parameters have been printed yet */
#define CT_DONE		(CF_DONE)	/* all parameters have been printed */
#define CT_NORETURN	(CF_DONE | CF_NORETURN)	/* the no-return call type */

/* Put flags. */
#define PF_FAILED	0x01	/* call failed, results may be invalid */
#define PF_LOCADDR	0x02	/* pointer is into local address space */
/* Yes, PF_LOCAL would conflict with the packet family definition.  Bah. */
#define PF_ALT		0x04	/* alternative output (callee specific) */
#define PF_STRING	PF_ALT	/* buffer is string (put_buf only) */
#define PF_FULL		0x08	/* print full format (callee specific) */
#define PF_PATH		(PF_STRING | PF_FULL)	/* flags for path names */
#define PF_NONAME	0x10	/* default to no field names at this depth */

/* I/O control flags. */
#define IF_OUT		0x1	/* call to print outgoing (written) data */
#define IF_IN		0x2	/* call to print incoming (read) data */
#define IF_ALL		0x4	/* all fields printed (not really a bit) */

/* Sysctl processing types, determining what the callback function is to do. */
#define ST_NAME		0	/* print the rest of the name */
#define ST_OLDP		1	/* print the data pointed to by oldp */
#define ST_NEWP		2	/* print the data pointed to by newp */
