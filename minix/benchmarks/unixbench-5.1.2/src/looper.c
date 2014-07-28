/*******************************************************************************
 *  The BYTE UNIX Benchmarks - Release 1
 *          Module: looper.c   SID: 1.4 5/15/91 19:30:22
 *
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	Ben Smith or Tom Yager at BYTE Magazine
 *	ben@bytepb.byte.com   tyager@bytepb.byte.com
 *
 *******************************************************************************
 *  Modification Log:
 *
 *  February 25, 1991 -- created (Ben S.)
 *  October 22, 1997 - code cleanup to remove ANSI C compiler warnings
 *                     Andy Kahn <kahn@zk3.dec.com>
 *
 ******************************************************************************/
char SCCSid[] = "@(#) @(#)looper.c:1.4 -- 5/15/91 19:30:22";
/*
 *  Shell Process creation
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "timeit.c"

unsigned long iter;
char *cmd_argv[28];
int  cmd_argc;

void report(int sig)
{
        fprintf(stderr,"COUNT|%lu|60|lpm\n", iter);
	exit(0);
}

int main(int argc, char *argv[])
{
int	slave, count, duration;
int	status;

if (argc < 2)
	{
	fprintf(stderr,"Usage: %s duration command [args..]\n", argv[0]);
	fprintf(stderr,"  duration in seconds\n");
	exit(1);
	}

if((duration = atoi(argv[1])) < 1)
	{
	fprintf(stderr,"Usage: %s duration command [arg..]\n", argv[0]);
	fprintf(stderr,"  duration in seconds\n");
	exit(1);
	}

/* get command  */
cmd_argc=argc-2;
for( count=2;count < argc; ++count)
	cmd_argv[count-2]=argv[count];
#ifdef DEBUG
printf("<<%s>>",cmd_argv[0]);
for(count=1;count < cmd_argc; ++count)
	printf(" <%s>", cmd_argv[count]);
putchar('\n');
exit(0);
#endif

iter = 0;
wake_me(duration, report);

while (1)
	{
	if ((slave = fork()) == 0)
		{ /* execute command */
		execvp(cmd_argv[0],cmd_argv);
		exit(99);
		}
	else if (slave < 0)
		{
		/* woops ... */
		fprintf(stderr,"Fork failed at iteration %lu\n", iter);
		perror("Reason");
		exit(2);
		}
	else
		/* master */
		wait(&status);
        if (status == 99 << 8)
                {
                fprintf(stderr, "Command \"%s\" didn't exec\n", cmd_argv[0]);
                exit(2);
                }
	else if (status != 0)
		{
		fprintf(stderr,"Bad wait status: 0x%x\n", status);
		exit(2);
		}
	iter++;
	}
}
