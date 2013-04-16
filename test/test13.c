/* test 13 */

/* File: pipes.c - created by Marty Leisner */
/* Leisner.Henr         1-Dec-87  8:55:04 */

/* Copyright (C) 1987 by Martin Leisner. All rights reserved. */
/* Used by permission. */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define BLOCK_SIZE 	1000
#define NUM_BLOCKS	1000

char buffer[BLOCK_SIZE];

int max_error = 2;
#include "common.h"


int main(void);
void quit(void);

int main()
{
  int stat_loc, pipefd[2];
  register int i;

  start(13);

  pipe(pipefd);

  switch (fork()) {
      case 0:
	/* Child code */
	for (i = 0; i < NUM_BLOCKS; i++)
	         if (read(pipefd[0], buffer, BLOCK_SIZE) != BLOCK_SIZE) break;
	exit(0);

      case -1:
	perror("fork broke");
	exit(1);

      default:
	/* Parent code */
	for (i = 0; i < NUM_BLOCKS; i++) write(pipefd[1], buffer, BLOCK_SIZE);
	wait(&stat_loc);
	break;
  }
  quit();
  return(-1);			/* impossible */
}

