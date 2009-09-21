/*
 *
 *   Copyright (c) International Business Machines  Corp., 2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* 12/20/2002   Port to LTP     robbiew@us.ibm.com */
/* 06/30/2001   Port to Linux   nsharoff@us.ibm.com */

/*
 * NAME
 *	shmt10.c - test simultaneous shmat/shmdt
 *
 * CALLS
 *	shmget, shmat, shmdt, shmctl
 *
 * ALGORITHM
 *	Create a shared memory segment and fork a child. Both
 *	parent and child spin in a loop attaching and detaching
 *	the segment. After completing the specified number of
 *	iterations, the child exits and the parent deletes the
 *	segment.
 *
 * USAGE
 *  shmt10 [-i 500]
 *	-i # of iterations, default 500
 *
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define	SIZE	0x32768

/** LTP Port **/
#include "test.h"
#include "usctest.h"

char *TCID="shmt10";            /* Test program identifier.    */
int TST_TOTAL=2;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */
/**************/

int shmid;
key_t key;

int child(int);
int rm_shm(int);
void fini(int);

int main(int argc, char *argv[])
{
	char *c1=NULL;
	int pid, st;
	register int i;
	int iter = 500;
	int c;
	extern char *optarg;

	key = (key_t)getpid();
	signal(SIGTERM, fini);

/*--------------------------------------------------------*/

	while ((c = getopt(argc, argv, "i:")) != EOF) {
		switch (c) {
		case 'i':
			iter = atoi(optarg);
			break;
		default:
			tst_resm(TCONF, "usage: %s [-i <# iterations>]", argv[0]);
			tst_exit();
		}
	}


/*------------------------------------------------------------------------*/

	if ((shmid = shmget(key, SIZE, IPC_CREAT|0666)) < 0) {
		tst_resm(TFAIL,"shmget") ;
		tst_resm(TFAIL, "Error: shmid = %d\n", shmid) ;
		tst_exit() ;
	}

	pid = fork();
	switch(pid) {
	case -1:
		tst_resm(TBROK,"fork failed");
                tst_exit() ;
	case 0:
		child(iter);
		tst_exit();
	}

	for (i = 0; i < iter; i++) {
		if ((c1 = (char *) shmat(shmid, (void *)0, 0)) == (char *)-1) {
			tst_resm(TFAIL, 
			"Error shmat: iter %d, shmid = %d\n", i, shmid);
			break;
		}
		if (shmdt(c1) < 0) {
			tst_resm(TFAIL, 
			"Error: shmdt: iter %d ", i) ;
			break;
		}
	}
	while ( wait(&st) < 0 && errno == EINTR ) 
		       ;
	tst_resm(TPASS,"shmat,shmdt");
/*------------------------------------------------------------------------*/

	rm_shm(shmid);
	tst_exit();

/*------------------------------------------------------------------------*/
	return(0);
}

int rm_shm(shmid)
int shmid ;
{
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
                perror("shmctl");
                tst_resm(TFAIL,
                "shmctl Failed to remove: shmid = %d, errno = %d\n",
                shmid, errno) ;
                tst_exit();
        }
        return(0);
}

int child(iter)
int iter;
{
	register int i;
	char *c1;

	for (i = 0; i < iter; i++) {
		if ((c1 = (char *) shmat(shmid, (void *)0, 0)) == (char *)-1) {
			tst_resm(TFAIL, 
			"Error:child proc: shmat: iter %d, shmid = %d\n", 
			i, shmid);
			tst_exit();
		}
		if (shmdt(c1) < 0) {
			tst_resm(TFAIL, 
			"Error: child proc: shmdt: iter %d ", i);
			tst_exit();
		}
	}
	return(0);
}

void 
fini(int sig)
{
	rm_shm(shmid);
}
