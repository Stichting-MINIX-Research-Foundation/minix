
#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <minix/u64.h>
#include <minix/config.h>
#include <minix/const.h>

#include "sysutil.h"

#define CALIBRATE_TICKS(h) ((h)/5)
#define MICROHZ		1000000		/* number of micros per second */
#define MICROSPERTICK(h)	(MICROHZ/(h))	/* number of micros per HZ tick */

#define CALIBRATE 						\
	if(!calibrated) {					\
		int r;						\
		if((r=tsc_calibrate()) != OK)			\
			panic("calibrate failed: %d", r); \
	}

static u32_t calib_tsc, Hz = 0;
static int calibrated = 0;

int
tsc_calibrate(void)
{
	u64_t start, end, diff;
	struct tms tms;
	unsigned long t = 0;

	/* Get HZ. */
	Hz = sys_hz();

	/* Wait for clock to tick. */
	while(!t || (t == times(&tms)))
		t = times(&tms);

	t++;

	/* Wait for clock to tick CALIBRATE_TICKS times, and time
	 * this using the TSC.
	 */
	read_tsc_64(&start);
	while(times(&tms) < t+CALIBRATE_TICKS(Hz)) ;
	read_tsc_64(&end);

	diff = sub64(end, start);
	if(ex64hi(diff) != 0)
	  panic("tsc_calibrate: CALIBRATE_TICKS too high for TSC frequency");
	calib_tsc = ex64lo(diff);
#if 0
	printf("tsc_calibrate: "
		"%lu cycles/%d ticks of %d Hz; %lu cycles/s\n",
			calib_tsc, CALIBRATE_TICKS(Hz), Hz,
			div64u(mul64u(calib_tsc, Hz), CALIBRATE_TICKS(Hz)));
#endif
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
	end = add64u(now, div64u(mul64u(calib_tsc,
		micros * Hz / CALIBRATE_TICKS(Hz)), MICROHZ));

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
	return tsc_to_micros(ex64lo(tsc), ex64hi(tsc));
}

u32_t tsc_to_micros(u32_t low, u32_t high)
{
	u32_t micros;

	if(high) {
		return 0;
	}
	CALIBRATE;

	micros = (div64u(mul64u(low, MICROHZ * CALIBRATE_TICKS(Hz)),
		calib_tsc)/Hz);

	return micros;
}

u32_t tsc_get_khz(void)
{
	CALIBRATE;

	return calib_tsc / (CALIBRATE_TICKS(Hz) * MICROSPERTICK(Hz)) * 1000;
}
