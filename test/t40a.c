/* t40a.c
 *
 * Test FD_* macros
 *
 * Select works on regular files, (pseudo) terminal devices, streams-based
 * files, FIFOs, pipes, and sockets. This test verifies the FD_* macros.
 *
 * This test is part of a bigger select test. It expects as argument which sub-
 * test it is.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <limits.h>

#ifndef OPEN_MAX
# define OPEN_MAX 1024
#endif

#define MAX_ERROR 5

int errct = 0, subtest = -1;

void e(int n, char *s) {
  printf("Subtest %d, error %d, %s\n", subtest, n, s);

  if (errct++ > MAX_ERROR) {
    printf("Too many errors; test aborted\n");
    exit(errct);
  }
}

int main(int argc, char **argv) {
  fd_set fds;
  int i;
  
  /* Get subtest number */
  if(argc != 2) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-1);
  } else if(sscanf(argv[1], "%d", &subtest) != 1) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-1);
  }

  /* FD_ZERO */
  FD_ZERO(&fds);
  for(i = 0; i < OPEN_MAX; i++) {
    if(FD_ISSET(i, &fds)) {
      e(1, "fd set should be completely empty");
      break;
    }
  }

  /* FD_SET */
  for(i = 0; i < OPEN_MAX; i++) FD_SET(i, &fds);
  for(i = 0; i < OPEN_MAX; i++) {
    if(!FD_ISSET(i, &fds)) {
      e(2, "fd set should be completely filled");
      break;
    }  
  }  
  
  /* Reset to empty set and verify it's really empty */
  FD_ZERO(&fds);
  for(i = 0; i < OPEN_MAX; i++) {
    if(FD_ISSET(i, &fds)) {
      e(3, "fd set should be completely empty");
      break;
    }
  }

  /* Let's try a variation on filling the set */
  for(i = 0; i < OPEN_MAX; i += 2) FD_SET(i, &fds);
  for(i = 0; i < OPEN_MAX - 1; i+= 2 ) {
    if(!(FD_ISSET(i, &fds) && !FD_ISSET(i+1, &fds))) {
      e(4, "bit pattern does not match");
      break;
    }
  }

  /* Reset to empty set and verify it's really empty */
  FD_ZERO(&fds);
  for(i = 0; i < OPEN_MAX; i++) {
    if(FD_ISSET(i, &fds)) {
      e(5,"fd set should be completely empty");
      break;
    }
  }

  /* Let's try another variation on filling the set */
  for(i = 0; i < OPEN_MAX - 1; i += 2) FD_SET(i+1, &fds);
  for(i = 0; i < OPEN_MAX - 1; i+= 2 ) {
    if(!(FD_ISSET(i+1, &fds) && !FD_ISSET(i, &fds))) {
      e(6, "bit pattern does not match");
      break;
    }
  }

  /* Reset to empty set and verify it's really empty */
  FD_ZERO(&fds);
  for(i = 0; i < OPEN_MAX; i++) {
    if(FD_ISSET(i, &fds)) {
      e(7, "fd set should be completely empty");
      break;
    }
  }

  /* FD_CLR */
  for(i = 0; i < OPEN_MAX; i++) FD_SET(i, &fds); /* Set all bits */
  for(i = 0; i < OPEN_MAX; i++) FD_CLR(i, &fds); /* Clear all bits */
  for(i = 0; i < OPEN_MAX; i++) {
    if(FD_ISSET(i, &fds)) {
      e(8, "all bits in fd set should be cleared");
      break;
    }
  }

  /* Reset to empty set and verify it's really empty */
  FD_ZERO(&fds);
  for(i = 0; i < OPEN_MAX; i++) {
    if(FD_ISSET(i, &fds)) {
      e(9, "fd set should be completely empty");
      break;
    }
  }

  for(i = 0; i < OPEN_MAX; i++) FD_SET(i, &fds); /* Set all bits */
  for(i = 0; i < OPEN_MAX; i += 2) FD_CLR(i, &fds); /* Clear all bits */
  for(i = 0; i < OPEN_MAX; i += 2) {
    if(FD_ISSET(i, &fds)) {
      e(10, "all even bits in fd set should be cleared");
      break;
    }
  }
  
  exit(errct);
}
