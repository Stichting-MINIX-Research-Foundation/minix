#define _SYSTEM 1

#include <assert.h>
#include <errno.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ipc.h>
#include <minix/rs.h>
#include <minix/type.h>
#include <stdlib.h>
#include <timers.h>
#include <unistd.h>	

#include <machine/archtypes.h>
#include "../../../kernel/priv.h"
#include "../../../kernel/proc.h"

#define SCHED_BITS	9
#define PRIO_BITS	8
#define QUANTUM_BITS	14

#define SCHED_SHIFT	0
#define PRIO_SHIFT	(SCHED_BITS + SCHED_SHIFT)
#define QUANTUM_SHIFT	(PRIO_SHIFT + PRIO_BITS)
#define ABI_SHIFT	(QUANTUM_SHIFT + QUANTUM_BITS)

#define MAXU(bits)	((unsigned) ((1 << (bits)) - 1))
#define MINS(bits)	((int) (-(1 << ((bits) - 1))))
#define MAXS(bits)	((int) ((1 << ((bits) - 1)) - 1))

#define ENCODE(value, shift, bits) \
	((((unsigned) (value)) & ((1 << (bits)) - 1)) << (shift))
#define DECODE(value, shift, bits) \
	((((unsigned) (value)) >> (shift)) & ((1 << (bits)) - 1))

PUBLIC int rss_nice_encode(int *nice, endpoint_t scheduler, 
	unsigned priority, unsigned quantum)
{
	unsigned scheduler_u;
	
	assert(ABI_SHIFT == 31);

	/* check whether values fit */
	if (!nice) return EINVAL;
	*nice = 0;
	scheduler_u = (unsigned) (scheduler + NR_TASKS);
	if (scheduler_u > MAXU(SCHED_BITS)) return EINVAL;
	if (priority > MAXU(PRIO_BITS)) return EINVAL;
	if (quantum > MAXU(QUANTUM_BITS)) return EINVAL;
	
	/* encode */
	*nice = ENCODE(scheduler_u, SCHED_SHIFT, SCHED_BITS) |
		ENCODE(priority, PRIO_SHIFT, PRIO_BITS) |
		ENCODE(quantum, QUANTUM_SHIFT, QUANTUM_BITS) |
		ENCODE(1, ABI_SHIFT, 1);
		
	return OK;
}

PUBLIC int rss_nice_decode(int nice, endpoint_t *scheduler, 
	unsigned *priority, unsigned *quantum)
{
	unsigned scheduler_u;
	
	assert(ABI_SHIFT == 31);
	
	/* check arguments */
	if (!scheduler) return EINVAL;
	if (!priority) return EINVAL;
	if (!quantum) return EINVAL;
	
	/* accept either old or new ABI */
	if (nice & (1 << ABI_SHIFT)) {
		/* new ABI, decode */
		scheduler_u = DECODE(nice, SCHED_SHIFT, SCHED_BITS);
		*scheduler = (int) scheduler_u - NR_TASKS;
		*priority = DECODE(nice, PRIO_SHIFT, PRIO_BITS);
		*quantum = DECODE(nice, QUANTUM_SHIFT, QUANTUM_BITS);
	} else {
		/* old ABI, not useful so just take defaults */
		*scheduler = SCHED_PROC_NR;
		*priority = USER_Q;
		*quantum = USER_QUANTUM;
	}
	
	return OK;
}
