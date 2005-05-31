/* The file contais the clock task, which handles all time related functions.
 * Important events that are handled by the CLOCK include alarm timers and
 * (re)scheduling user processes. 
 * The CLOCK offers a direct interface to kernel processes. System services 
 * can access its services through system calls, such as sys_syncalrm(). The
 * CLOCK task thus is hidden for the outside.  
 *
 * Changes:
 *   Mar 18, 2004   clock interface moved to SYSTEM task (Jorrit N. Herder) 
 *   Oct 10, 2004   call vector + return values allowed  (Jorrit N. Herder) 
 *   Sep 30, 2004   source code documentation updated  (Jorrit N. Herder)
 *   Sep 24, 2004   redesigned timers and alarms  (Jorrit N. Herder)
 *   Jun 04, 2004   new timeout flag alarm functionality  (Jorrit N. Herder)
 *
 * The function do_clocktick() is not triggered from the clock library, but 
 * by the clock's interrupt handler when a watchdog timer has expired or 
 * another user process must be scheduled. 
 *
 * In addition to the main clock_task() entry point, which starts the main 
 * loop, there are several other minor entry points:
 *   clock_stop:	called just before MINIX shutdown
 *   get_uptime:	get realtime since boot in clock ticks
 *   set_timer:		set a watchdog timer (*, see note below!)
 *   reset_timer:	reset a watchdog timer (*)
 *   calc_elapsed:	do timing measurements: get delta ticks and pulses
 *   read_clock:	read the counter of channel 0 of the 8253A timer
 *
 * (*) The CLOCK task keeps tracks of watchdog timers for the entire kernel.
 * The watchdog functions of expired timers are executed in do_clocktick(). 
 * It is crucial that watchdog functions cannot block, or the CLOCK task may
 * be blocked. Do not send() a message when the receiver is not expecting it.
 * The use of notify(), which always returns, is strictly preferred! 
 */

#include "kernel.h"
#include "proc.h"
#include <signal.h>
#include <minix/com.h>

/* Function prototype for PRIVATE functions. */ 
FORWARD _PROTOTYPE( void init_clock, (void) );
FORWARD _PROTOTYPE( int clock_handler, (irq_hook_t *hook) );
FORWARD _PROTOTYPE( int do_clocktick, (message *m_ptr) );


/* Constant definitions. */
#define SCHED_RATE (MILLISEC*HZ/1000)	/* number of ticks per schedule */
#define MILLISEC         100		/* how often to call the scheduler */

/* Clock parameters. */
#if (CHIP == INTEL)
#define COUNTER_FREQ (2*TIMER_FREQ) /* counter frequency using square wave */
#define LATCH_COUNT     0x00	/* cc00xxxx, c = channel, x = any */
#define SQUARE_WAVE     0x36	/* ccaammmb, a = access, m = mode, b = BCD */
				/*   11x11, 11 = LSB then MSB, x11 = sq wave */
#define TIMER_COUNT ((unsigned) (TIMER_FREQ/HZ)) /* initial value for counter*/
#define TIMER_FREQ  1193182L	/* clock frequency for timer in PC and AT */

#define CLOCK_ACK_BIT	0x80	/* PS/2 clock interrupt acknowledge bit */
#endif

#if (CHIP == M68000)
#define TIMER_FREQ  2457600L	/* timer 3 input clock frequency */
#endif

/* The CLOCK's timers queue. The functions in <timers.h> operate on this. 
 * The process structure contains one timer per type of alarm (SIGNALRM,
 * SYNCALRM, and FLAGALRM), which means that a process can have a single
 * outstanding timer for each alarm type.
 * If other kernel parts want to use additional timers, they must declare 
 * their own persistent timer structure, which can be passed to the clock
 * via (re)set_timer().
 * When a timer expires its watchdog function is run by the CLOCK task. 
 */
PRIVATE timer_t *clock_timers;		/* queue of CLOCK timers */
PRIVATE clock_t next_timeout;		/* realtime that next timer expires */

/* The boot time and the current real time. The real time is incremented by
 * the clock on each clock tick. The boot time is set by a utility program
 * after system startup to prevent troubles reading the CMOS.  
 */
PRIVATE clock_t realtime;		/* real time clock */

/* Variables for and changed by the CLOCK's interrupt handler. */
PRIVATE irq_hook_t clock_hook;
PRIVATE clock_t pending_ticks;		/* ticks seen by low level only */
PRIVATE int sched_ticks = SCHED_RATE;	/* counter: when 0, call scheduler */
PRIVATE struct proc *prev_ptr;		/* last user process run by clock */


/*===========================================================================*
 *				clock_task				     *
 *===========================================================================*/
PUBLIC void clock_task()
{
/* Main program of clock task. It corrects realtime by adding pending ticks
 * seen only by the interrupt service, then it determines which call this is 
 * by looking at the message type and dispatches.
 */
  message m;			/* message buffer for both input and output */
  int result;
  init_clock();			/* initialize clock task */

  /* Main loop of the clock task.  Get work, process it, sometimes reply. */
  while (TRUE) {
      /* Go get a message. */
      receive(ANY, &m);	

      /* Transfer ticks seen by the low level handler. */
      lock();
      realtime += pending_ticks;	
      pending_ticks = 0;		
      unlock();

      /* Handle the request. */
      switch (m.m_type) {
          case HARD_INT:
              result = do_clocktick(&m);	/* handle clock tick */
              break;
          default:				/* illegal message type */
              kprintf("Warning, illegal CLOCK request from %d.\n", m.m_source);
              result = EBADREQUEST;			
      }

      /* Send reply, unless inhibited, e.g. by do_clocktick(). Use the kernel 
       * function lock_send() to prevent a system call trap. The destination
       * is known to be blocked waiting for a message.  
       */
      if (result != EDONTREPLY) {
          m.m_type = result;
          if (OK != lock_send(m.m_source, &m))
              kprintf("Warning, CLOCK couldn't reply to %d.\n", m.m_source);
      }
  }
}


/*===========================================================================*
 *				do_clocktick				     *
 *===========================================================================*/
PRIVATE int do_clocktick(m_ptr)
message *m_ptr;				/* pointer to request message */
{
/* Despite its name, this routine is not called on every clock tick. It
 * is called on those clock ticks when a lot of work needs to be done.
 */
  register struct proc *rp;
  register int proc_nr;
  timer_t *tp;
  struct proc *p;

  /* Check if a clock timer expired and run its watchdog function. */
  if (next_timeout <= realtime) { 
  	tmrs_exptimers(&clock_timers, realtime);
  	next_timeout = clock_timers == NULL ? 
		TMR_NEVER : clock_timers->tmr_exp_time;
  }

  /* If a process has been running too long, pick another one. */
  if (--sched_ticks <= 0) {
	if (bill_ptr == prev_ptr) 
		lock_sched(PPRI_USER);		/* process has run too long */
	sched_ticks = SCHED_RATE;		/* reset quantum */
	prev_ptr = bill_ptr;			/* new previous process */
  }

  /* Inhibit sending a reply. */
  return(EDONTREPLY);
}


/*===========================================================================*
 *				clock_handler				     *
 *===========================================================================*/
PRIVATE int clock_handler(hook)
irq_hook_t *hook;
{
/* This executes on every clock tick (i.e., every time the timer chip
 * generates an interrupt). It does a little bit of work so the clock
 * task does not have to be called on every tick.
 *
 * Switch context to do_clocktick() if an alarm has gone off.
 * Also switch there to reschedule if the reschedule will do something.
 * This happens when
 *	(1) quantum has expired
 *	(2) current process received full quantum (as clock sampled it!)
 *	(3) something else is ready to run.
 *
 * Many global global and static variables are accessed here.  The safety
 * of this must be justified.  Most of them are not changed here:
 *	proc_ptr, bill_ptr:
 *		These are used for accounting.  It does not matter if proc.c
 *		is changing them, provided they are always valid pointers,
 *		since at worst the previous process would be billed.
 *	next_timeout, realtime, sched_ticks, bill_ptr, prev_ptr
 *	rdy_head[PPRI_USER]
 *		These are tested to decide whether to call notify().  It
 *		does not matter if the test is sometimes (rarely) backwards
 *		due to a race, since this will only delay the high-level
 *		processing by one tick, or call the high level unnecessarily.
 * The variables which are changed require more care:
 *	rp->p_user_time, rp->p_sys_time:
 *		These are protected by explicit locks in system.c.  They are
 *		not properly protected in dmp.c (the increment here is not
 *		atomic) but that hardly matters.
 *	pending_ticks:
 *		This is protected by explicit locks in clock.c.  Don't
 *		update realtime directly, since there are too many
 *		references to it to guard conveniently.
 *	lost_ticks:
 *		Clock ticks counted outside the clock task.
 *	sched_ticks, prev_ptr:
 *		Updating these competes with similar code in do_clocktick().
 *		No lock is necessary, because if bad things happen here
 *		(like sched_ticks going negative), the code in do_clocktick()
 *		will restore the variables to reasonable values, and an
 *		occasional missed or extra sched() is harmless.
 *
 * Are these complications worth the trouble?  Well, they make the system 15%
 * faster on a 5MHz 8088, and make task debugging much easier since there are
 * no task switches on an inactive system.
 */
  register struct proc *rp;
  register unsigned ticks;
  message m;
  clock_t now;

  /* Acknowledge the PS/2 clock interrupt. */
  if (machine.ps_mca) outb(PORT_B, inb(PORT_B) | CLOCK_ACK_BIT);

  /* Update user and system accounting times. Charge the current process for
   * user time. If the current process is not billable, that is, if a non-user
   * process is running, charge the billable process for system time as well.
   * Thus the unbillable process' user time is the billable user's system time.
   */
  ticks = lost_ticks + 1;
  lost_ticks = 0;
  pending_ticks += ticks;
  now = realtime + pending_ticks;

  /* Update administration. */
  proc_ptr->p_user_time += ticks;
  if (proc_ptr != bill_ptr) bill_ptr->p_sys_time += ticks;

  /* Check if do_clocktick() must be called. Done for alarms and scheduling.
   * If bill_ptr == prev_ptr, there are no ready users so don't need sched(). 
   */ 
  if (next_timeout <= now || (sched_ticks == 1 && bill_ptr == prev_ptr
          && rdy_head[PPRI_USER] != NIL_PROC))
  {  
      m.NOTIFY_TYPE = HARD_INT;
      lock_notify(CLOCK, &m);
  } 
  else if (--sched_ticks <= 0) {
      sched_ticks = SCHED_RATE;		/* reset the quantum */
      prev_ptr = bill_ptr;		/* new previous process */
  }
  return(1);				/* reenable clock interrupts */
}


/*===========================================================================*
 *				get_uptime				     *
 *===========================================================================*/
PUBLIC clock_t get_uptime()
{
/* Get and return the current clock uptime in ticks.
 * Be careful about pending_ticks.
 */
  clock_t uptime;

  lock();
  uptime = realtime + pending_ticks;
  unlock();
  return(uptime);
}


/*===========================================================================*
 *				set_timer				     *
 *===========================================================================*/
PUBLIC void set_timer(tp, exp_time, watchdog)
struct timer *tp;		/* pointer to timer structure */
clock_t exp_time;		/* expiration realtime */
tmr_func_t watchdog;		/* watchdog to be called */
{
/* Insert the new timer in the active timers list. Always update the 
 * next timeout time by setting it to the front of the active list.
 */
  tmrs_settimer(&clock_timers, tp, exp_time, watchdog);
  next_timeout = clock_timers->tmr_exp_time;
}


/*===========================================================================*
 *				reset_timer				     *
 *===========================================================================*/
PUBLIC void reset_timer(tp)
struct timer *tp;		/* pointer to timer structure */
{
/* The timer pointed to by 'tp' is no longer needed. Remove it from both the
 * active and expired lists. Always update the next timeout time by setting
 * it to the front of the active list.
 */
  tmrs_clrtimer(&clock_timers, tp);
  next_timeout = (clock_timers == NULL) ? 
	TMR_NEVER : clock_timers->tmr_exp_time;
}


#if (CHIP == INTEL)

/*===========================================================================*
 *				init_clock				     *
 *===========================================================================*/
PRIVATE void init_clock()
{
  /* Initialize the CLOCK's interrupt hook. */
  clock_hook.proc_nr = CLOCK;

  /* Initialize channel 0 of the 8253A timer to, e.g., 60 Hz. */
  outb(TIMER_MODE, SQUARE_WAVE);	/* set timer to run continuously */
  outb(TIMER0, TIMER_COUNT);		/* load timer low byte */
  outb(TIMER0, TIMER_COUNT >> 8);	/* load timer high byte */
  put_irq_handler(&clock_hook, CLOCK_IRQ, clock_handler);/* register handler */
  enable_irq(&clock_hook);		/* ready for clock interrupts */
}

/*===========================================================================*
 *				clock_stop				     *
 *===========================================================================*/
PUBLIC void clock_stop()
{
/* Reset the clock to the BIOS rate. (For rebooting) */
  outb(TIMER_MODE, 0x36);
  outb(TIMER0, 0);
  outb(TIMER0, 0);
}


/*===========================================================================*
 *				read_clock				     *
 *===========================================================================*/
PUBLIC unsigned long read_clock()
{
/* Read the counter of channel 0 of the 8253A timer.  This counter counts
 * down at a rate of TIMER_FREQ and restarts at TIMER_COUNT-1 when it
 * reaches zero. A hardware interrupt (clock tick) occurs when the counter
 * gets to zero and restarts its cycle.  
 */
  unsigned count;

  lock();
  outb(TIMER_MODE, LATCH_COUNT);
  count = inb(TIMER0);
  count |= (inb(TIMER0) << 8);
  unlock();
  
  return count;
}

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* Initialize the timer C in the MFP 68901: implement init_clock() here. */
#endif /* (CHIP == M68000) */


