/* t40f.c
 *
 * Test timing
 *
 * Select works on regular files, (pseudo) terminal devices, streams-based
 * files, FIFOs, pipes, and sockets. This test verifies selecting with a time
 * out set. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "common.h"

#define DO_HANDLEDATA 1
#define DO_PAUSE 3
#define DO_TIMEOUT 7
#define DO_DELTA 0.5
#define MAX_ERROR 5
#define DELTA(x,y)  (x.tv_sec - y.tv_sec) * CLOCKS_PER_SEC \
  + (x.tv_usec - y.tv_usec) * CLOCKS_PER_SEC / 1000000

int got_signal = 0;
int fd_ap[2];

static void catch_signal(int sig_no) {
  got_signal = 1;
}

static float compute_diff(struct timeval start, struct timeval end, float compare) {
  /* Compute time difference. It is assumed that the value of start <= end. */
  clock_t delta;
  int seconds, hundreths;
  float diff;

  delta = DELTA(end, start); /* delta is in ticks */
  seconds = (int) (delta / CLOCKS_PER_SEC);
  hundreths = (int) (delta * 100 / CLOCKS_PER_SEC) - (seconds * 100);

  diff = seconds + (hundreths / 100.0);
  diff -= compare;
  if(diff < 0) diff *= -1; /* Make diff a positive value */

  return diff;
}

static void do_child(void) {
  struct timeval tv;
 
  /* Let the parent do initial read and write tests from and to the pipe. */
  tv.tv_sec = DO_PAUSE + DO_PAUSE + DO_PAUSE + 1;
  tv.tv_usec = 0;
  (void) select(0, NULL, NULL, NULL, &tv);

  /* At this point the parent has a pending select with a DO_TIMEOUT timeout.
     We're going to interrupt by sending a signal */
  if(kill(getppid(), SIGUSR1) < 0) perror("Failed to send signal");
  
  exit(0);
}

static void do_parent(int child) {
  fd_set fds_read;
  struct timeval tv, start_time, end_time;
  int retval;

  /* Install signal handler for SIGUSR1 */
  signal(SIGUSR1, catch_signal);

  /* Parent and child share an anonymous pipe. Select for read and wait for the
   timeout to occur. We wait for DO_PAUSE seconds. Let's see if that's
   approximately right.*/
  FD_ZERO(&fds_read);
  FD_SET(fd_ap[0], &fds_read);
  tv.tv_sec = DO_PAUSE;
  tv.tv_usec = 0;

  (void) gettimeofday(&start_time, NULL);   /* Record starting time */
  retval = select(fd_ap[0]+1, &fds_read, NULL, NULL, &tv); 
  (void) gettimeofday(&end_time, NULL);     /* Record ending time */
  
  /* Did we time out? */
  if(retval != 0) em(1, "Should have timed out");
  
  /* Approximately right? The standard does not specify how precise the timeout
     should be. Instead, the granularity is implementation-defined. In this
     test we assume that the difference should be no more than half a second.*/
  if(compute_diff(start_time, end_time, DO_PAUSE) > DO_DELTA)
    em(2, "Time difference too large");
  
  /* Let's wait for another DO_PAUSE seconds, expressed as microseconds */
  FD_ZERO(&fds_read);
  FD_SET(fd_ap[0], &fds_read);
  tv.tv_sec = 0;
  tv.tv_usec = DO_PAUSE * 1000000L;
  
  (void) gettimeofday(&start_time, NULL);   /* Record starting time */
  retval = select(fd_ap[0]+1, &fds_read, NULL, NULL, &tv); 
  (void) gettimeofday(&end_time, NULL);     /* Record ending time */
  
  if(retval != 0) em(3, "Should have timed out");
  if(compute_diff(start_time, end_time, DO_PAUSE) > DO_DELTA)
    em(4, "Time difference too large");
  
  /* Let's wait for another DO_PAUSE seconds, expressed in seconds and micro
     seconds. */
  FD_ZERO(&fds_read);
  FD_SET(fd_ap[0], &fds_read);
  tv.tv_sec = DO_PAUSE - 1;
  tv.tv_usec = (DO_PAUSE - tv.tv_sec) * 1000000L;
  
  (void) gettimeofday(&start_time, NULL);   /* Record starting time */
  retval = select(fd_ap[0]+1, &fds_read, NULL, NULL, &tv); 
  (void) gettimeofday(&end_time, NULL);     /* Record ending time */
  
  if(retval != 0) em(5, "Should have timed out");
  if(compute_diff(start_time, end_time, DO_PAUSE) > DO_DELTA)
    em(6, "Time difference too large");

  /* Finally, we test if our timeout is interrupted by a signal */
  FD_ZERO(&fds_read);
  FD_SET(fd_ap[0], &fds_read);
  tv.tv_sec = DO_TIMEOUT;
  tv.tv_usec = 0;

  (void) gettimeofday(&start_time, NULL);   /* Record starting time */
  retval = select(fd_ap[0]+1, &fds_read, NULL, NULL, &tv); 
  (void) gettimeofday(&end_time, NULL);     /* Record ending time */
  
  if(retval != -1) em(7, "Should have been interrupted");
  if(compute_diff(start_time, end_time, DO_TIMEOUT) < DO_DELTA)
    em(8, "Failed to get interrupted by a signal");

  if(!got_signal) em(9, "Failed to get interrupted by a signal");

  waitpid(child, &retval, 0);
  exit(errct);
}
  
int main(int argc, char **argv) {
  int forkres;

  /* Get subtest number */
  if(argc != 2) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-2);
  } else if(sscanf(argv[1], "%d", &subtest) != 1) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-2);
  }
  
  /* Set up anonymous pipe */
  if(pipe(fd_ap) < 0) {
    perror("Could not create anonymous pipe");
    exit(-1);
  }

  forkres = fork();
  if(forkres == 0) do_child();
  else if(forkres > 0)  do_parent(forkres);
  else { /* Fork failed */
    perror("Unable to fork");
    exit(-1);
  }

  exit(-2); /* We're not supposed to get here. Both do_* routines should exit*/
  
}
