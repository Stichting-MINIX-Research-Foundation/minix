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
 *	shmt07
 *
 * CALLS
 *	shmctl(2) shmget(2) shmat(2)
 *
 * ALGORITHM
 * Create and attach a shared memory segment, write to it
 * and then fork a child. The child Verifies that the shared memory segment
 * that it inherited from the parent conatins the same data that was originally
 * written to it by the parent.
 *
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/utsname.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#define		ADDR	(void *)0x80000
#define		ADDR_IA (void *)0x40000000
#define		SIZE	16*1024

/** LTP Port **/
#include "test.h"
#include "usctest.h"


char *TCID="shmt07";            /* Test program identifier.    */
int TST_TOTAL=2;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */
/**************/

int child(void);
int rm_shm(int);

int main(void)
{
	char	*cp=NULL;
	int	shmid, pid, status;
	key_t 	key;

	key = (key_t) getpid() ;

/*---------------------------------------------------------*/

	errno = 0;

	if ((shmid = shmget(key, SIZE, IPC_CREAT|0666)) < 0) {
		perror("shmget");
		tst_resm(TFAIL,"Error: shmget: shmid = %d, errno = %d\n",
		shmid, errno) ;
		tst_exit() ;
	}

#ifdef __ia64__	
	  cp = (char *) shmat(shmid, ADDR_IA, 0);
#elif defined(__ARM_ARCH_4T__)
	  cp = (char *) shmat(shmid, (void *) NULL, 0);
#else
	  cp = (char *) shmat(shmid, ADDR, 0);
#endif
	if (cp == (char *)-1) {
		perror("shmat");
		tst_resm(TFAIL,
		"Error: shmat: shmid = %d, errno = %d\n",
		shmid, errno) ;
		rm_shm(shmid) ;
		tst_exit() ;
	}

	*cp 	= '1';
	*(cp+1) = '2';

	tst_resm(TPASS,"shmget,shmat");

/*-------------------------------------------------------*/


	pid = fork() ;
	switch (pid) {
	    case -1 :
			tst_resm(TBROK,"fork failed");
       		        tst_exit() ;
	
           case 0  :  	
			if (*cp != '1') {
				tst_resm(TFAIL, "Error: not 1\n");
			}
			if (*(cp+1) != '2') {
				tst_resm(TFAIL, "Error: not 2\n");
			}
			tst_exit() ;
	}

	/* parent */
	while( wait(&status) < 0 && errno == EINTR ) ;

	tst_resm(TPASS,"cp & cp+1 correct") ;

/*-----------------------------------------------------------*/
	rm_shm(shmid) ;
	tst_exit() ;
/*-----------------------------------------------------------*/
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

