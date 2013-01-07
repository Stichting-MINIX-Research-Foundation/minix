/* t40d.c
 *
 * Test FIFOs and pipes
 *
 * Select works on regular files, (pseudo) terminal devices, streams-based
 * files, FIFOs, pipes, and sockets. This test verifies selecting for FIFOs
 * (named pipes) and pipes (anonymous pipes). This test will not verify most
 * error file descriptors, as the setting of this fdset in the face of an error
 * condition is implementation-specific (except for regular files (alway set) 
 * and sockets (protocol-specific or OOB data received), but those file types
 * are not being tested in this specific test).
 *
 * This test is part of a bigger select test. It expects as argument which sub-
 * test it is.
 *
 * [1] If a socket has a pending error, it shall be considered to have an
 * exceptional condition pending. Otherwise, what constitutes an exceptional
 * condition is file type-specific. For a file descriptor for use with a
 * socket, it is protocol-specific except as noted below. For other file types
 * it is implementation-defined. If the operation is meaningless for a
 * particular file type, pselect() or select() shall indicate that the
 * descriptor is ready for read or write operations, and shall indicate that
 * the descriptor has no exceptional condition pending.
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
#include <time.h>
#include <assert.h>

#define NAMEDPIPE1 "selecttestd-1"
#define NAMEDPIPE2 "selecttestd-2"
#define SENDSTRING "minixrocks"
#define DO_HANDLEDATA 1
#define DO_PAUSE 3
#define DO_TIMEOUT 7
#define MAX_ERROR 5

int errct = 0, subtest = -1;
char errbuf[1000];
int fd_ap[2]; /* Anonymous pipe; read from fd_ap[0], write to fd_ap[1] */
int fd_np1; /* Named pipe */
int fd_np2; /* Named pipe */

void e(int n, char *s) {
  printf("Subtest %d, error %d, %s\n", subtest, n, s);
  
  if (errct++ > MAX_ERROR) {
    printf("Too many errors; test aborted\n");
    exit(errct);
  }
}

void do_child(void) {
  struct timeval tv;
 
  /* Open named pipe for writing. This will block until a reader arrives. */
  if((fd_np1 = open(NAMEDPIPE1, O_WRONLY)) == -1) {
    printf("Error opening %s for writing, signalling parent to quit\n",
	   NAMEDPIPE1);
    perror(NULL);
    printf("Please make sure that %s is not in use while running this test\n",
	   NAMEDPIPE1);
    exit(-1);
  }

  /* Going to sleep for three seconds to allow the parent proc to get ready */
  tv.tv_sec = DO_HANDLEDATA;
  tv.tv_usec = 0; 
  select(0, NULL, NULL, NULL, &tv);

  /* Try to write. Doesn't matter how many bytes we actually send. */
  (void) write(fd_np1, SENDSTRING, strlen(SENDSTRING));

  /* Wait for another second to allow the parent to process incoming data */
  tv.tv_sec = DO_HANDLEDATA;
  tv.tv_usec = 0;
  (void) select(0,NULL, NULL, NULL, &tv);

  close(fd_np1);

  /* Wait for another second to allow the parent to process incoming data */
  tv.tv_sec = DO_HANDLEDATA;
  tv.tv_usec = 0;
  (void) select(0,NULL, NULL, NULL, &tv);

  /* Open named pipe for reading. This will block until a writer arrives. */
  if((fd_np2 = open(NAMEDPIPE2, O_RDONLY)) == -1) {
    printf("Error opening %s for reading, signalling parent to quit\n",
	   NAMEDPIPE2);
    perror(NULL);
    printf("Please make sure that %s is not in use while running this test\n",
	   NAMEDPIPE2);
    exit(-1);
  }

  /* Wait for another second to allow the parent to run some tests. */
  tv.tv_sec = DO_HANDLEDATA;
  tv.tv_usec = 0;
  (void) select(0, NULL, NULL, NULL, &tv);
  
  close(fd_np2);

  /*                             Anonymous pipe                              */

  /* Let the parent do initial read and write tests from and to the pipe. */
  tv.tv_sec = DO_PAUSE;
  tv.tv_usec = 0;
  (void) select(0, NULL, NULL, NULL, &tv);
  
  /* Unblock blocking read select by writing data */
  if(write(fd_ap[1], SENDSTRING, strlen(SENDSTRING)) < 0) {
    perror("Could not write to anonymous pipe");
    exit(-1);
  }

  exit(0);
}

int count_fds(int nfds, fd_set *fds) {
  /* Return number of bits set in fds */
  int i, result = 0;
  assert(fds != NULL && nfds > 0);
  for(i = 0; i < nfds; i++) {
    if(FD_ISSET(i, fds)) result++;
  }
  return result;
}

int empty_fds(int nfds, fd_set *fds) {
  /* Returns nonzero if the first bits up to nfds in fds are not set */
  int i;
  assert(fds != NULL && nfds > 0);
  for(i = 0; i < nfds; i++) if(FD_ISSET(i, fds)) return 0;
  return 1;
}

int compare_fds(int nfds, fd_set *lh, fd_set *rh) {
  /* Returns nonzero if lh equals rh up to nfds bits */
  int i;
  assert(lh != NULL && rh != NULL && nfds > 0);
  for(i = 0; i < nfds; i++) {
    if((FD_ISSET(i, lh) && !FD_ISSET(i, rh)) ||
       (!FD_ISSET(i, lh) && FD_ISSET(i, rh))) {
      return 0;
    }
  }
  return 1;
}

void dump_fds(int nfds, fd_set *fds) {
  /* Print a graphical representation of bits in fds */
  int i;
  if(fds != NULL && nfds > 0) {
    for(i = 0; i < nfds; i++) printf("%d ", (FD_ISSET(i, fds) ? 1 : 0));
    printf("\n");
  }
}

void do_parent(int child) {
  fd_set fds_read, fds_write, fds_error;
  fd_set fds_compare_read, fds_compare_write;
  struct timeval tv;
  time_t start, end;
  int retval;
  char buf[20];

  /* Open named pipe for reading. This will block until a writer arrives. */
  if((fd_np1 = open(NAMEDPIPE1, O_RDONLY)) == -1) {
    printf("Error opening %s for reading\n", NAMEDPIPE1);
    perror(NULL);
    printf("Please make sure that %s is not in use while running this test.\n",
	   NAMEDPIPE1);
    waitpid(child, &retval, 0);
    exit(-1);
  }
  
  /* Clear bit masks */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write);
  /* Set read and write bits */
  FD_SET(fd_np1, &fds_read);
  FD_SET(fd_np1, &fds_write);
  tv.tv_sec = DO_TIMEOUT;
  tv.tv_usec = 0;

  /* Test if we can read or write from/to fd_np1. As fd_np1 is opened read only
   * we cannot actually write, so the select should return immediately [1] and
   * the offending bit set in the fd set. We read from a pipe that is opened
   * with O_NONBLOCKING cleared, so it is guaranteed we can read.
   * However, at this moment the writer is sleeping, so the pipe is empty and
   * read is supposed to block. Therefore, only 1 file descriptor should be
   * ready. A timeout value is still set in case an error occurs in a faulty
   * implementation. */
  retval = select(fd_np1+1, &fds_read, &fds_write, NULL, &tv);

  /* Did we receive an error? */ 
  if(retval <= 0) {
    snprintf(errbuf, sizeof(errbuf),
	     "one fd should be set%s", (retval == 0 ? " (TIMEOUT)" : ""));
    e(1, errbuf);
  }

  if(!empty_fds(fd_np1+1,&fds_read)) e(2, "no read bits should be set");


  /* Make sure the write bit is set (and just 1 bit) */
  FD_ZERO(&fds_compare_write); FD_SET(fd_np1, &fds_compare_write);
  if(!compare_fds(fd_np1+1, &fds_compare_write, &fds_write))
    e(3, "write should be set");

  /* Clear sets and set up new bit masks */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write);
  FD_SET(fd_np1, &fds_read);
  tv.tv_sec = DO_TIMEOUT; /* To make sure we get to see some error messages
			     instead of blocking forever when the
			     implementation is faulty. A timeout causes retval
			     to be 0. */
  tv.tv_usec = 0;
  /* The sleeping writer is about to wake up and write data to the pipe. */
  retval = select(fd_np1+1, &fds_read, &fds_write, NULL, &tv);
  
  /* Correct amount of ready file descriptors? Just 1 read */
  if(retval != 1) {
    snprintf(errbuf, sizeof(errbuf),
	     "one fd should be set%s", (retval == 0 ? " (TIMEOUT)" : ""));
    e(4, errbuf);
  }

  if(!FD_ISSET(fd_np1, &fds_read)) e(5, "read should be set");

  /* Note that we left the write set empty. This should be equivalent to
   * setting this parameter to NULL. */
  if(!empty_fds(fd_np1+1, &fds_write)) e(6, "write should NOT be set");

  /* In case something went wrong above, we might end up with a child process
   * blocking on a write call we close the file descriptor now. Synchronize on
   * a read. */
  if(read(fd_np1, buf, sizeof(SENDSTRING)) < 0) perror("Read error");

  /* Close file descriptor. We're going to reverse the test */
  close(fd_np1);

  /* Wait for a second to allow the child to close the pipe as well */
  tv.tv_sec = DO_HANDLEDATA;
  tv.tv_usec = 0;
  retval = select(0,NULL, NULL, NULL, &tv);
  
  /* Open named pipe for writing. This call blocks until a reader arrives. */
  if((fd_np2 = open(NAMEDPIPE2, O_WRONLY)) == -1) {
    printf("Error opening %s for writing\n",
	   NAMEDPIPE2);
    perror(NULL);
    printf("Please make sure that %s is not in use while running this test\n",
	   NAMEDPIPE2);
    exit(-1);
  }

  /* At this moment the child process has opened the named pipe for reading and
   * we have opened it for writing. We're now going to reverse some of the
   * tests we've done earlier. */

  /* Clear sets and set up bit masks */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
  FD_SET(fd_np2, &fds_read);
  FD_SET(fd_np2, &fds_write);
  tv.tv_sec = DO_TIMEOUT;
  tv.tv_usec = 0;
  /* Select for reading from an fd opened O_WRONLY. This should return
   * immediately as it is not a meaningful operation [1] and is therefore not
   * blocking. The select should return two file descriptors are ready (the
   * failing read and valid write). */

  retval = select(fd_np2+1, &fds_read, &fds_write, &fds_error, &tv);

  /* Did we receive an error? */
  if(retval <= 0) {
    snprintf(errbuf, sizeof(errbuf),
	     "two fds should be set%s", (retval == 0 ? " (TIMEOUT)" : ""));
    e(7, errbuf);
  }

  /* Make sure read bit is set (and just 1 bit) */
  FD_ZERO(&fds_compare_read); FD_SET(fd_np2, &fds_compare_read);
  if(!compare_fds(fd_np2+1, &fds_compare_read, &fds_read))
    e(8, "read should be set");

  /* Write bit should be set (and just 1 bit) */
  FD_ZERO(&fds_compare_write); FD_SET(fd_np2, &fds_compare_write);
  if(!compare_fds(fd_np2+1, &fds_compare_write, &fds_write))
    e(9, "write should be set");

  if(!empty_fds(fd_np2+1, &fds_error))
    e(10, "Error should NOT be set");

  FD_ZERO(&fds_read); FD_ZERO(&fds_write);
  FD_SET(fd_np2, &fds_write);
  tv.tv_sec = DO_TIMEOUT;
  tv.tv_usec = 0;
  retval = select(fd_np2+1, &fds_read, &fds_write, NULL, &tv);

  /* Correct amount of ready file descriptors? Just 1 write */
  if(retval != 1) {
    snprintf(errbuf, sizeof(errbuf),
	     "one fd should be set%s", (retval == 0 ? " (TIMEOUT)" : ""));
    e(11, errbuf);
  }

  if(!empty_fds(fd_np2+1, &fds_read)) e(12, "read should NOT be set");
  
  /*                             Anonymous pipe                              */

  /* Check if we can write to the pipe */
  FD_ZERO(&fds_read); FD_ZERO(&fds_write);
  FD_SET(fd_ap[1], &fds_write);
  tv.tv_sec = DO_TIMEOUT;
  tv.tv_usec = 0;
  retval = select(fd_ap[1]+1, NULL, &fds_write, NULL, &tv); 

  /* Correct amount of ready file descriptors? Just 1 write */
  if(retval != 1) {
    snprintf(errbuf, sizeof(errbuf),
	     "one fd should be set%s", (retval == 0 ? " (TIMEOUT)" : ""));
    e(13, errbuf);
  }

  /* Make sure write bit is set (and just 1 bit) */
  FD_ZERO(&fds_compare_write); FD_SET(fd_ap[1], &fds_compare_write);
  if(!compare_fds(fd_ap[1]+1, &fds_compare_write, &fds_write))
    e(14, "write should be set");

  /* Intentionally test reading from pipe and letting it time out. */
  FD_SET(fd_ap[0], &fds_read);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  start = time(NULL);
  retval = select(fd_ap[0]+1, &fds_read, NULL, NULL, &tv);
  end = time(NULL);

  /* Did we time out? */
  if(retval != 0) e(15, "we should have timed out");

  /* Did it take us approximately 1 second? */
  if((int) (end - start) != 1) {
    snprintf(errbuf, sizeof(errbuf),
	     "time out is not 1 second (instead, it is %ld)",
	     (long int) (end - start));
    e(16, errbuf);    
  }

  /* Do another read, but this time we expect incoming data from child. */
  FD_ZERO(&fds_read);
  FD_SET(fd_ap[0], &fds_read);
  tv.tv_sec = DO_TIMEOUT;
  tv.tv_usec = 0;
  retval = select(fd_ap[0]+1, &fds_read, NULL, NULL, &tv);

  /* Correct amount of ready file descriptors? Just 1 read. */
  if(retval != 1) e(17, "one fd should be set");

  /* Is the read bit set? And just 1 bit. */
  FD_ZERO(&fds_compare_read); FD_SET(fd_ap[0], &fds_compare_read);
  if(!compare_fds(fd_ap[0]+1, &fds_compare_read, &fds_read))
    e(18, "read should be set.");
  
  /* By convention fd_ap[0] is meant to be used for reading from the pipe and
   * fd_ap[1] is meant for writing, where fd_ap is a an anonymous pipe.
   * However, it is unspecified what happens when fd_ap[0] is used for writing
   * and fd_ap[1] for reading. (It is unsupported on Minix.) As such, it is not
   * necessary to make test cases for wrong pipe file descriptors using select.
   */
  
  waitpid(child, &retval, 0);
  unlink(NAMEDPIPE2);
  unlink(NAMEDPIPE1);
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

  /* Create named pipe2. It is unlinked by do_parent. */
  if(mkfifo(NAMEDPIPE1, 0600) < 0) {
    printf("Could not create named pipe %s", NAMEDPIPE1);
    perror(NULL);
    exit(-1);
  }

  if(mkfifo(NAMEDPIPE2, 0600) < 0) {
    printf("Could not create named pipe %s", NAMEDPIPE2);
    perror(NULL);
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
