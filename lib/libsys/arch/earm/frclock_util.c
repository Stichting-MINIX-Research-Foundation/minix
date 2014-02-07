/* Some utility functions around the free running clock on ARM. The clock is
 * 32-bits wide, but we provide 64-bit wrapper functions to make it look
 * similar to the read_tsc functions. On hardware we could actually make use
 * of the timer overflow counter, but emulator doesn't emulate it. */

#include <minix/minlib.h>
#include <minix/sysutil.h>
#include <minix/type.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <assert.h>

#define MICROHZ         1000000ULL	/* number of micros per second */
#define MICROSPERTICK(h)	(MICROHZ/(h)) /* number of micros per HZ tick */

static u64_t Hz;

extern struct minix_kerninfo *_minix_kerninfo;

int
micro_delay(u32_t micros)
{
        u64_t start, delta, delta_end;

	Hz = sys_hz();

        /* Start of delay. */
        read_frclock_64(&start);
	assert(_minix_kerninfo->minix_arm_frclock_hz);
	delta_end = (_minix_kerninfo->minix_arm_frclock_hz * micros) / MICROHZ;

        /* If we have to wait for at least one HZ tick, use the regular
         * tickdelay first. Round downwards on purpose, so the average
         * half-tick we wait short (depending on where in the current tick
         * we call tickdelay). We can correct for both overhead of tickdelay
         * itself and the short wait in the busywait later.
         */
        if (micros >= MICROSPERTICK(Hz))
                tickdelay(micros*Hz/MICROHZ);

        /* Wait (the rest) of the delay time using busywait. */
	do {
                read_frclock_64(&delta);
	} while (delta_frclock_64(start, delta) < delta_end);


        return 0;
}

u32_t frclock_64_to_micros(u64_t tsc)
{
        return (u32_t) (tsc / (_minix_kerninfo->minix_arm_frclock_hz / MICROHZ));
}

void
read_frclock(u32_t *frclk)
{
	assert(frclk);
	assert(_minix_kerninfo->minix_frclock_tcrr);
	assert(_minix_kerninfo->minix_arm_frclock_hz);
	*frclk = *(volatile u32_t *)((u8_t *) _minix_kerninfo->minix_frclock_tcrr);
}

u32_t
delta_frclock(u32_t base, u32_t cur)
{
	u32_t delta;

	if (cur < base) {
		/* We have wrapped around, so delta is base to wrapping point
		 * plus starting point (0) to cur. This supports wrapping once
		 * only. */
		delta = (UINT_MAX - base) + cur;
	} else {
		delta = cur - base;
	}

	return delta;
}

void
read_frclock_64(u64_t *frclk)
{
	read_frclock((u32_t *) frclk);	
}

u64_t
delta_frclock_64(u64_t base, u64_t cur)
{
	return (u64_t) delta_frclock((u32_t) base, (u32_t) cur);
}

