/* This file contains the clock task, which handles time related functions.
 * Important events that are handled by the CLOCK include setting and 
 * monitoring alarm timers and deciding when to (re)schedule processes. 
 * The CLOCK offers a direct interface to kernel processes. System services 
 * can access its services through system calls, such as sys_setalarm(). The
 * CLOCK task thus is hidden from the outside world.  
 *
 * Changes:
 *   Aug 18, 2006   removed direct hardware access etc, MinixPPC (Ingmar Alting)
 *   Oct 08, 2005   reordering and comment editing (A. S. Woodhull)
 *   Mar 18, 2004   clock interface moved to SYSTEM task (Jorrit N. Herder) 
 *   Sep 30, 2004   source code documentation updated  (Jorrit N. Herder)
 *   Sep 24, 2004   redesigned alarm timers  (Jorrit N. Herder)
 *
 * Clock task is notified by the clock's interrupt handler when a timer
 * has expired.
 *
 * In addition to the main clock_task() entry point, which starts the main 
 * loop, there are several other minor entry points:
 *   clock_stop:		called just before MINIX shutdown
 *   get_realtime:		get wall time since boot in clock ticks
 *   set_realtime:		set wall time since boot in clock ticks
 *   set_adjtime_delta:		set the number of ticks to adjust realtime
 *   get_monotonic:		get monotonic time since boot in clock ticks
 *   set_kernel_timer:		set a watchdog timer (+)
 *   reset_kernel_timer:	reset a watchdog timer (+)
 *   read_clock:		read the counter of channel 0 of the 8253A timer
 *
 * (+) The CLOCK task keeps tracks of watchdog timers for the entire kernel.
 * It is crucial that watchdog functions not block, or the CLOCK task may
 * be blocked. Do not send() a message when the receiver is not expecting it.
 * Instead, notify(), which always returns, should be used. 
 */

#include "kernel/kernel.h"
#include <minix/endpoint.h>
#include <assert.h>

#include "clock.h"

#ifdef USE_WATCHDOG
#include "watchdog.h"
#endif

/* Function prototype for PRIVATE functions.
 */ 
static void load_update(void);

/* The CLOCK's timers queue. The functions in <minix/timers.h> operate on this.
 * Each system process possesses a single synchronous alarm timer. If other 
 * kernel parts want to use additional timers, they must declare their own 
 * persistent (static) timer structure, which can be passed to the clock
 * via (re)set_kernel_timer().
 * When a timer expires its watchdog function is run by the CLOCK task. 
 */
static minix_timer_t *clock_timers;	/* queue of CLOCK timers */
static clock_t next_timeout;	/* monotonic time that next timer expires */

/* The time is incremented by the interrupt handler on each clock tick.
 */
static clock_t monotonic = 0;

/* Reflects the wall time and may be slowed/sped up by using adjclock()
 */
static clock_t realtime = 0;

/* Number of ticks to adjust realtime by. A negative value implies slowing
 * down realtime, a positive value implies speeding it up.
 */
static int32_t adjtime_delta = 0;

/*
 * The boot processor's timer interrupt handler. In addition to non-boot cpus
 * it keeps real time and notifies the clock task if need be.
 */
int timer_int_handler(void)
{
	/* Update user and system accounting times. Charge the current process
	 * for user time. If the current process is not billable, that is, if a
	 * non-user process is running, charge the billable process for system
	 * time as well.  Thus the unbillable process' user time is the billable
	 * user's system time.
	 */

	struct proc * p, * billp;

	/* FIXME watchdog for slave cpus! */
#ifdef USE_WATCHDOG
	/*
	 * we need to know whether local timer ticks are happening or whether
	 * the kernel is locked up. We don't care about overflows as we only
	 * need to know that it's still ticking or not
	 */
	watchdog_local_timer_ticks++;
#endif

	if (cpu_is_bsp(cpuid)) {
		monotonic++;

		/* if adjtime_delta has ticks remaining, apply one to realtime.
		 * limit changes to every other interrupt.
		 */
		if (adjtime_delta != 0 && monotonic & 0x1) {
			/* go forward or stay behind */
			realtime += (adjtime_delta > 0) ? 2 : 0;
			adjtime_delta += (adjtime_delta > 0) ? -1 : +1;
		} else {
			realtime++;
		}
	}

	/* Update user and system accounting times. Charge the current process
	 * for user time. If the current process is not billable, that is, if a
	 * non-user process is running, charge the billable process for system
	 * time as well.  Thus the unbillable process' user time is the billable
	 * user's system time.
	 */

	p = get_cpulocal_var(proc_ptr);
	billp = get_cpulocal_var(bill_ptr);

	p->p_user_time++;

	if (! (priv(p)->s_flags & BILLABLE)) {
		billp->p_sys_time++;
	}

	/* Decrement virtual timers, if applicable. We decrement both the
	 * virtual and the profile timer of the current process, and if the
	 * current process is not billable, the timer of the billed process as
	 * well.  If any of the timers expire, do_clocktick() will send out
	 * signals.
	 */
	if ((p->p_misc_flags & MF_VIRT_TIMER) && (p->p_virt_left > 0)) {
		p->p_virt_left--;
	}
	if ((p->p_misc_flags & MF_PROF_TIMER) && (p->p_prof_left > 0)) {
		p->p_prof_left--;
	}
	if (! (priv(p)->s_flags & BILLABLE) &&
			(billp->p_misc_flags & MF_PROF_TIMER) &&
			(billp->p_prof_left > 0)) {
		billp->p_prof_left--;
	}

	/*
	 * Check if a process-virtual timer expired. Check current process, but
	 * also bill_ptr - one process's user time is another's system time, and
	 * the profile timer decreases for both!
	 */
	vtimer_check(p);

	if (p != billp)
		vtimer_check(billp);

	/* Update load average. */
	load_update();

	if (cpu_is_bsp(cpuid)) {
		/* if a timer expired, notify the clock task */
		if ((next_timeout <= monotonic)) {
			tmrs_exptimers(&clock_timers, monotonic, NULL);
			next_timeout = (clock_timers == NULL) ?
				TMR_NEVER : clock_timers->tmr_exp_time;
		}

#ifdef DEBUG_SERIAL
		if (kinfo.do_serial_debug)
			do_ser_debug();
#endif

	}

	arch_timer_int_handler();

	return(1);					/* reenable interrupts */
}

/*===========================================================================*
 *				get_realtime				     *
 *===========================================================================*/
clock_t get_realtime(void)
{
  /* Get and return the current wall time in ticks since boot. */
  return(realtime);
}

/*===========================================================================*
 *				set_realtime				     *
 *===========================================================================*/
void set_realtime(clock_t newrealtime)
{
  realtime = newrealtime;
}

/*===========================================================================*
 *				set_adjtime_delta			     *
 *===========================================================================*/
void set_adjtime_delta(int32_t ticks)
{
  adjtime_delta = ticks;
}

/*===========================================================================*
 *				get_monotonic				     *
 *===========================================================================*/
clock_t get_monotonic(void)
{
  /* Get and return the number of ticks since boot. */
  return(monotonic);
}

/*===========================================================================*
 *				set_kernel_timer			     *
 *===========================================================================*/
void set_kernel_timer(tp, exp_time, watchdog)
minix_timer_t *tp;		/* pointer to timer structure */
clock_t exp_time;		/* expiration monotonic time */
tmr_func_t watchdog;		/* watchdog to be called */
{
/* Insert the new timer in the active timers list. Always update the 
 * next timeout time by setting it to the front of the active list.
 */
  tmrs_settimer(&clock_timers, tp, exp_time, watchdog, NULL);
  next_timeout = clock_timers->tmr_exp_time;
}

/*===========================================================================*
 *				reset_kernel_timer			     *
 *===========================================================================*/
void reset_kernel_timer(tp)
minix_timer_t *tp;		/* pointer to timer structure */
{
/* The timer pointed to by 'tp' is no longer needed. Remove it from both the
 * active and expired lists. Always update the next timeout time by setting
 * it to the front of the active list.
 */
  tmrs_clrtimer(&clock_timers, tp, NULL);
  next_timeout = (clock_timers == NULL) ? 
	TMR_NEVER : clock_timers->tmr_exp_time;
}

/*===========================================================================*
 *				load_update				     * 
 *===========================================================================*/
static void load_update(void)
{
	u16_t slot;
	int enqueued = 0, q;
	struct proc *p;
	struct proc **rdy_head;

	/* Load average data is stored as a list of numbers in a circular
	 * buffer. Each slot accumulates _LOAD_UNIT_SECS of samples of
	 * the number of runnable processes. Computations can then
	 * be made of the load average over variable periods, in the
	 * user library (see getloadavg(3)).
	 */
	slot = (monotonic / system_hz / _LOAD_UNIT_SECS) % _LOAD_HISTORY;
	if(slot != kloadinfo.proc_last_slot) {
		kloadinfo.proc_load_history[slot] = 0;
		kloadinfo.proc_last_slot = slot;
	}

	rdy_head = get_cpulocal_var(run_q_head);
	/* Cumulation. How many processes are ready now? */
	for(q = 0; q < NR_SCHED_QUEUES; q++) {
		for(p = rdy_head[q]; p != NULL; p = p->p_nextready) {
			enqueued++;
		}
	}

	kloadinfo.proc_load_history[slot] += enqueued;

	/* Up-to-dateness. */
	kloadinfo.last_clock = monotonic;
}

int boot_cpu_init_timer(unsigned freq)
{
	if (init_local_timer(freq))
		return -1;

	if (register_local_timer_handler(
				(irq_handler_t) timer_int_handler))
		return -1;

	return 0;
}

int app_cpu_init_timer(unsigned freq)
{
	if (init_local_timer(freq))
		return -1;

	return 0;
}
