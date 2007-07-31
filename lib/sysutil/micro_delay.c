
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <minix/u64.h>
#include <minix/config.h>
#include <minix/const.h>

#include "sysutil.h"

#define CALIBRATE_TICKS (HZ/5)
#define MICROHZ		1000000		/* number of micros per second */
#define MICROSPERTICK	(MICROHZ/HZ)	/* number of micros per HZ tick */

static u32_t calib_tsc;
static int calibrated = 0;

int
micro_delay_calibrate(void)
{
	u64_t start, end, diff, hz;
	struct tms tms;
	unsigned long t = 0;

	/* Wait for clock to tick. */
	while(!t || (t == times(&tms)))
		t = times(&tms);

	t++;

	/* Wait for clock to tick CALIBRATE_TICKS times, and time
	 * this using the TSC.
	 */
	read_tsc_64(&start);
	while(times(&tms) < t+CALIBRATE_TICKS) ;
	read_tsc_64(&end);

	diff = sub64(end, start);
	if(ex64hi(diff) != 0)
	  panic(__FILE__,
		"micro_delay_calibrate: CALIBRATE_TICKS too high "
			"for TSC frequency\n", NO_NUM);
	calib_tsc = ex64lo(diff);
	printf("micro_delay_calibrate: "
		"%lu cycles/%d ticks of %d Hz; %lu cycles/s\n",
			calib_tsc, CALIBRATE_TICKS, HZ,
			div64u(mul64u(calib_tsc, HZ), CALIBRATE_TICKS));
	calibrated = 1;

	return OK;
}

int
micro_delay(u32_t micros)
{
	u64_t now, end;

	/* Start of delay. */
	read_tsc_64(&now);

	/* We have to be calibrated. */
	if(!calibrated) {
		int r;
		printf("micro_delay: calibrating\n");
		if((r=micro_delay_calibrate()) != OK)
			panic(__FILE__, "micro_delay: calibrate failed\n", r);
	}

	/* We have to know when to end the delay. */
	end = add64u(now, div64u(mul64u(calib_tsc,
		micros * HZ / CALIBRATE_TICKS), MICROHZ));

	/* If we have to wait for at least one HZ tick, use the regular
	 * tickdelay first. Round downwards on purpose, so the average
	 * half-tick we wait short (depending on where in the current tick
	 * we call tickdelay). We can correct for both overhead of tickdelay
	 * itself and the short wait in the busywait later.
	 */
	if(micros >= MICROSPERTICK)
		tickdelay(micros*HZ/MICROHZ);

	/* Wait (the rest) of the delay time using busywait. */
	while(cmp64(now, end) < 0)
		read_tsc_64(&now);

	return OK;
}
