/*
 * This is arch independent NMI watchdog implementation part. It is used to
 * detect kernel lockups and help debugging. each architecture must add its own
 * low level code that triggers periodic checks
 */

#include "watchdog.h"
#include "arch/i386/glo.h"
#include "profile.h"

unsigned watchdog_local_timer_ticks = 0U;
struct arch_watchdog *watchdog;
int watchdog_enabled;

static void lockup_check(struct nmi_frame * frame)
{
	/* FIXME this should be CPU local */
	static unsigned no_ticks;
	static unsigned last_tick_count = (unsigned) -1;

	/*
	 * when debugging on serial console, printing takes a lot of time some
	 * times while the kernel is certainly not locked up. We don't want to
	 * report a lockup in such situation
	 */
	if (serial_debug_active)
		return;

	if (last_tick_count != watchdog_local_timer_ticks) {
		if (no_ticks == 1) {
			printf("watchdog : kernel unlocked\n");
			no_ticks = 0;
		}
		/* we are still ticking, everything seems good */
		last_tick_count = watchdog_local_timer_ticks;
		return;
	}

	/*
	 * if watchdog_local_timer_ticks didn't changed since last time, give it
	 * some more time and only if it still dead, trigger the watchdog alarm
	 */
	if (++no_ticks < 10) {
		if (no_ticks == 1)
			printf("WARNING watchdog : possible kernel lockup\n");
		return;
	}

	/* if we get this far, the kernel is locked up */
	arch_watchdog_lockup(frame);
}

void nmi_watchdog_handler(struct nmi_frame * frame)
{
#if SPROFILE
	/*
	 * Do not check for lockups while profiling, it is extremely likely that
	 * a false positive is detected if the frequency is high
	 */
	if (watchdog_enabled && !sprofiling)
		lockup_check(frame);
	if (sprofiling)
		nmi_sprofile_handler(frame);
	
	if ((watchdog_enabled || sprofiling) && watchdog->reinit)
		watchdog->reinit(cpuid);
#else
	if (watchdog_enabled) {
		lockup_check(frame);
		if (watchdog->reinit)
			watchdog->reinit(cpuid);
	}
#endif
}

int nmi_watchdog_start_profiling(const unsigned freq)
{
	int err;
	
	/* if watchdog hasn't been enabled, we must enable it now */
	if (!watchdog_enabled) {
		if (arch_watchdog_init())
			return ENODEV;
	}

	if (!watchdog->profile_init) {
		printf("WARNING NMI watchdog profiling not supported\n");
		nmi_watchdog_stop_profiling();
		return ENODEV;
	}

	err = watchdog->profile_init(freq);
	if (err != OK)
		return err;

	watchdog->resetval = watchdog->profile_resetval;

	return OK;
}

void nmi_watchdog_stop_profiling(void)
{
	/*
	 * if we do not rearm the NMI source, we are done, if we want to keep
	 * the watchdog running, we reset is to its normal value
	 */

	if (watchdog)
		watchdog->resetval = watchdog->watchdog_resetval;

	if (!watchdog_enabled)
		arch_watchdog_stop();
}
