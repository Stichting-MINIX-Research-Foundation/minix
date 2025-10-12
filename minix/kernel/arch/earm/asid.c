
#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "arch_proto.h"
#include <machine/cpu.h>
#include "asid.h"
#include "cpufunc.h"

/*
 * ASID manager for ARMv7.
 *
 * Manages a pool of 255 user-space ASIDs (1-255). ASID 0 is reserved.
 * This implementation uses a free list for efficient, flush-free allocation
 * of unused ASIDs. When the free list is exhausted, it falls back to a
 * round-robin recycling strategy which requires a full TLB flush.
 *
 * The use of generation counters prevents the ABA problem, where a process
 * could incorrectly match stale TLB entries from a long-dead process that
 * used the same ASID.
 */

#define ASID_COUNT 256             // Total number of ASIDs (0-255)
#define ASID_MAX_VALUE 255         // The highest valid ASID number
#define ASID_FIRST_AVAILABLE 1     // First user-space ASID

/* Global generation counters for each ASID */
static u32_t asid_generations[ASID_COUNT];

/* Free list for unused ASIDs */
static u8_t asid_free_list[ASID_COUNT];
static int asid_free_count = 0;

/* Round-robin counter for recycling */
static int next_asid_to_recycle = ASID_FIRST_AVAILABLE;

static spinlock_t asid_lock;

void asid_init(void)
{
	int i;
	spinlock_init(&asid_lock);
	memset(asid_generations, 0, sizeof(asid_generations));

	/* Initialize the free list with all available ASIDs */
	asid_free_count = 0;
	for (i = ASID_FIRST_AVAILABLE; i <= ASID_MAX_VALUE; i++) {
		asid_free_list[asid_free_count++] = (u8_t)i;
	}
}

void asid_alloc(struct proc *p)
{
	int asid = 0;

	spinlock_lock(&asid_lock);

	/* If process already has a valid ASID, do nothing. */
	if (p->p_asid != 0 && p->p_asid_generation == asid_generations[p->p_asid]) {
		spinlock_unlock(&asid_lock);
		return;
	}

	/* First, try to get an ASID from the free list. This is the fast path. */
	if (asid_free_count > 0) {
		asid = asid_free_list[--asid_free_count];
	} else {
		/*
		 * Free list is empty. We must recycle an ASID. This is the slow path.
		 * For SMP, a full TLB shootdown IPI would be required here.
		 * For a single-core system, flushing the local TLB is sufficient.
		 */
		asid = next_asid_to_recycle;
		next_asid_to_recycle = ((asid - ASID_FIRST_AVAILABLE + 1) %
			(ASID_MAX_VALUE - ASID_FIRST_AVAILABLE + 1)) + ASID_FIRST_AVAILABLE;

		/* Invalidate all TLB entries before recycling the ASID. */
		tlb_flush_all();
	}

	/*
	 * Increment the generation for the chosen ASID. This invalidates any
	 * stale references to this ASID number from previous holders.
	 */
	asid_generations[asid]++;
	if (asid_generations[asid] == 0) { /* Handle wrap-around */
		/*
		 * The generation counter for this ASID has wrapped. To prevent a
		 * potential ABA problem with a process from the previous cycle of
		 * 2^32 generations, we must flush the entire TLB.
		 */
		tlb_flush_all();
		asid_generations[asid] = 1; /* Restart at 1 */
	}

	p->p_asid = asid;
	p->p_asid_generation = asid_generations[asid];

	spinlock_unlock(&asid_lock);
}

void asid_free(struct proc *p)
{
	if (p->p_asid == 0) {
		return;
	}

	spinlock_lock(&asid_lock);

	/*
	 * The generation check ensures we don't free an ASID that was already
	 * recycled and given to another process.
	 */
	if (p->p_asid_generation == asid_generations[p->p_asid]) {
		if (asid_free_count < (ASID_COUNT - ASID_FIRST_AVAILABLE)) {
			asid_free_list[asid_free_count++] = p->p_asid;
		}
		/*
		 * If the free list is somehow full, we just let the ASID
		 * drop. It will be recycled later by the allocator.
		 */
	}

	p->p_asid = 0;
	p->p_asid_generation = 0;

	spinlock_unlock(&asid_lock);
}
