/* Test 69. clock_getres(), clock_gettime(). */

#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TRIALS 100
#define MAX_ERROR 4
#define DEBUG 0

int subtest = 1;

#include "common.c"

int main(void);
void quit(void);
static void test_clock_getres();
static void test_clock_gettime();
static void show_timespec(char *msg, struct timespec *ts);

static void test_clock_getres()
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

static void test_clock_gettime()
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

static void show_timespec(char *msg, struct timespec *ts)
{
#if DEBUG == 1
  printf("[%s] tv_sec=%d tv_nsec=%ld\n", msg, ts->tv_sec, ts->tv_nsec);
#endif /* DEBUG == 1 */
}

int main()
{
  start(69);
  struct timespec starttime, endtime;

  /* get test start time */
  if (clock_gettime(CLOCK_MONOTONIC, &starttime) == -1) e(1);

  test_clock_getres();
  test_clock_gettime();

  /* get test end time */
  if (clock_gettime(CLOCK_MONOTONIC, &endtime) == -1) e(2);

  /* we shouldn't have gone backwards in time during this test */
  if ((starttime.tv_sec > endtime.tv_sec) ||
	(starttime.tv_sec == endtime.tv_sec &&
		 starttime.tv_nsec > endtime.tv_nsec)) e(3);

  quit();
  return(-1);			/* impossible */
}

