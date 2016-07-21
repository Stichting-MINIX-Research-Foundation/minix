/* libsockevent - sockevent_proc.c - process suspension state management */

#include <minix/drivers.h>
#include <minix/sockdriver.h>

#include "sockevent_proc.h"

static struct sockevent_proc sockevent_procs[NR_PROCS];
static struct sockevent_proc *sockevent_freeprocs;

/*
 * Initialize the process suspension table.
 */
void
sockevent_proc_init(void)
{
	unsigned int slot;

	for (slot = 0; slot < __arraycount(sockevent_procs); slot++) {
		sockevent_procs[slot].spr_next = sockevent_freeprocs;
		sockevent_freeprocs = &sockevent_procs[slot];
	}
}

/*
 * Allocate and return a new socket process suspension entry.  Return NULL if
 * no entries are available.
 */
struct sockevent_proc *
sockevent_proc_alloc(void)
{
	struct sockevent_proc *spr;

	if ((spr = sockevent_freeprocs) == NULL)
		return NULL;

	sockevent_freeprocs = spr->spr_next;
	spr->spr_next = NULL;

	return spr;
}

/*
 * Free up a previously allocated socket process suspension entry for reuse.
 */
void
sockevent_proc_free(struct sockevent_proc * spr)
{

	spr->spr_next = sockevent_freeprocs;
	sockevent_freeprocs = spr;
}
