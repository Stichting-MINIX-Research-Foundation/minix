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
#include <errno.h>
#include <sys/wait.h>
#include <string.h>

#define TERMINALW "/dev/ttypf"
#define TERMINALR "/dev/ptypf"
#define SENDSTRING "minixrocks"
#define MAX_ERROR 5

int errct = 0, subtest = -1;

void e(int n, char *s) {
  printf("Subtest %d, error %d, %s\n", subtest, n, s);

  if (errct++ > MAX_ERROR) {
    printf("Too many errors; test aborted\n");
    exit(errct);
  }
}

int do_child(void) {
  int fd, retval;
  struct timeval tv;

  /* Opening master terminal for writing */
  if((fd = open(TERMINALW, O_WRONLY)) == -1) {
    printf("Error opening %s for writing, signalling parent to quit\n",
	   TERMINALW);
    perror(NULL);
    printf("Please make sure that %s is not in use while running this test\n",
	   TERMINALW);
    exit(-1);
  }

  /* Going to sleep for two seconds to allow the parent proc to get ready */
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  select(0, NULL, NULL, NULL, &tv);

  /* Try to write. Doesn't matter how many bytes we actually send. */
  retval = write(fd, SENDSTRING, strlen(SENDSTRING));
  close(fd);

  /* Wait for another second to allow the parent to process incoming data */
  tv.tv_usec = 1000000;
  retval = select(0,NULL, NULL, NULL, &tv);
  exit(0);
}

int do_parent(int child) {
  int fd;
  fd_set fds_read, fds_write, fds_error;
  int retval;

  /* Open slave terminal for reading */
  if((fd = open(TERMINALR, O_RDONLY)) == -1) {
    printf("Error opening %s for reading\n", TERMINALR);
    perror(NULL);
    printf("Please make sure that %s is not in use while running this test.\n",
	   TERMINALR);
    waitpid(child, &retval, 0);
    exit(-1);
  }

  /* Clear bit masks */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
  /* Set read bits */
  FD_SET(fd, &fds_read);
  FD_SET(fd, &fds_write);
  
  /* Test if we can read or write from/to fd. As fd is opened read only we
   * cannot actually write, so the select should return immediately with fd
   * set in fds_write, but not in fds_read. Note that the child waits two
   * seconds before sending data. This gives us the opportunity run this
   * sub-test as reading from fd is blocking at this point. */
  retval = select(fd+1, &fds_read, &fds_write, &fds_error, NULL);
  
  if(retval != 1) e(1, "incorrect amount of ready file descriptors");


  if(FD_ISSET(fd, &fds_read)) e(2, "read should NOT be set");
  if(!FD_ISSET(fd, &fds_write)) e(3, "write should be set");
  if(FD_ISSET(fd, &fds_error)) e(4, "error should NOT be set");

  /* Block until ready; until child wrote stuff */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
  FD_SET(fd, &fds_read);
  retval = select(fd+1, &fds_read, NULL, &fds_error, NULL);
 
  if(retval != 1) e(5, "incorrect amount of ready file descriptors");
  if(!FD_ISSET(fd, &fds_read)) e(6, "read should be set");
  if(FD_ISSET(fd, &fds_error)) e(7, "error should not be set");


  FD_ZERO(&fds_read); FD_ZERO(&fds_error);
  FD_SET(fd,&fds_write);
  retval = select(fd+1, NULL, &fds_write, NULL, NULL);
  /* As it is impossible to write to a read only fd, this select should return
   * immediately with fd set in fds_write. */
  if(retval != 1) e(8, "incorrect amount or ready file descriptors");

  close(fd);
  waitpid(child, &retval, 0);
  exit(errct);
}

int main(int argc, char **argv) {
  int forkres;

  /* Get subtest number */
  if(argc != 2) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-1);
  } else if(sscanf(argv[1], "%d", &subtest) != 1) {
    printf("Usage: %s subtest_no\n", argv[0]);
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
