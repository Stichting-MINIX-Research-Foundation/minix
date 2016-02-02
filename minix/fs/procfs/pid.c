/* ProcFS - pid.c - generators for PID-specific files */

#include "inc.h"

#include <sys/mman.h>
#include <minix/vm.h>

static void pid_psinfo(int slot);
static void pid_cmdline(int slot);
static void pid_environ(int slot);
static void pid_map(int slot);

/*
 * The files that are dynamically created in each PID directory.  The data
 * field contains each file's read function.  Subdirectories are not yet
 * supported.
 */
struct file pid_files[] = {
	{ "psinfo",	REG_ALL_MODE,	(data_t) pid_psinfo	},
	{ "cmdline",	REG_ALL_MODE,	(data_t) pid_cmdline	},
	{ "environ",	REG_ALL_MODE,	(data_t) pid_environ	},
	{ "map",	REG_ALL_MODE,	(data_t) pid_map	},
	{ NULL,		0,		(data_t) NULL		}
};

/*
 * Is the given slot a zombie process?
 */
static int
is_zombie(int slot)
{

	return (slot >= NR_TASKS &&
	    (proc_list[slot - NR_TASKS].mpl_flags & MPLF_ZOMBIE));
}

/*
 * Get MINIX3-specific process data for the process identified by the given
 * kernel slot.  Return OK or a negative error code.
 */
int
get_proc_data(pid_t pid, struct minix_proc_data * mpd)
{
	int mib[4] = { CTL_MINIX, MINIX_PROC, PROC_DATA, pid };
	size_t oldlen;

	oldlen = sizeof(*mpd);
	if (__sysctl(mib, __arraycount(mib), mpd, &oldlen, NULL, 0) != 0)
		return -errno;

	return OK;
}

/*
 * Print process information.  This feature is now used only by mtop(1), and as
 * a result, we only provide information that mtop(1) actually uses.  In the
 * future, this file may be extended with additional fields again.
 */
static void
pid_psinfo(int slot)
{
	struct minix_proc_data mpd;
	struct vm_usage_info vui;
	pid_t pid;
	uid_t uid;
	char *p;
	int task, type, state;

	if ((pid = pid_from_slot(slot)) == 0)
		return;

	if (get_proc_data(pid, &mpd) != OK)
		return;

	task = (slot < NR_TASKS);

	/* Get the type of the process. */
	if (task)
		type = TYPE_TASK;
	else if (mpd.mpd_flags & MPDF_SYSTEM)
		type = TYPE_SYSTEM;
	else
		type = TYPE_USER;

	/*
	 * Get the (rudimentary) state of the process.  The zombie flag is also
	 * in the proc_list entry but it just may be set since we obtained that
	 * entry, in which case we'd end up with the wrong state here.
	 */
	if (mpd.mpd_flags & MPDF_ZOMBIE)
		state = STATE_ZOMBIE;
	else if (mpd.mpd_flags & MPDF_RUNNABLE)
		state = STATE_RUN;
	else if (mpd.mpd_flags & MPDF_STOPPED)
		state = STATE_STOP;
	else
		state = STATE_SLEEP;

	/* Get the process's effective user ID. */
	if (!task)
		uid = proc_list[slot - NR_TASKS].mpl_uid;
	else
		uid = 0;

	/* Get memory usage.  We do not care if this fails. */
	memset(&vui, 0, sizeof(vui));
	if (!(mpd.mpd_flags & MPDF_ZOMBIE))
		(void)vm_info_usage(mpd.mpd_endpoint, &vui);

	/* Spaces in the process name would mess up the output format. */
	if ((p = strchr(mpd.mpd_name, ' ')) != NULL)
		*p = '\0';

	/* Print all the information. */
	buf_printf("%d %c %d %s %c %d %d %u %u "
	    "%"PRIu64" %"PRIu64" %"PRIu64" %lu %d %u\n",
	    PSINFO_VERSION,			/* information version */
	    type,				/* process type */
	    mpd.mpd_endpoint,			/* process endpoint */
	    mpd.mpd_name,			/* process name */
	    state,				/* process state letter */
	    mpd.mpd_blocked_on,			/* endpt blocked on, or NONE */
	    mpd.mpd_priority,			/* process priority */
	    mpd.mpd_user_time,			/* user time */
	    mpd.mpd_sys_time,			/* system time */
	    mpd.mpd_cycles,			/* execution cycles */
	    mpd.mpd_kipc_cycles,		/* kernel IPC cycles */
	    mpd.mpd_kcall_cycles,		/* kernel call cycles */
	    vui.vui_total,			/* total memory */
	    mpd.mpd_nice,			/* nice value */
	    uid					/* effective user ID */
	);
}

/*
 * Dump the process's command line as it is contained in the process itself.
 * Each argument is terminated with a null character.
 */
static void
pid_cmdline(int slot)
{
	char buf[BUF_SIZE];
	int mib[] = { CTL_KERN, KERN_PROC_ARGS, 0, KERN_PROC_ARGV };
	size_t oldlen;
	pid_t pid;

	/* Kernel tasks and zombies have no memory. */
	if ((pid = pid_from_slot(slot)) <= 0 || is_zombie(slot))
		return;

	mib[2] = proc_list[slot - NR_TASKS].mpl_pid;

	/* TODO: zero-copy into the main output buffer */
	oldlen = sizeof(buf);

	if (__sysctl(mib, __arraycount(mib), buf, &oldlen, NULL, 0) != 0)
		return;

	buf_append(buf, oldlen);
}

/*
 * Dump the process's initial environment as it is contained in the process
 * itself.  Each entry is terminated with a null character.
 */
static void
pid_environ(int slot)
{
	char buf[BUF_SIZE];
	int mib[] = { CTL_KERN, KERN_PROC_ARGS, 0, KERN_PROC_ENV };
	size_t oldlen;
	pid_t pid;

	/* Kernel tasks and zombies have no memory. */
	if ((pid = pid_from_slot(slot)) <= 0 || is_zombie(slot))
		return;

	mib[2] = proc_list[slot - NR_TASKS].mpl_pid;

	/* TODO: zero-copy into the main output buffer */
	oldlen = sizeof(buf);

	if (__sysctl(mib, __arraycount(mib), buf, &oldlen, NULL, 0) != 0)
		return;

	buf_append(buf, oldlen);
}

/*
 * Print the virtual memory regions of a process.
 */
static void
pid_map(int slot)
{
	struct minix_proc_data mpd;
	struct vm_region_info vri[MAX_VRI_COUNT];
	vir_bytes next;
	pid_t pid;
	int i, r, count;

	/* Kernel tasks and zombies have no memory. */
	if ((pid = pid_from_slot(slot)) <= 0 || is_zombie(slot))
		return;

	/* Get the process endpoint. */
	if (get_proc_data(pid, &mpd) != OK)
		return;

	count = 0;
	next = 0;

	do {
		r = vm_info_region(mpd.mpd_endpoint, vri, MAX_VRI_COUNT,
		    &next);

		if (r <= 0)
			break;

		for (i = 0; i < r; i++) {
			buf_printf("%08lx-%08lx %c%c%c\n",
			    vri[i].vri_addr,
			    vri[i].vri_addr + vri[i].vri_length,
			    (vri[i].vri_prot & PROT_READ) ? 'r' : '-',
			    (vri[i].vri_prot & PROT_WRITE) ? 'w' : '-',
			    (vri[i].vri_prot & PROT_EXEC) ? 'x' : '-');

			count++;
		}
	} while (r == MAX_VRI_COUNT);
}
