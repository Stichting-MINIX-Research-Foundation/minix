/* t40e.c
 *
 * Test sockets
 *
 * Select works on regular files, (pseudo) terminal devices, streams-based
 * files, FIFOs, pipes, and sockets. This test verifies selecting for sockets.
 *
 * This test is part of a bigger select test. It expects as argument which sub-
 * test it is.
 *
 * Specific rules for sockets:
 * If a socket has a pending error, it shall be considered to have an
 * exceptional condition pending. Otherwise, what constitutes an exceptional
 * condition is file type-specific. For a file descriptor for use with a
 * socket, it is protocol-specific except as noted below. For other file types
 * it is implementation-defined. If the operation is meaningless for a
 * particular file type, pselect() or select() shall indicate that the
 * descriptor is ready for read or write operations, and shall indicate that
 * the descriptor has no exceptional condition pending.
 *
 * [1] If a descriptor refers to a socket, the implied input function is the
 * recvmsg()function with parameters requesting normal and ancillary data, such
 * that the presence of either type shall cause the socket to be marked as
 * readable. The presence of out-of-band data shall be checked if the socket
 * option SO_OOBINLINE has been enabled, as out-of-band data is enqueued with
 * normal data. If the socket is currently listening, then it shall be marked
 * as readable if an incoming connection request has been received, and a call
 * to the accept() function shall complete without blocking.
 *
 * [2] If a descriptor refers to a socket, the implied output function is the
 * sendmsg() function supplying an amount of normal data equal to the current
 * value of the SO_SNDLOWAT option for the socket. If a non-blocking call to
 * the connect() function has been made for a socket, and the connection
 * attempt has either succeeded or failed leaving a pending error, the socket
 * shall be marked as writable.
 * 
 * [3] A socket shall be considered to have an exceptional condition pending if
 * a receive operation with O_NONBLOCK clear for the open file description and
 * with the MSG_OOB flag set would return out-of-band data without blocking.
 * (It is protocol-specific whether the MSG_OOB flag would be used to read
 * out-of-band data.) A socket shall also be considered to have an exceptional
 * condition pending if an out-of-band data mark is present in the receive
 * queue. Other circumstances under which a socket may be considered to have an
 * exceptional condition pending are protocol-specific and
 * implementation-defined.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <netdb.h>

#define DO_HANDLEDATA 1
#define DO_PAUSE 3
#define DO_TIMEOUT 7
#define MYPORT 3490
#define NUMCHILDREN 5
#define MAX_ERROR 10

int errct = 0, subtest = -1;
char errbuf[1000];

void e(int n, char *s) {
  printf("Subtest %d, error %d, %s\n", subtest, n, s);

  if (errct++ > MAX_ERROR) {
    printf("Too many errors; test aborted\n");
    exit(errct);
  }
}

/* All *_fds routines are helping routines. They intentionally use FD_* macros
   in order to prevent making assumptions on how the macros are implemented.*/

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

void do_child(int childno) {
  int fd_sock, port;
  int retval;

  fd_set fds_read, fds_write, fds_error;
  fd_set fds_compare_write;

  struct hostent *he;
  struct sockaddr_in server;
  struct timeval tv;

  if((fd_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Error getting socket\n");
    exit(-1);
  }

  if((he = gethostbyname("127.0.0.1")) == NULL){/*"localhost" might be unknown*/
    perror("Error resolving");
    exit(-1);
  }

  /* Child 4 connects to the wrong port. See Actual testing description below.*/
  port = (childno == 3 ? MYPORT + 1 : MYPORT);

  memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
  server.sin_family = AF_INET;
  server.sin_port = htons(port);

#if 0
  printf("Going to connect to: %s:%d\n", inet_ntoa(server.sin_addr),
	 ntohs(server.sin_port));
#endif

  /* Normally we'd zerofill sin_zero, but there is no such thing on Minix */
#ifndef _MINIX
  memset(server.sin_zero, '\0', sizeof server.sin_zero);
#endif

  /* Wait for parent to set up connection */
  tv.tv_sec = (childno <= 1 ? DO_PAUSE : DO_TIMEOUT);
  tv.tv_usec = 0;
  retval = select(0, NULL, NULL, NULL, &tv);

  /* All set, let's do some testing */
  /* Children 3 and 4 do a non-blocking connect */
  if(childno == 2 || childno == 3)
    fcntl(fd_sock, F_SETFL, fcntl(fd_sock, F_GETFL, 0) | O_NONBLOCK);
 
  if(connect(fd_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    /* Well, we don't actually care. The connect is non-blocking and is
       supposed to "in progress" at this point. */
  }

  if(childno == 2 || childno == 3) { /* Children 3 and 4 */
    /* Open Group: "If a non-blocking call to the connect() function has been
       made for a socket, and the connection attempt has either succeeded or
       failed leaving a pending error, the socket shall be marked as writable.
       ...
       A socket shall be considered to have an exceptional condition pending if
       a receive operation with O_NONBLOCK clear for the open file description
       and with the MSG_OOB flag set would return out-of-band data without
       blocking. (It is protocol-specific whether the MSG_OOB flag would be used
       to read out-of-band data.) A socket shall also be considered to have an
       exceptional condition pending if an out-of-band data mark is present in
       the receive queue. Other circumstances under which a socket may be
       considered to have an exceptional condition pending are protocol-specific
       and implementation-defined."

       In other words, it only makes sense for us to check the write set as the
       read set is not expected to be set, but is allowed to be set (i.e.,
       unspecified) and whether the error set is set is implementation-defined.
    */
    FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
    FD_SET(fd_sock, &fds_write);
    tv.tv_sec = DO_TIMEOUT;
    tv.tv_usec = 0;
    retval = select(fd_sock+1, NULL, &fds_write, NULL, &tv);
    

    if(retval <= 0) e(6, "expected one fd to be ready");
    
    FD_ZERO(&fds_compare_write); FD_SET(fd_sock, &fds_compare_write);
    if(!compare_fds(fd_sock+1, &fds_compare_write, &fds_compare_write))
      e(7, "write should be set");
  }

  if(close(fd_sock) < 0) {
    perror("Error disconnecting");
    exit(-1);
  }

  exit(errct);
}

void do_parent(void) {
#ifndef _MINIX
  int yes = 1;
#endif
  int fd_sock, fd_new, exitstatus;
  int sockets[NUMCHILDREN], i;
  fd_set fds_read, fds_write, fds_error;
  fd_set fds_compare_read, fds_compare_write;
  struct timeval tv;
  int retval, childresults = 0;

  struct sockaddr_in my_addr;
  struct sockaddr_in other_addr;
  socklen_t other_size;

  if((fd_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Error getting socket\n");
    exit(-1);
  }

  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(MYPORT); /* Short, network byte order */
  my_addr.sin_addr.s_addr = INADDR_ANY;
  /* Normally we'd zerofill sin_zero, but there is no such thing on Minix */
#ifndef _MINIX
  memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);
#endif
  
  /* Reuse port number. Not implemented in Minix. */
#ifndef _MINIX
  if(setsockopt(fd_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    perror("Error setting port reuse option");
    exit(-1);
  }
#endif

  /* Bind to port */
  if(bind(fd_sock, (struct sockaddr *) &my_addr, sizeof my_addr) < 0) {
    perror("Error binding to port");
    exit(-1);
  }
  
  /* Mark socket to be used for incoming connections */
  if(listen(fd_sock, 20) < 0) {
    perror("Listen");
    exit(-1);
  }
    
  /*                              Actual testing                              */
  /* While sockets resemble file descriptors, they are not the same at all.
     We can read/write from/to and close file descriptors, but we cannot open
     them O_RDONLY or O_WRONLY; they are always O_RDWR (other flags do not make
     sense regarding sockets). As such, we cannot provide wrong file descriptors
     to select, except for descriptors that are not in use.
     We will test standard behavior and what is described in [2]. [1] and [3]
     are not possible to test on Minix, as Minix does not support OOB data. That
     is, the TCP layer can handle it, but there is no socket interface for it.
     Our test consists of waiting for input from the first two children and
     waiting to write output [standard usage]. Then the first child closes its
     connection we select for reading. This should fail with error set. Then we
     close child number two on our side and select for reading. This should fail
     with EBADF. Child number three shall then do a non-blocking connect (after
     waiting for DO_PAUSE seconds) and do a select, resulting in being marked
     ready for writing. Subsequently child number four also does a non-blocking
     connect to loclhost on MYPORT+1 (causing the connect to fail) and then does
     a select. This should result in write and error being set (error because of
     pending error).
  */

  /* Accept and store connections from the first two children */
  other_size = sizeof(other_addr);
  for(i = 0; i < 2; i++) {
    fd_new = accept(fd_sock, (struct sockaddr *) &other_addr, &other_size);
    if(fd_new < 0) break;
    sockets[i] = fd_new;
  }

  /* If we break out of the for loop, we ran across an error and want to exit.
     Check whether we broke out. */
  if(fd_new < 0) {
    perror("Error accepting connection");
    exit(-1);
  }

  /* Select error condition checking */
  for(childresults = 0; childresults < 2; childresults++) {
    FD_ZERO(&fds_read); FD_ZERO(&fds_write); FD_ZERO(&fds_error);
    FD_SET(sockets[childresults], &fds_read);
    FD_SET(sockets[childresults], &fds_write);
    FD_SET(sockets[childresults], &fds_error);
    tv.tv_sec = DO_TIMEOUT;
    tv.tv_usec = 0;
    
    retval = select(sockets[childresults]+1, &fds_read, &fds_write, &fds_error,
		    &tv);
    
    if(retval <= 0) {
      snprintf(errbuf, sizeof(errbuf),
	       "two fds should be set%s", (retval == 0 ? " (TIMEOUT)" : ""));
      e(1, errbuf);
    }

    FD_ZERO(&fds_compare_read); FD_ZERO(&fds_compare_write);
    FD_SET(sockets[childresults], &fds_compare_write);

    /* We can't say much about being ready for reading at this point or not. It
       is not specified and the other side might have data ready for us to read
    */
    if(!compare_fds(sockets[childresults]+1, &fds_compare_write, &fds_write))
      e(2, "write should be set");

    if(!empty_fds(sockets[childresults]+1, &fds_error))
      e(3, "no error should be set");
  }


  /* We continue by accepting a connection of child 3 */
  fd_new = accept(fd_sock, (struct sockaddr *) &other_addr, &other_size);
  if(fd_new < 0) {
    perror("Error accepting connection\n");
    exit(-1);
  }
  sockets[2] = fd_new;
   
  /* Child 4 will never connect */

  /* Child 5 is still pending to be accepted. Open Group: "If the socket is
     currently listening, then it shall be marked as readable if an incoming
     connection request has been received, and a call to the accept() function
     shall complete without blocking."*/
  FD_ZERO(&fds_read);
  FD_SET(fd_sock, &fds_read);
  tv.tv_sec = DO_TIMEOUT;
  tv.tv_usec = 0;
  retval = select(fd_sock+1, &fds_read, NULL, NULL, &tv);
  if(retval <= 0) {
    snprintf(errbuf, sizeof(errbuf),
	     "one fd should be set%s", (retval == 0 ? " (TIMEOUT)" : ""));
    e(4, errbuf);
  }

  /* Check read bit is set */
  FD_ZERO(&fds_compare_read); FD_SET(fd_sock, &fds_compare_read);
  if(!compare_fds(fd_sock+1, &fds_compare_read, &fds_read))
    e(5, "read should be set");


  /* Accept incoming connection to unblock child 5 */
  fd_new = accept(fd_sock, (struct sockaddr *) &other_addr, &other_size);
  if(fd_new < 0) {
    perror("Error accepting connection\n");
    exit(-1);
  }
  sockets[4] = fd_new;


  /* We're done, let's wait a second to synchronize children and parent. */
  tv.tv_sec = DO_HANDLEDATA;
  tv.tv_usec = 0;
  select(0, NULL, NULL, NULL, &tv);

  /* Close connection with children. */
  for(i = 0; i < NUMCHILDREN; i++) {
    if(i == 3) /* No need to disconnect child 4 that failed to connect. */
      continue;

    if(close(sockets[i]) < 0) {
      perror(NULL);
    }
  }

  /* Close listening socket */
  if(close(fd_sock) < 0) {
    perror("Closing listening socket");
    errct++;
  }

  for(i = 0; i < NUMCHILDREN; i++) {
    wait(&exitstatus); /* Wait for children */
    if(exitstatus > 0)
      errct += WEXITSTATUS(exitstatus); /* and count their errors, too. */
  }

  exit(errct);
}

int main(int argc, char **argv) {
  int forkres, i;

  /* Get subtest number */
  if(argc != 2) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-2);
  } else if(sscanf(argv[1], "%d", &subtest) != 1) {
    printf("Usage: %s subtest_no\n", argv[0]);
    exit(-2);
  }
  
  /* Fork off a bunch of children */
  for(i = 0; i < NUMCHILDREN; i++) {
      forkres = fork();
      if(forkres == 0) do_child(i);
      else if(forkres < 0) {
	perror("Unable to fork");
	exit(-1);
      }
  }
  /* do_child always calls exit(), so when we end up here, we're the parent. */
  do_parent();

  exit(-2); /* We're not supposed to get here. Both do_* routines should exit.*/
}
