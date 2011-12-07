/*
 * gcov-pull - Request gcov data from server and write it to gcda files
 * Author: Anton Kuijsten
*/

#include <fcntl.h>
#include <stdio.h>
#include <lib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#ifndef __NBSD_LIBC
#include <alloca.h>
#endif
#include <string.h>
#include <assert.h>
#include <minix/gcov.h>

#define BUFF_SZ (4 * 1024 * 1024)	/* 4MB */

int read_int(void);

char *buff_p;

/* helper function to read int from the buffer */
int read_int(void)
{
	int res;
	memcpy(&res, buff_p, sizeof(int));
	buff_p += sizeof(int);
	return res;
}

int main(int argc, char *argv[])
{
  FILE *fd = NULL;
  int server_nr, command, size, result;
  char buff[BUFF_SZ]; /* Buffer for all the metadata and file data sent */

  if(argc!=2 || sscanf(argv[1], "%d", &server_nr)!=1) {
  	fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
  	return 1;
  }

  /*
    When making a GCOV call to a server, the gcov library linked into
    the server will try to write gcov data to disk. This  writing is
    normally done with calls to the vfs,  using stdio library calls.
    This is not correct behaviour for servers, especially vfs itself.
    Therefore, the server catches those attempts.  The messages used for
    this communication are stored in a buffer. When the gcov operation
    is  done, the buffer is copied from the server to this user space,
    from where the calls are finally made to the vfs. GCOV calls to the
    various servers are all routed trough vfs. For more information, see
    the <minix/gcov.h> header file.
  */
  
  /* visit complete buffer, so vm won't has to 
     manage the pages while flushing
   */ 
  memset(buff, 'a', sizeof(buff));

  buff_p = buff;

  result = gcov_flush_svr(buff_p, BUFF_SZ, server_nr);

  if(result >= BUFF_SZ) {
    fprintf(stderr, "Too much data to hold in buffer: %d\n", result);
    fprintf(stderr, "Maximum: %d\n", BUFF_SZ);
    return 1;
  }

  if(result < 0) {
    fprintf(stderr, "Call failed\n");
    return 1;
  }

  /* At least GCOVOP_END opcode expected. */
  if(result < sizeof(int)) {
    fprintf(stderr, "Invalid gcov data from pid %d\n", server_nr);
    return 1;
  }

  /* Only GCOVOP_END is valid but empty. */
  if(result == sizeof(int)) {
    fprintf(stderr, "no gcov data.\n");
    return 0;
  }

  /* Iterate through the system calls contained in the buffer,
   * and execute them
   */
  while((command=read_int()) != GCOVOP_END) {
  	char *fn;
	switch(command) {
		case GCOVOP_OPEN:
			size = read_int();
			fn = buff_p;
			if(strchr(fn, '/')) {
				fn = strrchr(fn, '/');
				assert(fn);
				fn++;
			}
			assert(fn);
			if(!(fd = fopen(fn, "w+"))) {
				perror(buff_p);
				exit(1);
			}
			buff_p += size;
			break;
		case GCOVOP_CLOSE:
			if(!fd) {
				fprintf(stderr, "bogus close\n");
				exit(1);
			}
			fclose(fd);
			fd = NULL;
			break;
		case GCOVOP_WRITE:
			size = read_int();
			fwrite(buff_p, size, 1, fd);
			buff_p += size;
			break;
		default:
			fprintf(stderr, "bogus command %d in buffer.\n",
				command);
			exit(1);
	}
  }

  return 0;
}
