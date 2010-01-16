/*
 * This is arch independent NMI watchdog implementaion part. It is used to
 * detect kernel lockups and help debugging. each architecture must add its own
 * low level code that triggers periodic checks
 */

#include "watchdog.h"

unsigned watchdog_local_timer_ticks;
struct arch_watchdog *watchdog;
int watchdog_enabled;

void nmi_watchdog_handler(struct nmi_frame * frame)
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
		goto reset_and_continue;

	if (last_tick_count != watchdog_local_timer_ticks) {
		if (no_ticks == 1) {
			kprintf("watchdog : kernel unlocked\n");
			no_ticks = 0;
		}
		/* we are still ticking, everything seems good */
		last_tick_count = watchdog_local_timer_ticks;
		goto reset_and_continue;
	}

	/*
	 * if watchdog_local_timer_ticks didn't changed since last time, give it
	 * some more time and only if it still dead, trigger the watchdog alarm
	 */
	if (++no_ticks < 10) {
		if (no_ticks == 1)
			kprintf("WARNING watchdog : possible kernel lockup\n");
		goto reset_and_continue;
	}

	arch_watchdog_lockup(frame);

reset_and_continue:
	if (watchdog->reinit)
		watchdog->reinit(cpuid);
}
