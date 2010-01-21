/*******************************************************************************
 *  The BYTE UNIX Benchmarks - Release 3
 *          Module: hanoi.c   SID: 3.3 5/15/91 19:30:20
 *
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	Ben Smith, Rick Grehan or Tom Yager
 *	ben@bytepb.byte.com   rick_g@bytepb.byte.com   tyager@bytepb.byte.com
 *
 *******************************************************************************
 *  Modification Log:
 *  $Header: hanoi.c,v 3.5 87/08/06 08:11:14 kenj Exp $
 *  August 28, 1990 - Modified timing routines (ty)
 *  October 22, 1997 - code cleanup to remove ANSI C compiler warnings
 *                     Andy Kahn <kahn@zk3.dec.com>
 *
 ******************************************************************************/
char SCCSid[] = "@(#) @(#)hanoi.c:3.3 -- 5/15/91 19:30:20";

#define other(i,j) (6-(i+j))

#include <stdio.h>
#include <stdlib.h>
#include "timeit.c"

void mov(int n, int f, int t);

unsigned long iter = 0;
int num[4];
long cnt;

void report(int sig)
{
	fprintf(stderr,"COUNT|%ld|1|lps\n", iter);
	exit(0);
}


int main(int argc, char *argv[])
{
	int disk=10, /* default number of disks */
         duration;

	if (argc < 2) {
		fprintf(stderr,"Usage: %s duration [disks]\n", argv[0]);
		exit(1);
		}
	duration = atoi(argv[1]);
	if(argc > 2) disk = atoi(argv[2]);
	num[1] = disk;

	wake_me(duration, report);

	while(1) {
		mov(disk,1,3);
		iter++;
		}

	exit(0);
}

void mov(int n, int f, int t)
{
	int o;
	if(n == 1) {
		num[f]--;
		num[t]++;
		return;
	}
	o = other(f,t);
	mov(n-1,f,o);
	mov(1,f,t);
	mov(n-1,o,t);
}
