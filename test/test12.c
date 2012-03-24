/* test 12 */

/* Copyright (C) 1987 by Martin Leisner. All rights reserved. */
/* Used by permission. */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define NUM_TIMES	1000
#define MAX_ERROR 2

#include "common.c"

int main(void);

int main()
{
  register int i;
  int k;

  start(12);

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

