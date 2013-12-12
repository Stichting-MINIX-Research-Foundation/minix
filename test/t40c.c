/* t40c.c
 *
 * Test (pseudo) terminal devices
 *
 * Select works on regular files, (pseudo) terminal devices, streams-based
 * files, FIFOs, pipes, and sockets. This test verifies selecting for (pseudo)
 * terminal devices.
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
#include <sys/syslimits.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>

#include "common.h"

#define TERMINALW "/dev/ttypf"
#define TERMINALR "/dev/ptypf"
#define SENDSTRING "minixrocks"
#define MAX_ERROR 5

static void open_terminal(int *child_fd, int *parent_fd) {
  int fd1, fd2, i;
  char opentermw[5+OPEN_MAX+1];
  char opentermr[5+OPEN_MAX+1];
  char *term[] = {"f","e","d","c","b","a","9","8","7","6","5","4","3","2","1"};
#define TERMS (sizeof(term)/sizeof(term[0]))

  if (!child_fd || !parent_fd) exit(EXIT_FAILURE);

  for (i = 0; i < TERMS; i++) {
	snprintf(opentermw, 5+OPEN_MAX, "/dev/ttyp%s", term[i]);
	snprintf(opentermr, 5+OPEN_MAX, "/dev/ptyp%s", term[i]);

	/* Open master terminal for writing */
	if((fd1 = open(opentermw, O_WRONLY)) == -1) continue;

	/* Open slave terminal for reading */
	if((fd2 = open(opentermr, O_RDONLY)) == -1) {
		close(fd1);
		continue;
	}

	*child_fd = fd1;
	*parent_fd = fd2;
	return;
  }

  /* If we get here we failed to find a terminal pair */
  exit(EXIT_FAILURE);
}

static int do_child(int terminal) {
  /* Going to sleep for two seconds to allow the parent proc to get ready */
  sleep(2);

  /* Try to write. Doesn't matter how many bytes we actually send. */
  (void) write(terminal, SENDSTRING, strlen(SENDSTRING));

  /* Wait for another second to allow the parent to process incoming data */
  sleep(1);

  /* Write some more, and wait some more. */
  (void) write(terminal, SENDSTRING, strlen(SENDSTRING));

  sleep(1);

  close(terminal);
  exit(0);
}

static int do_parent(int child, int terminal) {
  fd_set fds_read, fds_read2, fds_write, fds_error;
  int retval, terminal2, highest;
  char buf[256];

  /* Clear bit masks */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
  /* Set read bits */
  FD_SET(terminal, &fds_read);
  FD_SET(terminal, &fds_write);
  
  /* Test if we can read or write from/to fd. As fd is opened read only we
   * cannot actually write, so the select should return immediately with fd
   * set in fds_write, but not in fds_read. Note that the child waits two
   * seconds before sending data. This gives us the opportunity run this
   * sub-test as reading from fd is blocking at this point. */
  retval = select(terminal+1, &fds_read, &fds_write, &fds_error, NULL);
  
  if(retval != 1) em(1, "incorrect amount of ready file descriptors");

  if(FD_ISSET(terminal, &fds_read)) em(2, "read should NOT be set");
  if(!FD_ISSET(terminal, &fds_write)) em(3, "write should be set");
  if(FD_ISSET(terminal, &fds_error)) em(4, "error should NOT be set");

  /* Block until ready; until child wrote stuff */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
  FD_SET(terminal, &fds_read);
  retval = select(terminal+1, &fds_read, NULL, &fds_error, NULL);
 
  if(retval != 1) em(5, "incorrect amount of ready file descriptors");
  if(!FD_ISSET(terminal, &fds_read)) em(6, "read should be set");
  if(FD_ISSET(terminal, &fds_error)) em(7, "error should not be set");

  FD_ZERO(&fds_read); FD_ZERO(&fds_error);
  FD_SET(terminal, &fds_write);
  retval = select(terminal+1, NULL, &fds_write, NULL, NULL);
  /* As it is impossible to write to a read only fd, this select should return
   * immediately with fd set in fds_write. */
  if(retval != 1) em(8, "incorrect amount or ready file descriptors");

  /* See if selecting on the same object with two different fds results in both
   * fds being returned as ready, immediately.
   */
  terminal2 = dup(terminal);
  if (terminal2 < 0) em(9, "unable to dup file descriptor");

  FD_ZERO(&fds_read);
  FD_SET(terminal, &fds_read);
  FD_SET(terminal2, &fds_read);
  fds_read2 = fds_read;
  highest = terminal > terminal2 ? terminal : terminal2;

  retval = select(highest+1, &fds_read, NULL, NULL, NULL);
  if (retval != 2) em(10, "incorrect amount of ready file descriptors");
  if (!FD_ISSET(terminal, &fds_read)) em(11, "first fd missing from set");
  if (!FD_ISSET(terminal2, &fds_read)) em(12, "second fd missing from set");

  /* Empty the buffer. */
  if (read(terminal, buf, sizeof(buf)) <= 0) em(13, "unable to read data");

  /* Repeat the test, now with a delay. */
  retval = select(highest+1, &fds_read2, NULL, NULL, NULL);
  if (retval != 2) em(10, "incorrect amount of ready file descriptors");
  if (!FD_ISSET(terminal, &fds_read2)) em(11, "first fd missing from set");
  if (!FD_ISSET(terminal2, &fds_read2)) em(12, "second fd missing from set");

  close(terminal2);
  close(terminal);
  waitpid(child, &retval, 0);
  exit(errct);
}

int main(int argc, char **argv) {
  int forkres;
  int master, slave;

  /* Get subtest number */
  if(argc != 2) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-1);
  } else if(sscanf(argv[1], "%d", &subtest) != 1) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-1);
  }

  open_terminal(&master, &slave);

  forkres = fork();
  if(forkres == 0) do_child(master);
  else if(forkres > 0)  do_parent(forkres, slave);
  else { /* Fork failed */
    perror("Unable to fork");
    exit(-1);
  }

  exit(-2); /* We're not supposed to get here. Both do_* routines should exit*/
  
}
