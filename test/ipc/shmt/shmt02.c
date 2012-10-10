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

/* 12/20/2002		 Port to LTP		 robbiew@us.ibm.com */
/* 06/30/2001		 Port to Linux		 nsharoff@us.ibm.com */

/*
 * NAME
 *		 shmt02
 *
 * CALLS
 *		 shmctl(2) shmget(2)
 *
 * ALGORITHM
 * Create and attach a shared memory segment, write to it
 * and then remove it.		  Verify that the shared memory segment
 * is accessible as long as the process is still alive.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/utsname.h>
#include <errno.h>

/** LTP Port **/
#include "test.h"
#include "usctest.h"

char *TCID="shmt02";            /* Test program identifier.    */
int TST_TOTAL=3;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */


/**************/

#define K_1 1024

int rm_shm(int);

int main(void)
{
		 register int		 shmid;
		 char		 		 *cp;
		 key_t		 		 key;

		 errno = 0;
		 key = (key_t)getpid() ;

/*----------------------------------------------------------------*/


		 if ((shmid = shmget(key, 16*K_1, IPC_CREAT|0666)) < 0 ) {
		 		 perror("shmget");
		 		 tst_resm(TFAIL, "shmget Failed: shmid = %d, errno = %d\n",
		 		 shmid, errno) ;
		 		 tst_exit() ;
		 }

		 tst_resm(TPASS, "shmget") ;

/*----------------------------------------------------------------*/


		 /* are we doing with ia64 or arm_arch_4t arch */
#if defined (__ia64__) || defined (__ARM_ARCH_4T__)
		 cp = (char *) shmat(shmid, (void *)NULL, 0);
#else		
 		 cp = (char *) shmat(shmid, (void *)0x80000, 0);
#endif
		 if (cp == (char *)-1) {
		 		 perror("shmat");
		 		 tst_resm(TFAIL, "shmat Failed: shmid = %d, errno = %d\n",
		 		 shmid, errno) ;
		 		 rm_shm(shmid) ;
		 		 tst_exit() ;		 
		 }

		 *cp     = '1';
		 *(cp+1) = '2';

		 tst_resm(TPASS, "shmat") ;

/*----------------------------------------------------------------*/


		 rm_shm(shmid) ;

		 if ( *cp != '1' || *(cp+1) != '2' ) {
		 		 tst_resm(TFAIL, 
		 		 "Error in shared memory contents: shmid = %d\n",
		 		 shmid);
		 }
		 
		 tst_resm(TPASS, "Correct shared memory contents") ;

/*------------------------------------------------------------------*/

		 tst_exit() ; 

/*-------------------- THIS LINE IS NOT REACHED -------------------*/
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

