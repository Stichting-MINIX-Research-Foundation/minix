/* MINIX3 implementations of a subset of some BSD-kernel-specific functions. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>

#include <sys/sysctl.h>	/* for KERN_PROC_ */

#include <minix/paths.h>
#include <minix/procfs.h>

#include "minix_proc.h"

/*
 * Parse a procfs psinfo file, and fill the given minix_proc structure with the
 * results.  Return 1 on success, or 0 if this process should be skipped.
 */
static int
parse_psinfo(FILE *fp, int op, int __unused arg, pid_t pid,
	struct minix_proc *p)
{
	char type, state, pstate, name[256];
	unsigned int uid, pgrp, dev;
	int version;

	assert(op == KERN_PROC_ALL); /* this is all we support right now */

	if (fscanf(fp, "%d", &version) != 1 || version != PSINFO_VERSION)
		return 0;

	if (fscanf(fp, " %c %*d %255s %c %*d %*d %*u %*u %*u %*u",
	    &type, name, &state) != 3)
		return 0;

	if (type != TYPE_USER)
		return 0; /* user processes only */

	if (fscanf(fp, " %*u %*u %*u %c %*d %u %*u %u %*d %*c %*d %u",
	    &pstate, &uid, &pgrp, &dev) != 4)
		return 0;

	/* The fields as expected by the main w(1) code. */
	p->p_pid = pid;
	p->p__pgid = (pid_t)pgrp;
	p->p_tpgid = (dev != 0) ? (pid_t)pgrp : 0;
	p->p_tdev = (dev_t)dev;
	strlcpy(p->p_comm, name, sizeof(p->p_comm));

	/* Some fields we need for ranking ("sorting") processes later. */
	p->p_minix_state = state;
	p->p_minix_pstate = pstate;

	return 1;
}

/*
 * The w(1)-specific implementation of kvm_getproc2.  Return an array of
 * process information structures (of type minix_proc), along with the number
 * of processes in the resulting array.  Return NULL on failure, in which case
 * errno should be set to something meaningful.
 */
struct minix_proc *
minix_getproc(void * __unused dummy, int op, int arg, int elemsize, int *cnt)
{
	struct minix_proc *procs;
	char path[PATH_MAX];
	DIR *dp;
	FILE *fp;
	struct dirent *de;
	pid_t pid, *pids;
	unsigned int i, npids, size;
	int e;

	assert(elemsize == sizeof(struct minix_proc));

	/*
	 * First see how much memory we will need in order to store the actual
	 * process data.  We store the PIDs in a (relatively small) allocated
	 * memory area immediately, so that we don't have to reiterate through
	 * the /proc directory twice.
	 */
	if ((dp = opendir(_PATH_PROC)) == NULL)
		return NULL;

	if ((pids = malloc(size = sizeof(pid_t) * 64)) == NULL) {
		e = errno;
		closedir(dp);
		errno = e;
		return NULL;
	}
	npids = 0;

	while ((de = readdir(dp)) != NULL) {
		if ((pid = (pid_t)atoi(de->d_name)) > 0) {
			if (sizeof(pid_t) * npids == size &&
			    (pids = realloc(pids, size *= 2)) == NULL)
				break;
			pids[npids++] = pid;
		}
	}

	closedir(dp);

	/* No results, or out of memory?  Then bail out. */
	if (npids == 0 || pids == NULL) {
		if (pids != NULL) {
			e = errno;
			free(pids);
			errno = e;
		} else
			errno = ENOENT; /* no processes found */
		return NULL;
	}

	/* Now obtain actual process data for the PIDs we obtained. */
	if ((procs = malloc(sizeof(struct minix_proc) * npids)) == NULL) {
		e = errno;
		free(pids);
		errno = e;
		return NULL;
	}

	*cnt = 0;
	for (i = 0; i < npids; i++) {
		pid = pids[i];

		snprintf(path, sizeof(path), _PATH_PROC "%u/psinfo", pid);

		/* Processes may legitimately disappear between calls. */
		if ((fp = fopen(path, "r")) == NULL)
			continue;

		if (parse_psinfo(fp, op, arg, pid, &procs[*cnt]))
			(*cnt)++;

		fclose(fp);
	}

	free(pids);

	/* The returned data is not freed, but we are called only once. */
	return procs;
}

/*
 * A w(1)-specific MINIX3 implementation of kvm_getargv2.  Return an array of
 * strings representing the command line of the given process, optionally (if
 * not 0) limited to a number of printable characters if the arguments were
 * to be printed with a space in between.  Return NULL on failure.  Since the
 * caller will not use earlier results after calling this function again, we
 * can safely return static results.
 */
char **
minix_getargv(void * __unused dummy, const struct minix_proc * p, int nchr)
{
#define MAX_ARGS 32
	static char *argv[MAX_ARGS+1], buf[4096];
	char path[PATH_MAX];
	ssize_t i, bytes;
	int fd, argc;

	/* Get the command line of the process from procfs. */
	snprintf(path, sizeof(path), _PATH_PROC "%u/cmdline", p->p_pid);

	if ((fd = open(path, O_RDONLY)) < 0)
		return NULL;

	bytes = read(fd, buf, sizeof(buf));

	close(fd);

	if (bytes <= 0)
		return NULL;

	/*
	 * Construct an array of arguments.  Stop whenever we run out of bytes
	 * or printable characters (simply counting the null characters as
	 * spaces), or whenever we fill up our argument array.  Note that this
	 * is security-sensitive code, as it effectively processes (mostly-)
	 * arbitrary input from other users.
	 */
	bytes--; /* buffer should/will be null terminated */
	if (nchr != 0 && bytes > nchr)
		bytes = nchr;
	argc = 0;
	for (i = 0; i < bytes && argc < MAX_ARGS; i++) {
		if (i == 0 || buf[i-1] == 0)
			argv[argc++] = &buf[i];
	}
	buf[i] = 0;
	argv[argc] = NULL;

	return argv;
}

/*
 * A w(1)-specific implementation of proc_compare_wrapper.  Return 0 if the
 * first given process is more worthy of being shown as the representative
 * process of what a user is doing, or 1 for the second process.  Since procfs
 * currently does not expose enough information to do this well, we use some
 * very basic heuristics, and leave a proper implementation to future work.
 */
int
minix_proc_compare(const struct minix_proc * p1, const struct minix_proc * p2)
{
	static const int state_prio[] = /* best to worst */
	    { STATE_RUN, STATE_WAIT, STATE_SLEEP, STATE_STOP, STATE_ZOMBIE };
	unsigned int i;
	int sp1 = -1, sp2 = -1;

	if (p1 == NULL) return 1;
	if (p2 == NULL) return 0;

	/*
	 * Pick any runnable candidate over a waiting candidate, any waiting
	 * candidate over a sleeping candidate, etcetera.  The rationale is
	 * roughly as follows: runnable means that something is definitely
	 * happening; waiting means that probably something interesting is
	 * happening, which eliminates e.g. shells; sleeping means that not
	 * much is going on; stopped and zombified means that definitely
	 * nothing is going on.
	 */
	for (i = 0; i < sizeof(state_prio) / sizeof(state_prio[0]); i++) {
		if (p1->p_minix_state == state_prio[i]) sp1 = i;
		if (p2->p_minix_state == state_prio[i]) sp2 = i;
	}
	if (sp1 != sp2)
		return (sp1 > sp2);

	/*
	 * Pick any non-PM-sleeping process over any PM-sleeping process.
	 * Here the rationale is that PM-sleeping processes are typically
	 * waiting for another process, which means that the other process is
	 * probably more worthy of reporting.  Again, the shell is an example
	 * of a process we'd rather not report if there's something else.
	 */
	if (sp1 == STATE_SLEEP) {
		if (p1->p_minix_pstate != PSTATE_NONE) return 1;
		if (p2->p_minix_pstate != PSTATE_NONE) return 0;
	}

	/*
	 * Pick the candidate with the largest PID.  The rationale is that
	 * statistically that will most likely yield the most recently spawned
	 * process, which makes it the most interesting process as well.
	 */
	return p1->p_pid < p2->p_pid;
}

/*
 * Obtain the system uptime in seconds.  Return 0 on success, with the uptime
 * stored in the given time_t field.  Return -1 on failure.
 */
int
minix_getuptime(time_t *timep)
{
	FILE *fp;
	int r;

	if ((fp = fopen(_PATH_PROC "uptime", "r")) == NULL)
		return -1;

	r = fscanf(fp, "%llu", timep);

	fclose(fp);

	return (r == 1) ? 0 : -1;
}
