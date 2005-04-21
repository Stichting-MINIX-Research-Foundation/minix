/* test 12 */

/* Copyright (C) 1987 by Martin Leisner. All rights reserved. */
/* Used by permission. */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define NUM_TIMES	1000

int errct = 0;

_PROTOTYPE(int main, (void));
_PROTOTYPE(void quit, (void));

int main()
{
  register int i;
  int k;

  printf("Test 12 ");
  fflush(stdout);		/* have to flush for child's benefit */

  system("rm -rf DIR_12; mkdir DIR_12");
  chdir("DIR_12");

  for (i = 0; i < NUM_TIMES; i++) switch (fork()) {
	    case 0:	exit(1);	  		break;
	    case -1:
		printf("fork broke\n");
		exit(1);
	    default:	wait(&k);	  		break;
	}

  quit();
  return(-1);			/* impossible */
}

void quit()
{

  chdir("..");
  system("rm -rf DIR*");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
