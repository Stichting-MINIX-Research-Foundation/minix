
#include "inc.h"

static TAILQ_HEAD(, trace_proc) proc_root;
static unsigned int nr_procs;

/*
 * Initialize the list of traced processes.
 */
void
proc_init(void)
{

	TAILQ_INIT(&proc_root);
	nr_procs = 0;
}

/*
 * Add a new process to the list of traced processes, allocating memory for it
 * first.  Return the new process structure with its PID assigned and the rest
 * zeroed out, or NULL upon allocation failure (with errno set appropriately).
 */
struct trace_proc *
proc_add(pid_t pid)
{
	struct trace_proc *proc;

	proc = (struct trace_proc *)malloc(sizeof(struct trace_proc));

	if (proc == NULL)
		return NULL;

	memset(proc, 0, sizeof(*proc));

	proc->pid = pid;

	TAILQ_INSERT_TAIL(&proc_root, proc, next);
	nr_procs++;

	return proc;
}

/*
 * Retrieve the data structure for a traced process based on its PID.  Return
 * a pointer to the structure, or NULL if no structure exists for this process.
 */
struct trace_proc *
proc_get(pid_t pid)
{
	struct trace_proc *proc;

	/* Linear search for now; se we can easily add a hashtable later.. */
	TAILQ_FOREACH(proc, &proc_root, next) {
		if (proc->pid == pid)
			return proc;
	}

	return NULL;
}

/*
 * Remove a process from the list of traced processes.
 */
void
proc_del(struct trace_proc * proc)
{

	TAILQ_REMOVE(&proc_root, proc, next);
	nr_procs--;

	free(proc);
}

/*
 * Iterator for the list of traced processes.  If a NULL pointer is given,
 * return the first process in the list; otherwise, return the next process in
 * the list.  Not stable with respect to list modifications.
 */
struct trace_proc *
proc_next(struct trace_proc * proc)
{

	if (proc == NULL)
		return TAILQ_FIRST(&proc_root);
	else
		return TAILQ_NEXT(proc, next);
}

/*
 * Return the number of processes in the list of traced processes.
 */
unsigned int
proc_count(void)
{

	return nr_procs;
}
