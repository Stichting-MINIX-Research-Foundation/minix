/*	sleep() - Sleep for a number of seconds.	Author: Kees J. Bot
 *								24 Apr 2000
 * (Inspired by the Minix-vmd version of same, except that
 * this implementation doesn't bother to check if all the signal
 * functions succeed.  Under Minix that is no problem.)
 */

#include <lib.h>
#define sleep _sleep
#include <signal.h>
#include <unistd.h>
#include <time.h>

static void handler(int sig)
{
	/* Dummy signal handler. */
}

unsigned sleep(unsigned sleep_seconds)
{
	sigset_t ss_full, ss_orig, ss_alarm;
	struct sigaction action_alarm, action_orig;
	unsigned alarm_seconds, nap_seconds;

	if (sleep_seconds == 0) return 0;	/* No rest for the wicked */

	/* Mask all signals. */
	sigfillset(&ss_full);
	sigprocmask(SIG_BLOCK, &ss_full, &ss_orig);

	/* Cancel currently running alarm. */
	alarm_seconds= alarm(0);

	/* How long can we nap without interruptions? */
	nap_seconds= sleep_seconds;
	if (alarm_seconds != 0 && alarm_seconds < sleep_seconds) {
		nap_seconds= alarm_seconds;
	}

	/* Now sleep. */
	action_alarm.sa_handler= handler;
	sigemptyset(&action_alarm.sa_mask);
	action_alarm.sa_flags= 0;
	sigaction(SIGALRM, &action_alarm, &action_orig);
	alarm(nap_seconds);

	/* Wait for a wakeup call, either our alarm, or some other signal. */
	ss_alarm= ss_orig;
	sigdelset(&ss_alarm, SIGALRM);
	sigsuspend(&ss_alarm);

	/* Cancel alarm, set mask and stuff back to normal. */
	nap_seconds -= alarm(0);
	sigaction(SIGALRM, &action_orig, NULL);
	sigprocmask(SIG_SETMASK, &ss_orig, NULL);

	/* Restore alarm counter to the time remaining. */
	if (alarm_seconds != 0 && alarm_seconds >= nap_seconds) {
		alarm_seconds -= nap_seconds;
		if (alarm_seconds == 0) {
			raise(SIGALRM);		/* Alarm expires now! */
		} else {
			alarm(alarm_seconds);	/* Count time remaining. */
		}
	}

	/* Return time not slept. */
	return sleep_seconds - nap_seconds;
}
