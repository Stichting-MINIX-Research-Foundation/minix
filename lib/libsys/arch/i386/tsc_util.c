
#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <minix/u64.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/minlib.h>
#include <machine/archtypes.h>

#include "sysutil.h"

#ifndef CONFIG_MAX_CPUS
#define CONFIG_MAX_CPUS 1
#endif

#define MICROHZ		1000000		/* number of micros per second */
#define MICROSPERTICK(h)	(MICROHZ/(h))	/* number of micros per HZ tick */

#define CALIBRATE 						\
	if(!calibrated) {					\
		int r;						\
		if((r=tsc_calibrate()) != OK)			\
			panic("calibrate failed: %d", r); \
	}

static u32_t calib_mhz, Hz = 0;
static int calibrated = 0;

int
tsc_calibrate(void)
{
	struct cpu_info cpu_info[CONFIG_MAX_CPUS];

	/* Get HZ. */
	Hz = sys_hz();

	/* Obtain CPU frequency from kernel */
	if (sys_getcpuinfo(&cpu_info)) {
		printf("tsc_calibrate: cannot get cpu info\n");
		return -1;
	}
	
	/* For now, use the frequency of the first CPU; everything here will 
	 * break down in case we get scheduled on multiple CPUs with different 
	 * frequencies regardless
	 */
	calib_mhz = cpu_info[0].freq;
	calibrated = 1;

	return OK;
}

int
micro_delay(u32_t micros)
{
	u64_t now, end;

	/* Start of delay. */
	read_tsc_64(&now);

	CALIBRATE;

	/* We have to know when to end the delay. */
	end = add64(now, mul64u(micros, calib_mhz));

	/* If we have to wait for at least one HZ tick, use the regular
	 * tickdelay first. Round downwards on purpose, so the average
	 * half-tick we wait short (depending on where in the current tick
	 * we call tickdelay). We can correct for both overhead of tickdelay
	 * itself and the short wait in the busywait later.
	 */
	if(micros >= MICROSPERTICK(Hz))
		tickdelay(micros*Hz/MICROHZ);

	/* Wait (the rest) of the delay time using busywait. */
	while(cmp64(now, end) < 0)
		read_tsc_64(&now);

	return OK;
}

u32_t tsc_64_to_micros(u64_t tsc)
{
	u64_t tmp;

	CALIBRATE;

	tmp = div64u64(tsc, calib_mhz);
	if (ex64hi(tmp)) {
		printf("tsc_64_to_micros: more than 2^32ms\n");
		return ~0UL;
	} else {
		return ex64lo(tmp);
	}
}

u32_t tsc_to_micros(u32_t low, u32_t high)
{
	return tsc_64_to_micros(make64(low, high));
}

u32_t tsc_get_khz(void)
{
	CALIBRATE;

	return calib_mhz * 1000;
}

#define frclock_64_to_micros tsc_64_to_micros
#define read_frclock_64 read_tsc_64

u64_t delta_frclock_64(u64_t base, u64_t cur)
{
        return cur - base;
}

