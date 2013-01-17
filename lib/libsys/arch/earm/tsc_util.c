
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

static u32_t calib_mhz = 1000, Hz = 1000;

int
micro_delay(u32_t micros)
{
	return OK;
}

u32_t tsc_64_to_micros(u64_t tsc)
{
	u64_t tmp;

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

