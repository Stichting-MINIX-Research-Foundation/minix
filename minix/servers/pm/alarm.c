/* This file deals with the alarm clock related system calls, eventually
 * passing off the work to the functions in timers.c and check_sig() in
 * signal.c to pass an alarm signal to a process.
 *
 * The entry points into this file are:
 *   do_itimer: perform the ITIMER system call
 *   set_alarm: tell the timer interface to start or stop a process timer
 *   check_vtimer: check if one of the virtual timers needs to be restarted
 */

#include "pm.h"
#include <signal.h>
#include <sys/time.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include "mproc.h"

#define US 1000000UL	/* shortcut for microseconds per second */

static clock_t ticks_from_timeval(struct timeval *tv);
static void timeval_from_ticks(struct timeval *tv, clock_t ticks);
static int is_sane_timeval(struct timeval *tv);
static void getset_vtimer(struct mproc *mp, int nwhich, struct
	itimerval *value, struct itimerval *ovalue);
static void get_realtimer(struct mproc *mp, struct itimerval *value);
static void set_realtimer(struct mproc *mp, struct itimerval *value);
static void cause_sigalrm(minix_timer_t *tp);

/*===========================================================================*
 *				ticks_from_timeval			     * 
 *===========================================================================*/
static clock_t ticks_from_timeval(tv)
struct timeval *tv;
{
  clock_t ticks;

  /* Large delays cause a lot of problems.  First, the alarm system call
   * takes an unsigned seconds count and the library has cast it to an int.
   * That probably works, but on return the library will convert "negative"
   * unsigneds to errors.  Presumably no one checks for these errors, so
   * force this call through.  Second, If unsigned and long have the same
   * size, converting from seconds to ticks can easily overflow.  Finally,
   * the kernel has similar overflow bugs adding ticks.
   *
   * Fixing this requires a lot of ugly casts to fit the wrong interface
   * types and to avoid overflow traps.  ALRM_EXP_TIME has the right type
   * (clock_t) although it is declared as long.  How can variables like
   * this be declared properly without combinatorial explosion of message
   * types?
   */
	
  /* In any case, the following conversion must always round up. */

  ticks = system_hz * (unsigned long) tv->tv_sec;
  if ( (ticks / system_hz) != (unsigned long)tv->tv_sec) {
	ticks = LONG_MAX;
  } else {
	ticks += ((system_hz * (unsigned long)tv->tv_usec + (US-1)) / US);
  }

  if (ticks > LONG_MAX) ticks = LONG_MAX;

  return(ticks);
}

/*===========================================================================*
 *				timeval_from_ticks			     * 
 *===========================================================================*/
static void timeval_from_ticks(tv, ticks)
struct timeval *tv;
clock_t ticks;
{
  tv->tv_sec = (long) (ticks / system_hz);
  tv->tv_usec = (long) ((ticks % system_hz) * US / system_hz);
}

/*===========================================================================*
 *				is_sane_timeval				     * 
 *===========================================================================*/
static int is_sane_timeval(tv)
struct timeval *tv;
{
  /* This imposes a reasonable time value range for setitimer. */
  return (tv->tv_sec >= 0 && tv->tv_sec <= MAX_SECS &&
 	  tv->tv_usec >= 0 && tv->tv_usec < US);
}
 
/*===========================================================================*
 *				do_itimer				     *
 *===========================================================================*/
int do_itimer()
{
  struct itimerval ovalue, value;	/* old and new interval timers */
  int setval, getval;			/* set and/or retrieve the values? */
  int r, which;

  /* Make sure 'which' is one of the defined timers. */
  which = m_in.m_lc_pm_itimer.which;
  if (which < 0 || which >= NR_ITIMERS) return(EINVAL);

  /* Determine whether to set and/or return the given timer value, based on
   * which of the value and ovalue parameters are nonzero. At least one of
   * them must be nonzero.
   */
  setval = (m_in.m_lc_pm_itimer.value != 0);
  getval = (m_in.m_lc_pm_itimer.ovalue != 0);

  if (!setval && !getval) return(EINVAL);

  /* If we're setting a new value, copy the new timer from user space.
   * Also, make sure its fields have sane values.
   */
  if (setval) {
	r = sys_datacopy(who_e, m_in.m_lc_pm_itimer.value,
		PM_PROC_NR, (vir_bytes)&value, (phys_bytes)sizeof(value));
  	if (r != OK) return(r);

  	if (!is_sane_timeval(&value.it_value) ||
  	    !is_sane_timeval(&value.it_interval))
  		return(EINVAL);
  }

  switch (which) {
  	case ITIMER_REAL :
  		if (getval) get_realtimer(mp, &ovalue);

  		if (setval) set_realtimer(mp, &value);

  		r = OK;
  		break;

  	case ITIMER_VIRTUAL :
  	case ITIMER_PROF :
		getset_vtimer(mp, which, (setval) ? &value : NULL,
			(getval) ? &ovalue : NULL);

  		r = OK;
  		break;

	default:
		panic("invalid timer type: %d", which);
  }

  /* If requested, copy the old interval timer to user space. */
  if (r == OK && getval) {
	r = sys_datacopy(PM_PROC_NR, (vir_bytes)&ovalue,
		who_e, m_in.m_lc_pm_itimer.ovalue,
		(phys_bytes)sizeof(ovalue));
  }

  return(r);
}

/*===========================================================================*
 *				getset_vtimer				     * 
 *===========================================================================*/
static void getset_vtimer(rmp, which, value, ovalue)
struct mproc *rmp;
int which;
struct itimerval *value;
struct itimerval *ovalue;
{
  clock_t newticks, *nptr;		/* the new timer value, in ticks */
  clock_t oldticks, *optr;		/* the old ticks value, in ticks */
  int r, num;

  /* The default is to provide sys_vtimer with two null pointers, i.e. to do
   * nothing at all.
   */
  optr = nptr = NULL;

  /* If the old timer value is to be retrieved, have 'optr' point to the
   * location where the old value is to be stored, and copy the interval.
   */
  if (ovalue != NULL) {
  	optr = &oldticks;

  	timeval_from_ticks(&ovalue->it_interval, rmp->mp_interval[which]);
  }

  /* If a new timer value is to be set, store the new timer value and have
   * 'nptr' point to it. Also, store the new interval.
   */
  if (value != NULL) {
  	newticks = ticks_from_timeval(&value->it_value);
  	nptr = &newticks;

  	/* If no timer is set, the interval must be zero. */
  	if (newticks <= 0)
  		rmp->mp_interval[which] = 0;
	else 
		rmp->mp_interval[which] =
			ticks_from_timeval(&value->it_interval);
  }

  /* Find out which kernel timer number to use. */
  switch (which) {
  case ITIMER_VIRTUAL: num = VT_VIRTUAL; break;
  case ITIMER_PROF:    num = VT_PROF;    break;
  default:             panic("invalid vtimer type: %d", which);
  }

  /* Make the kernel call. If requested, also retrieve and store
   * the old timer value.
   */
  if ((r = sys_vtimer(rmp->mp_endpoint, num, nptr, optr)) != OK)
  	panic("sys_vtimer failed: %d", r);

  if (ovalue != NULL) {
  	/* If the alarm expired already, we should take into account the
  	 * interval. Return zero only if the interval is zero as well.
  	 */
  	if (oldticks <= 0) oldticks = rmp->mp_interval[which];

	timeval_from_ticks(&ovalue->it_value, oldticks);
  }
}

/*===========================================================================*
 *				check_vtimer				     * 
 *===========================================================================*/
void check_vtimer(proc_nr, sig)
int proc_nr;
int sig;
{
  register struct mproc *rmp;
  int which, num;

  rmp = &mproc[proc_nr];

  /* Translate back the given signal to a timer type and kernel number. */
  switch (sig) {
  case SIGVTALRM: which = ITIMER_VIRTUAL; num = VT_VIRTUAL; break;
  case SIGPROF:   which = ITIMER_PROF;    num = VT_PROF;    break;
  default: panic("invalid vtimer signal: %d", sig);
  }

  /* If a repetition interval was set for this virtual timer, tell the
   * kernel to set a new timeout for the virtual timer.
   */
  if (rmp->mp_interval[which] > 0)
  	sys_vtimer(rmp->mp_endpoint, num, &rmp->mp_interval[which], NULL);
}

/*===========================================================================*
 *				get_realtimer				     * 
 *===========================================================================*/
static void get_realtimer(rmp, value)
struct mproc *rmp;
struct itimerval *value;
{
  clock_t exptime;	/* time at which alarm will expire */
  clock_t uptime;	/* current system time */
  clock_t remaining;	/* time left on alarm */
  int s;

  /* First determine remaining time, in ticks, of previous alarm, if set. */
  if (rmp->mp_flags & ALARM_ON) {
  	if ( (s = getticks(&uptime)) != OK)
  		panic("get_realtimer couldn't get uptime: %d", s);
  	exptime = *tmr_exp_time(&rmp->mp_timer);

  	remaining = exptime - uptime;

  	/* If the alarm expired already, we should take into account the
  	 * interval. Return zero only if the interval is zero as well.
  	 */
  	if (remaining <= 0) remaining = rmp->mp_interval[ITIMER_REAL];
  } else {
  	remaining = 0;
  }

  /* Convert the result to a timeval structure. */
  timeval_from_ticks(&value->it_value, remaining);

  /* Similarly convert and store the interval of the timer. */
  timeval_from_ticks(&value->it_interval, rmp->mp_interval[ITIMER_REAL]);
}

/*===========================================================================*
 *				set_realtimer				     * 
 *===========================================================================*/
static void set_realtimer(rmp, value)
struct mproc *rmp;
struct itimerval *value;
{
  clock_t ticks;	/* New amount of ticks to the next alarm. */
  clock_t interval;	/* New amount of ticks for the alarm's interval. */

  /* Convert the timeval structures in the 'value' structure to ticks. */
  ticks = ticks_from_timeval(&value->it_value);
  interval = ticks_from_timeval(&value->it_interval);

  /* If no timer is set, the interval must be zero. */
  if (ticks <= 0) interval = 0;

  /* Apply these values. */
  set_alarm(rmp, ticks);
  rmp->mp_interval[ITIMER_REAL] = interval;
}

/*===========================================================================*
 *				set_alarm				     * 
 *===========================================================================*/
void set_alarm(rmp, ticks)
struct mproc *rmp;		/* process that wants the alarm */
clock_t ticks;			/* how many ticks delay before the signal */
{
  if (ticks > 0) {
  	set_timer(&rmp->mp_timer, ticks, cause_sigalrm, rmp->mp_endpoint);
	rmp->mp_flags |=  ALARM_ON;
  } else if (rmp->mp_flags & ALARM_ON) {
  	cancel_timer(&rmp->mp_timer);
  	rmp->mp_flags &= ~ALARM_ON;
  }
}

/*===========================================================================*
 *				cause_sigalrm				     * 
 *===========================================================================*/
static void cause_sigalrm(tp)
minix_timer_t *tp;
{
  int proc_nr_n;
  register struct mproc *rmp;

  /* get process from timer */
  if(pm_isokendpt(tmr_arg(tp)->ta_int, &proc_nr_n) != OK) {
  	printf("PM: ignoring timer for invalid endpoint %d\n",
  		tmr_arg(tp)->ta_int);
  	return;
  }

  rmp = &mproc[proc_nr_n];

  if ((rmp->mp_flags & (IN_USE | EXITING)) != IN_USE) return;
  if ((rmp->mp_flags & ALARM_ON) == 0) return;

  /* If an interval is set, set a new timer; otherwise clear the ALARM_ON flag.
   * The set_alarm call will be calling set_timer from within this callback
   * from the expire_timers function. This is safe, but we must not use the
   * "tp" structure below this point anymore.
   */
  if (rmp->mp_interval[ITIMER_REAL] > 0)
	set_alarm(rmp, rmp->mp_interval[ITIMER_REAL]);
  else rmp->mp_flags &= ~ALARM_ON;

  check_sig(rmp->mp_pid, SIGALRM, FALSE /* ksig */);
}
