
/* Code to test reasonable response to out-of-memory condition
 * of regular processes.
 */

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

#include <sys/mman.h>
#include <sys/wait.h>

#define MAX_ERROR 2

#include "magic.h"
#include "common.c"

int main (int argc, char *argv[])
{
  pid_t f;
  start(64);
#define NADDRS 500
#define LEN 4096
  static void *addrs[NADDRS];
  int i = 0;
  int st;

  if((f=fork()) == -1) {
  	e(1);
	exit(1);
  }

  if(f == 0) {
  	/* child: use up as much memory as we can */
	while((addrs[i++ % NADDRS] = minix_mmap(0, LEN, PROT_READ|PROT_WRITE,
		MAP_PREALLOC|MAP_CONTIG|MAP_ANON, -1, 0)) != MAP_FAILED)
			;
	exit(0);
  }

  /* parent: wait for child */
  if(waitpid(f, &st, 0) < 0) 
	perror("waitpid");

  quit();

  return(0);
}
