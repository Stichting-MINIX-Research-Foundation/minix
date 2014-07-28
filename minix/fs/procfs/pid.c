/* ProcFS - pid.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

#include <sys/mman.h>
#include <minix/vm.h>

#define S_FRAME_SIZE	4096		/* use malloc if larger than this */
static char s_frame[S_FRAME_SIZE];	/* static storage for process frame */
static char *frame;			/* pointer to process frame buffer */

static void pid_psinfo(int slot);
static void pid_cmdline(int slot);
static void pid_environ(int slot);
static void pid_map(int slot);

/* The files that are dynamically created in each PID directory. The data field
 * contains each file's read function. Subdirectories are not yet supported.
 */
struct file pid_files[] = {
	{ "psinfo",	REG_ALL_MODE,	(data_t) pid_psinfo	},
	{ "cmdline",	REG_ALL_MODE,	(data_t) pid_cmdline	},
	{ "environ",	REG_ALL_MODE,	(data_t) pid_environ	},
	{ "map",	REG_ALL_MODE,	(data_t) pid_map	},
	{ NULL,		0,		(data_t) NULL		}
};

/*===========================================================================*
 *				is_zombie				     *
 *===========================================================================*/
static int is_zombie(int slot)
{
	/* Is the given slot a zombie process?
	 */

	return (slot >= NR_TASKS && 
		(mproc[slot - NR_TASKS].mp_flags & (TRACE_ZOMBIE | ZOMBIE)));
}

/*===========================================================================*
 *				pid_psinfo				     *
 *===========================================================================*/
static void pid_psinfo(int i)
{
	/* Print information used by ps(1) and top(1).
	 */
	int pi, task, state, type, p_state, f_state;
	char name[PROC_NAME_LEN+1], *p;
	struct vm_usage_info vui;
	pid_t ppid;

	pi = i - NR_TASKS;
	task = proc[i].p_nr < 0;

	/* Get the name of the process. Spaces would mess up the format.. */
	if (task || mproc[i].mp_name[0] == 0)
		strncpy(name, proc[i].p_name, sizeof(name) - 1);
	else
		strncpy(name, mproc[pi].mp_name, sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;
	if ((p = strchr(name, ' ')) != NULL)
		p[0] = 0;

	/* Get the type of the process. */
	if (task)
		type = TYPE_TASK;
	else if (mproc[i].mp_flags & PRIV_PROC)
		type = TYPE_SYSTEM;
	else
		type = TYPE_USER;

	/* Get the state of the process. */
	if (!task) {
		if (is_zombie(i))
			state = STATE_ZOMBIE;	/* zombie */
		else if (mproc[pi].mp_flags & TRACE_STOPPED)
			state = STATE_STOP;	/* stopped (traced) */
		else if (proc[i].p_rts_flags == 0)
			state = STATE_RUN;	/* in run-queue */
		else if (fp_is_blocked(&fproc[pi]) ||
		(mproc[pi].mp_flags & (WAITING | SIGSUSPENDED)))
			state = STATE_SLEEP;	/* sleeping */
		else
			state = STATE_WAIT;	/* waiting */
	} else {
		if (proc[i].p_rts_flags == 0)
			state = STATE_RUN;	/* in run-queue */
		else
			state = STATE_WAIT;	/* other i.e. waiting */
	}

	/* We assume that even if a process has become a zombie, its kernel
	 * proc entry still contains the old (but valid) information. Currently
	 * this is true, but in the future we may have to filter some fields.
	 */
	buf_printf("%d %c %d %s %c %d %d %lu %lu %lu %lu",
		PSINFO_VERSION,			/* information version */
		type,				/* process type */
		(int) proc[i].p_endpoint,	/* process endpoint */
		name,				/* process name */
		state,				/* process state letter */
		(int) P_BLOCKEDON(&proc[i]),	/* endpt blocked on, or NONE */
		(int) proc[i].p_priority,	/* process priority */
		(long) proc[i].p_user_time,	/* user time */
		(long) proc[i].p_sys_time,	/* system time */
		ex64hi(proc[i].p_cycles),	/* execution cycles */
		ex64lo(proc[i].p_cycles)
	);

	memset(&vui, 0, sizeof(vui));

	if (!is_zombie(i)) {
		/* We don't care if this fails.  */
		(void) vm_info_usage(proc[i].p_endpoint, &vui);
	}

	/* If the process is not a kernel task, we add some extra info. */
	if (!task) {
		if (mproc[pi].mp_flags & WAITING)
			p_state = PSTATE_WAITING;
		else if (mproc[pi].mp_flags & SIGSUSPENDED)
			p_state = PSTATE_SIGSUSP;
		else
			p_state = '-';

		if (mproc[pi].mp_parent == pi)
			ppid = NO_PID;
		else
			ppid = mproc[mproc[pi].mp_parent].mp_pid;

		switch (fproc[pi].fp_blocked_on) {
		case FP_BLOCKED_ON_NONE:	f_state = FSTATE_NONE; break;
		case FP_BLOCKED_ON_PIPE:	f_state = FSTATE_PIPE; break;
		case FP_BLOCKED_ON_LOCK:	f_state = FSTATE_LOCK; break;
		case FP_BLOCKED_ON_POPEN:	f_state = FSTATE_POPEN; break;
		case FP_BLOCKED_ON_SELECT:	f_state = FSTATE_SELECT; break;
		case FP_BLOCKED_ON_OTHER:	f_state = FSTATE_TASK; break;
		default:			f_state = FSTATE_UNKNOWN;
		}

		buf_printf(" %lu %lu %lu %c %d %u %u %u %d %c %d %u",
			vui.vui_total,			/* total memory */
			vui.vui_common,			/* common memory */
			vui.vui_shared,			/* shared memory */
			p_state,			/* sleep state */
			ppid,				/* parent PID */
			mproc[pi].mp_realuid,		/* real UID */
			mproc[pi].mp_effuid,		/* effective UID */
			mproc[pi].mp_procgrp,		/* process group */
			mproc[pi].mp_nice,		/* nice value */
			f_state,			/* VFS block state */
			(int) (fproc[pi].fp_blocked_on == FP_BLOCKED_ON_OTHER)
				? fproc[pi].fp_task : NONE, /* block proc */
			fproc[pi].fp_tty		/* controlling tty */
		);
	}

	/* always add kernel cycles */
	buf_printf(" %lu %lu %lu %lu",
		ex64hi(proc[i].p_kipc_cycles),
		ex64lo(proc[i].p_kipc_cycles),
		ex64hi(proc[i].p_kcall_cycles),
		ex64lo(proc[i].p_kcall_cycles));

	/* add total memory for tasks at the end */
	if(task) buf_printf(" %lu", vui.vui_total);

	/* Newline at the end of the file. */
	buf_printf("\n");
}

/*===========================================================================*
 *				put_frame				     *
 *===========================================================================*/
static void put_frame(void)
{
	/* If we allocated memory dynamically during a call to get_frame(),
	 * free it up here.
	 */

	if (frame != s_frame)
		free(frame);
}

/*===========================================================================*
 *				get_frame				     *
 *===========================================================================*/
static int get_frame(int slot, vir_bytes *basep, vir_bytes *sizep,
	size_t *nargsp)
{
	/* Get the execution frame from the top of the given process's stack.
	 * It may be very large, in which case we temporarily allocate memory
	 * for it (up to a certain size).
	 */
	vir_bytes base, size;
	size_t nargs;

	if (proc[slot].p_nr < 0 || is_zombie(slot))
		return FALSE;

	/* Get the frame base address and size. Limit the size to whatever we
	 * can handle. If our static buffer is not sufficiently large to store
	 * the entire frame, allocate memory dynamically. It is then later
	 * freed by put_frame().
	 */
	base = mproc[slot - NR_TASKS].mp_frame_addr;
	size = mproc[slot - NR_TASKS].mp_frame_len;

	if (size < sizeof(size_t)) return FALSE;

	if (size > ARG_MAX) size = ARG_MAX;

	if (size > sizeof(s_frame)) {
		frame = malloc(size);

		if (frame == NULL)
			return FALSE;
	}
	else frame = s_frame;

	/* Copy in the complete process frame. */
	if (sys_datacopy(proc[slot].p_endpoint, base,
			SELF, (vir_bytes) frame, (phys_bytes) size) != OK) {
		put_frame();

		return FALSE;
	}

	frame[size] = 0; /* terminate any last string */

	nargs = * (size_t *) frame;
	if (nargs < 1 || sizeof(size_t) + sizeof(char *) * (nargs + 1) > size) {
		put_frame();

		return FALSE;
	}

	*basep = base;
	*sizep = size;
	*nargsp = nargs;

	/* The caller now has to called put_frame() to clean up. */
	return TRUE;
}

/*===========================================================================*
 *				pid_cmdline				     *
 *===========================================================================*/
static void pid_cmdline(int slot)
{
	/* Dump the process's command line as it is contained in the process
	 * itself. Each argument is terminated with a null character.
	 */
	vir_bytes base, size, ptr;
	size_t i, len, nargs;
	char **argv;

	if (!get_frame(slot, &base, &size, &nargs))
		return;

	argv = (char **) &frame[sizeof(size_t)];

	for (i = 0; i < nargs; i++) {
		ptr = (vir_bytes) argv[i] - base;

		/* Check for bad pointers. */
		if ((long) ptr < 0L || ptr >= size)
			break;

		len = strlen(&frame[ptr]) + 1;

		buf_append(&frame[ptr], len);
	}

	put_frame();
}

/*===========================================================================*
 *				pid_environ				     *
 *===========================================================================*/
static void pid_environ(int slot)
{
	/* Dump the process's initial environment as it is contained in the
	 * process itself. Each entry is terminated with a null character.
	 */
	vir_bytes base, size, ptr;
	size_t nargs, off, len;
	char **envp;

	if (!get_frame(slot, &base, &size, &nargs))
		return;

	off = sizeof(size_t) + sizeof(char *) * (nargs + 1);
	envp = (char **) &frame[off];

	for (;;) {
		/* Make sure there is no buffer overrun. */
		if (off + sizeof(char *) > size)
			break;

		ptr = (vir_bytes) *envp;

		/* Stop at the terminating NULL pointer. */
		if (ptr == 0L)
			break;

		ptr -= base;

		/* Check for bad pointers. */
		if ((long) ptr < 0L || ptr >= size)
			break;

		len = strlen(&frame[ptr]) + 1;

		buf_append(&frame[ptr], len);

		off += sizeof(char *);
		envp++;
	}

	put_frame();
}

/*===========================================================================*
 *				dump_regions				     *
 *===========================================================================*/
static int dump_regions(int slot)
{
	/* Print the virtual memory regions of a process.
	 */
	struct vm_region_info vri[MAX_VRI_COUNT];
	vir_bytes next;
	int i, r, count;

	count = 0;
	next = 0;

	do {
		r = vm_info_region(proc[slot].p_endpoint, vri, MAX_VRI_COUNT,
			&next);

		if (r < 0)
			return r;

		if (r == 0)
			break;

		for (i = 0; i < r; i++) {
			buf_printf("%08lx-%08lx %c%c%c\n",
				vri[i].vri_addr, vri[i].vri_addr + vri[i].vri_length,
				(vri[i].vri_prot & PROT_READ) ? 'r' : '-',
				(vri[i].vri_prot & PROT_WRITE) ? 'w' : '-',
				(vri[i].vri_prot & PROT_EXEC) ? 'x' : '-');

			count++;
		}
	} while (r == MAX_VRI_COUNT);

	return count;
}

/*===========================================================================*
 *				pid_map					     *
 *===========================================================================*/
static void pid_map(int slot)
{
	/* Print a memory map of the process. Obtain the information from VM if
	 * possible; otherwise fall back on segments from the kernel.
	 */

	/* Zombies have no memory. */
	if (is_zombie(slot))
		return;

	/* Kernel tasks also have no memory. */
	if (proc[slot].p_nr >= 0) {
		if (dump_regions(slot) != 0)
			return;
	}
}
