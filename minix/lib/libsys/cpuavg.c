/*
 * Routines to maintain a decaying average of per-process CPU utilization, in a
 * way that results in numbers that are (hopefully) similar to those produced
 * by NetBSD.  Once a second, NetBSD performs the following basic computation
 * for each process:
 *
 *   avg = ccpu * avg + (1 - ccpu) * (run / hz)
 *
 * In this formula, 'avg' is the running average, 'hz' is the number of clock
 * ticks per second, 'run' is the number of ticks during which the process was
 * found running in the last second, and 'ccpu' is a decay value chosen such
 * that only 5% of the original average remains after 60 seconds: e**(-1/20).
 *
 * Here, the idea is that we update the average lazily, namely, only when the
 * process is running when the kernel processes a clock tick - no matter how
 * long it had not been running before that.  The result is that at any given
 * time, the average may be out of date.  For that reason, this code is shared
 * between the kernel and the MIB service: the latter occasionally obtains the
 * raw kernel process table, for example because a user runs ps(1), and it then
 * needs to bring the values up to date.  The kernel could do that itself just
 * before copying out the process table, but the MIB service is equally capable
 * of doing it post-copy - while also being preemptible during the computation.
 * There is more to be said about this, but the summary is that it is not clear
 * which of the two options is better in practice.  We simply chose this one.
 *
 * In addition, we deliberately delay updating the actual average by one
 * second, keeping the last second's number of process run ticks in a separate
 * variable 'last'.  This allows us to produce an estimate of short-term
 * activity of the process as well.  We use this to generate a "CPU estimate"
 * value.  BSD generates such a value for the purpose of scheduling, but we
 * have no actual use for that, and generating the value just for userland is
 * a bit too costly in our case.  Our inaccurate value should suffice for most
 * practical purposes though (e.g., comparisons between active processes).
 *
 * Overall, in terms of overhead, our approach should produce the same values
 * as NetBSD while having only the same overhead as NetBSD in the very worst
 * case, and much less overhead on average.  Even in the worst case, in our
 * case, the computation is spread out across each second, rather than all done
 * at once.  In terms of implementation, since this code is running in the
 * kernel, we make use of small tables of precomputed values, and we try to
 * save on computation as much as possible.  We copy much of the NetBSD
 * approach of avoiding divisions using FSCALE.
 *
 * Another difference with NetBSD is that our kernel does not actually call
 * this function from its clock interrupt handler, but rather when a process
 * has spent a number of CPU cycles that adds up to one clock tick worth of
 * execution time.  The result is better accuracy (no process can escape
 * accounting by yielding just before each clock interrupt), but due to the
 * inaccuracy of converting CPU cycles to clock ticks, a process may end up
 * using more than 'hz' clock ticks per second.  We could correct for this;
 * however, it has not yet shown to be a problem.
 *
 * Zooming out a bit again, the current average is fairly accurate but not
 * very precise.  There are two reasons for this.  First, the accounting is in
 * clock tick fractions, which means that a per-second CPU usage below 1/hz
 * cannot be measured.  Second, the NetBSD FSCALE and ccpu values are such that
 * (FSCALE - ccpu) equals 100, which means that a per-second CPU usage below
 * 1/100 cannot be measured either.  Both issues can be resolved by switching
 * to a CPU cycle based accounting approach, which requires 64-bit arithmetic
 * and a MINIX3-specific FSCALE value.  For now, this is just not worth doing.
 *
 * Finally, it should be noted that in terms of overall operating system
 * functionality, the CPU averages feature is entirely optional; as of writing,
 * the produced values are only used in the output of utilities such as ps(1).
 * If computing the CPU average becomes too burdensome in terms of either
 * performance or maintenance, it can simply be removed again.
 *
 * Original author: David van Moolenbroek <david@minix3.org>
 */

#include "sysutil.h"
#include <sys/param.h>

#define CCPUTAB_SHIFT	3				/* 2**3 == 8 */
#define CCPUTAB_MASK	((1 << CCPUTAB_SHIFT) - 1)

#define F(n) ((uint32_t)((n) * FSCALE))

/* e**(-1/20*n)*FSCALE for n=1..(2**CCPUTAB_SHIFT-1) */
static const uint32_t ccpu_low[CCPUTAB_MASK] = {
	F(0.951229424501), F(0.904837418036), F(0.860707976425),
	F(0.818730753078), F(0.778800783071), F(0.740818220682),
	F(0.704688089719)
};
#define ccpu		(ccpu_low[0])

/* e**(-1/20*8*n)*FSCALE for n=1.. until the value is zero (for FSCALE=2048) */
static const uint32_t ccpu_high[] = {
	F(0.670320046036), F(0.449328964117), F(0.301194211912),
	F(0.201896517995), F(0.135335283237), F(0.090717953289),
	F(0.060810062625), F(0.040762203978), F(0.027323722447),
	F(0.018315638889), F(0.012277339903), F(0.008229747049),
	F(0.005516564421), F(0.003697863716), F(0.002478752177),
	F(0.001661557273), F(0.001113775148), F(0.000746585808),
	F(0.000500451433)
};

/*
 * Initialize the per-process CPU average structure.  To be called when the
 * process is started, that is, as part of a fork call.
 */
void
cpuavg_init(struct cpuavg * ca)
{

	ca->ca_base = 0;
	ca->ca_run = 0;
	ca->ca_last = 0;
	ca->ca_avg = 0;
}

/*
 * Return a new CPU usage average value, resulting from decaying the old value
 * by the given number of seconds, using the formula (avg * ccpu**secs).
 * We use two-level lookup tables to limit the computational expense to two
 * multiplications while keeping the tables themselves relatively small.
 */
static uint32_t
cpuavg_decay(uint32_t avg, uint32_t secs)
{
	unsigned int slot;

	/*
	 * The ccpu_high table is set up such that with the default FSCALE, the
	 * values of any array entries beyond the end would be zero.  That is,
	 * the average would be decayed to a value that, if represented in
	 * FSCALE units, would be zero.  Thus, if it has been that long ago
	 * that we updated the average, we can just reset it to zero.
	 */
	if (secs > (__arraycount(ccpu_high) << CCPUTAB_SHIFT))
		return 0;

	if (secs > CCPUTAB_MASK) {
		slot = (secs >> CCPUTAB_SHIFT) - 1;

		avg = (ccpu_high[slot] * avg) >> FSHIFT;	/* decay #3 */

		secs &= CCPUTAB_MASK;
	}

	if (secs > 0)
		avg = (ccpu_low[secs - 1] * avg) >> FSHIFT;	/* decay #4 */

	return avg;
}

/*
 * Update the CPU average value, either because the kernel is processing a
 * clock tick, or because the MIB service updates obtained averages.  We
 * perform the decay in at most four computation steps (shown as "decay #n"),
 * and thus, this algorithm is O(1).
 */
static void
cpuavg_update(struct cpuavg * ca, clock_t now, clock_t hz)
{
	clock_t delta;
	uint32_t secs;

	delta = now - ca->ca_base;

	/*
	 * If at least a second elapsed since we last updated the average, we
	 * must do so now.  If not, we need not do anything for now.
	 */
	if (delta < hz)
		return;

	/*
	 * Decay the average by one second, and merge in the run fraction of
	 * the previous second, as though that second only just ended - even
	 * though the real time is at least one whole second ahead.  By doing
	 * so, we roll the statistics time forward by one virtual second.
	 */
	ca->ca_avg = (ccpu * ca->ca_avg) >> FSHIFT;		/* decay #1 */
	ca->ca_avg += (FSCALE - ccpu) * (ca->ca_last / hz) >> FSHIFT;

	ca->ca_last = ca->ca_run;	/* move 'run' into 'last' */
	ca->ca_run = 0;

	ca->ca_base += hz;		/* move forward by a second */
	delta -= hz;

	if (delta < hz)
		return;

	/*
	 * At least a whole second more elapsed since the start of the recorded
	 * second.  That means that our current 'run' counter (now moved into
	 * 'last') is also outdated, and we need to merge it in as well, before
	 * performing the next decay steps.
	 */
	ca->ca_avg = (ccpu * ca->ca_avg) >> FSHIFT;		/* decay #2 */
	ca->ca_avg += (FSCALE - ccpu) * (ca->ca_last / hz) >> FSHIFT;

	ca->ca_last = 0;		/* 'run' is already zero now */

	ca->ca_base += hz;		/* move forward by a second */
	delta -= hz;

	if (delta < hz)
		return;

	/*
	 * If additional whole seconds elapsed since the start of the last
	 * second slot, roll forward in time by that many whole seconds, thus
	 * decaying the value properly while maintaining alignment to whole-
	 * second slots.  The decay takes up to another two computation steps.
	 */
	secs = delta / hz;

	ca->ca_avg = cpuavg_decay(ca->ca_avg, secs);

	ca->ca_base += secs * hz;	/* move forward by whole seconds */
}

/*
 * The clock ticked, and this last clock tick is accounted to the process for
 * which the CPU average statistics are stored in 'ca'.  Update the statistics
 * accordingly, decaying the average as necessary.  The current system uptime
 * must be given as 'now', and the number of clock ticks per second must be
 * given as 'hz'.
 */
void
cpuavg_increment(struct cpuavg * ca, clock_t now, clock_t hz)
{

	if (ca->ca_base == 0)
		ca->ca_base = now;
	else
		cpuavg_update(ca, now, hz);

	/*
	 * Register that the process was running at this clock tick.  We could
	 * avoid one division above by precomputing (FSCALE/hz), but this is
	 * typically not a clean division and would therefore result in (more)
	 * loss of accuracy.
	 */
	ca->ca_run += FSCALE;
}

/*
 * Retrieve the decaying CPU utilization average (as return value), the number
 * of CPU run ticks in the current second so far (stored in 'cpticks'), and an
 * opaque CPU utilization estimate (stored in 'estcpu').  The caller must
 * provide the CPU average structure ('ca_orig'), which will not be modified,
 * as well as the current uptime in clock ticks ('now') and the number of clock
 * ticks per second ('hz').
 */
uint32_t
cpuavg_getstats(const struct cpuavg * ca_orig, uint32_t * cpticks,
	uint32_t * estcpu, clock_t now, clock_t hz)
{
	struct cpuavg ca;

	ca = *ca_orig;

	/* Update the average as necessary. */
	cpuavg_update(&ca, now, hz);

	/* Merge the last second into the average. */
	ca.ca_avg = (ccpu * ca.ca_avg) >> FSHIFT;
	ca.ca_avg += (FSCALE - ccpu) * (ca.ca_last / hz) >> FSHIFT;

	*cpticks = ca.ca_run >> FSHIFT;

	/*
	 * NetBSD's estcpu value determines a scheduling queue, and decays to
	 * 10% in 5*(the current load average) seconds.  Our 'estcpu' simply
	 * reports the process's percentage of CPU usage in the last second,
	 * thus yielding a value in the range 0..100 with a decay of 100% after
	 * one second.  This should be good enough for most practical purposes.
	 */
	*estcpu = (ca.ca_last / hz * 100) >> FSHIFT;

	return ca.ca_avg;
}

/*
 * Return the ccpu decay value, in FSCALE units.
 */
uint32_t
cpuavg_getccpu(void)
{

	return ccpu;
}
