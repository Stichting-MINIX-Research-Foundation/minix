/* Test 69. clock_getres(), clock_gettime(), clock_settime(), adjtime().
 *
 * Note, any type of ntpd or software that calls adjtime() or settimeofday()
 * should be disabled while running this test. This test takes ~40s to run.
 */

#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TRIALS 100
int max_error = 4;
#include "common.h"

#ifndef DEBUG
#define DEBUG 0
#endif

int subtest = 1;


int main(void);
void quit(void);
static void test_clock_getres(void);
static void test_clock_gettime(void);
static void test_clock_settime(void);
static void test_adjtime(void);
static void show_timespec(char *msg, struct timespec *ts);

static void test_clock_getres(void)
{
  struct timespec res;

  /* valid clock_id should succeed, invalid clock_id should fail */
  if (clock_getres(CLOCK_REALTIME, &res) == -1) e(10);
  if (res.tv_sec < 0 || res.tv_nsec < 0) e(11);
  show_timespec("res(CLOCK_REALTIME)", &res);

  if (clock_getres(CLOCK_MONOTONIC, &res) == -1) e(12);
  if (res.tv_sec < 0 || res.tv_nsec < 0) e(13);
  show_timespec("res(CLOCK_MONOTONIC)", &res);

  if (clock_getres(-1, &res) == 0) e(14);
}

static void test_clock_gettime(void)
{
  struct timespec ts, ts2;

  /* valid clock_id should succeed, invalid clock_id should fail */
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) e(21);
  if (ts.tv_sec < 0 || ts.tv_nsec < 0) e(22);
  show_timespec("time(CLOCK_REALTIME)", &ts);
  sleep(2);
  if (clock_gettime(CLOCK_REALTIME, &ts2) == -1) e(23);
  if (ts2.tv_sec < 0 || ts2.tv_nsec < 0) e(24);
  if (ts2.tv_sec <= ts.tv_sec) e(25);

  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) e(26);
  if (ts.tv_sec < 0 || ts.tv_nsec < 0) e(27);
  show_timespec("time(CLOCK_MONOTONIC)", &ts);
  sleep(2);
  if (clock_gettime(CLOCK_MONOTONIC, &ts2) == -1) e(28);
  if (ts2.tv_sec < 0 || ts2.tv_nsec < 0) e(29);
  if (ts2.tv_sec <= ts.tv_sec) e(30);

  if (clock_gettime(-1, &ts) == 0) e(31);
}

static void test_clock_settime(void)
{
  struct timespec ts;
  struct timespec ts2;

  /* shouldn't be able to set MONOTONIC */
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) e(50);
  if (clock_settime(CLOCK_MONOTONIC, &ts) == 0) e(51);
  if (errno != EINVAL) e(52); /* reponse should be EINVAL */

  /* set the time of REALTIME to that of MONOTONIC */
  if (clock_settime(CLOCK_REALTIME, &ts) == -1) e(53);

  ts.tv_sec += 600; /* time travel 10 minutes into the future */
  if (clock_gettime(CLOCK_REALTIME, &ts2) == -1) e(54);
  if (clock_settime(CLOCK_REALTIME, &ts) == -1) e(55);
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) e(56);

  /* get the value we set, if it's not about 10 minutes ahead, it failed */
  if (ts.tv_sec - ts2.tv_sec < 500 ||
	ts.tv_sec - ts2.tv_sec > 700) e(57);

  /* back to current time - don't leave the system time 10 ahead */
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) e(58);
  ts.tv_sec -= 600;
  if (clock_settime(CLOCK_REALTIME, &ts) == -1) e(59);

  /* Test with an invalid clock */
  if (clock_settime(-1, &ts) == 0) e(60);
}

static void test_adjtime(void)
{
  struct timeval delta, olddelta;
  struct timespec rt, mt;

  /* set the realtime clock to the same value as the monotonic clock */
  if (clock_gettime(CLOCK_MONOTONIC, &mt) == -1) e(65);
  if (clock_settime(CLOCK_REALTIME, &mt) == -1) e(66);

  delta.tv_sec = 7;
  delta.tv_usec = 0;

  if (adjtime(&delta, &olddelta) != 0) e(70);	/* adjust +7 seconds */
  sleep(15);	/* should take 14 seconds to adjust the clock */

  /* check that the 7 second adjustment puts us between 5 and 10 seconds
   * ahead of the monotonic clock.
   */
  if (clock_gettime(CLOCK_MONOTONIC, &mt) == -1) e(71);
  if (clock_gettime(CLOCK_REALTIME, &rt) == -1) e(72);
  show_timespec("Monotonic", &mt);
  show_timespec("Realtime (+7)", &rt);
  if (rt.tv_sec - 5 < mt.tv_sec || rt.tv_sec - 10 > mt.tv_sec) e(73);

  delta.tv_sec = -7;
  if (adjtime(&delta, &olddelta) != 0) e(73);	/* adjust -7 seconds */
  sleep(15);	/* should take 14 seconds to adjust the clock */

  /* check that the 7 second adjustment puts us close to even with
   * the monotonic clock.
   */
  if (clock_gettime(CLOCK_MONOTONIC, &mt) == -1) e(74);
  if (clock_gettime(CLOCK_REALTIME, &rt) == -1) e(75);
  show_timespec("Monotonic", &mt);
  show_timespec("Realtime (-7)", &rt);
  if (abs(rt.tv_sec - mt.tv_sec) > 5) e(76);

}

static void show_timespec(char *msg, struct timespec *ts)
{
#if DEBUG == 1
  printf("[%s] tv_sec=%d tv_nsec=%ld\n", msg, ts->tv_sec, ts->tv_nsec);
#endif /* DEBUG == 1 */
}

int main(void)
{
  start(69);
  struct timespec starttime, endtime;

  /* get test start time */
  if (clock_gettime(CLOCK_MONOTONIC, &starttime) == -1) e(1);

  test_clock_getres();
  test_clock_gettime();
  test_clock_settime();
  test_adjtime();

  /* get test end time */
  if (clock_gettime(CLOCK_MONOTONIC, &endtime) == -1) e(2);

  /* we shouldn't have gone backwards in time during this test */
  if ((starttime.tv_sec > endtime.tv_sec) ||
	(starttime.tv_sec == endtime.tv_sec &&
		 starttime.tv_nsec > endtime.tv_nsec)) e(3);

  quit();
  return(-1);			/* impossible */
}

