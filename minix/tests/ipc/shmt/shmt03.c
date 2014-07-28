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
/* 06/30/2001	Port to Linux	nsharoff@us.ibm.com */

/*
 * NAME
 *	shmt3
 *
 * CALLS
 *	shmctl(2) shmget(2) shmat(2)
 *
 * ALGORITHM
 * Create one shared memory segment and attach it twice to the same process, 
 * at an address that is chosen by the system. After the first attach has
 * completed, write to it and then do the second attach. 
 * Verify that the doubly attached segment contains the same data.
 *
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

/** LTP Port **/
#include "test.h"
#include "usctest.h"

char *TCID="shmt03";            /* Test program identifier.    */
int TST_TOTAL=4;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */
/**************/

#define		K_1 		1024
#define 	SUCCESSFUL	 1

int	first_attach,
	second_attach;
int	rm_shm(int);

int main(void)
{
	char *cp1, *cp2;
	int shmid;
	key_t key ; 


	key = (key_t) getpid() ;
	errno = 0 ;

/*------------------------------------------------------------*/


	if ((shmid = shmget(key, 16*K_1, IPC_CREAT|0666)) < 0) {
		perror("shmget");
		tst_resm(TFAIL, "shmget Failed: shmid = %d, errno = %d\n",
		shmid, errno) ;
		tst_exit() ;
	}

	tst_resm(TPASS,"shmget");

/*------------------------------------------------------------*/


	if ((cp1 = (char *)shmat(shmid, (void *)0, 0)) == (char *)-1) {
		perror("shmat");
		tst_resm(TFAIL, "shmat Failed: shmid = %d, errno = %d\n",
		shmid, errno) ;
	} else {
		*cp1 = '1' ;
		*(cp1+5*K_1) = '2' ;
		first_attach = SUCCESSFUL ;
	}

	tst_resm(TPASS,"1st shmat");

/*------------------------------------------------------------*/


	if ((cp2 = (char *)shmat(shmid, (void *)0, 0)) == (char *)-1) {
		perror("shmat");
		tst_resm(TFAIL, "shmat Failed: shmid = %d, errno = %d\n",
		shmid, errno) ;
	} 
	else { 
		second_attach = SUCCESSFUL  ;
		if ( (*cp2 != '1' || *(cp2+5*K_1) != '2') &&
		                      first_attach == SUCCESSFUL ) {
			tst_resm(TFAIL, 
			"Error: Shared memory contents\n") ;
		}
	}

	tst_resm(TPASS,"2nd shmat");	

/*---------------------------------------------------------------*/


	rm_shm(shmid) ;

	if ( first_attach  && second_attach ) {
		if ( *cp2 != '1' || *(cp2+5*K_1) != '2' ||
		     *cp1 != '1' || *(cp1+5*K_1) != '2'   ) {
			tst_resm(TFAIL, "Error: Shared memory contents\n") ;
		}
	}

	tst_resm(TPASS, "Correct shared memory contents");
/*-----------------------------------------------------------------*/
	tst_exit() ;

/*----------------------------------------------------------------*/
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

