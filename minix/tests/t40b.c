/* t40b.c
 *
 * Test regular files
 *
 * Select works on regular files, (pseudo) terminal devices, streams-based
 * files, FIFOs, pipes, and sockets. This test verifies selecting for regular
 * file descriptors. "File descriptors associated with regular files shall
 * always select true for ready to read, ready to write, and error conditions"
 * - Open Group. Although we set a timeout, the select should return
 * immediately.
 *
 * This test is part of a bigger select test. It expects as argument which sub-
 * test it is.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>

#include "common.h"

#define FILE1 "selecttestb-1"
#define FILES 2
#define TIME 3

#define MAX_ERROR 10

char errorbuf[1000];

int main(int argc, char **argv) {
  int fd1, fd2, retval;
  fd_set fds_read, fds_write, fds_error;
  struct timeval tv;
  time_t start, end;

  /* Get subtest number */
  if(argc != 2) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-1);
  } else if(sscanf(argv[1], "%d", &subtest) != 1) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-1);
  }

  /* Set timeout */
  tv.tv_sec = TIME;
  tv.tv_usec = 0;
  
  /* Open a file for writing */
  if((fd1 = open(FILE1, O_WRONLY|O_CREAT, 0644)) == -1) {
    snprintf(errorbuf, sizeof(errorbuf), "failed to open file %s for writing",
	     FILE1);
    em(1, errorbuf);
    perror(NULL);
    exit(1);
  }
  
  /* Open the same file for reading */ 
  if((fd2 = open(FILE1, O_RDONLY)) == -1) {
    snprintf(errorbuf, sizeof(errorbuf), "failed to open file %s for reading",
	     FILE1);
    em(2, errorbuf);
    perror(NULL);
    exit(1);
  }
  
  /* Clear file descriptor bit masks */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
  
  /* Fill bit mask */
  FD_SET(fd1, &fds_write);
  FD_SET(fd2, &fds_read);
  FD_SET(fd1, &fds_error);
  FD_SET(fd2, &fds_error);

  /* Do the select and time how long it takes */
  start = time(NULL);
  retval =  select(fd2+1, &fds_read, &fds_write, &fds_error, &tv);
  end = time(NULL);

  /* Correct amount of ready file descriptors? 1 read + 1 write + 2 errors */
  if(retval != 4) {
    em(3, "four fds should be set");
  }

  /* Test resulting bit masks */
  if(!FD_ISSET(fd1, &fds_write)) em(4, "write should be set");
  if(!FD_ISSET(fd2, &fds_read)) em(5, "read should be set");
  if(!FD_ISSET(fd1, &fds_error)) em(6, "error should be set");
  if(!FD_ISSET(fd2, &fds_error)) em(7, "error should be set");

  /* Was it instantaneous? */
  if(end-start != TIME - TIME) {
    snprintf(errorbuf,sizeof(errorbuf),"time spent blocking is not %d, but %ld",
	     TIME - TIME, (long int) (end-start));
    em(8, errorbuf);
  }

  /* Wait for read to become ready on O_WRONLY. This should fail immediately. */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
  FD_SET(fd1, &fds_read);
  FD_SET(fd1, &fds_error);
  FD_SET(fd2, &fds_error);
  tv.tv_sec = TIME;
  tv.tv_usec = 0;
  retval = select(fd2+1, &fds_read, NULL, &fds_error, &tv);

  /* Correct amount of ready file descriptors? 1 read + 2 error */
  if(retval != 3) em(9, "incorrect amount of ready file descriptors");
  if(!FD_ISSET(fd1, &fds_read)) em(10, "read should be set");
  if(!FD_ISSET(fd1, &fds_error)) em(11, "error should be set");
  if(!FD_ISSET(fd2, &fds_error)) em(12, "error should be set");

  /* Try again as above, bit this time with O_RDONLY in the write set */
  FD_ZERO(&fds_error);
  FD_SET(fd2, &fds_write);
  FD_SET(fd1, &fds_error);
  FD_SET(fd2, &fds_error);
  tv.tv_sec = TIME;
  tv.tv_usec = 0;
  retval = select(fd2+1, NULL, &fds_write, &fds_error, &tv);
  
  /* Correct amount of ready file descriptors? 1 write + 2 errors */
  if(retval != 3) em(13, "incorrect amount of ready file descriptors");
  if(!FD_ISSET(fd2, &fds_write)) em(14, "write should be set");
  if(!FD_ISSET(fd1, &fds_error)) em(15, "error should be set");
  if(!FD_ISSET(fd2, &fds_error)) em(16, "error should be set");
  
  close(fd1);
  close(fd2);
  unlink(FILE1);
  
  exit(errct);
}
