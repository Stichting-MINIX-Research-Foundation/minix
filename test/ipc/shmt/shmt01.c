/*
 * NAME
 *		 shmt01
 *
 * CALLS
 *		 shmat(2) shmget(2) shmdt(2)
 *
 * ALGORITHM
 * Create and attach a shared memory segment, write to it
 * and then detach the shared memroy twice, the second one will FAIL.
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

char *TCID="shmt01";            /* Test program identifier.    */
int TST_TOTAL=4;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */


/**************/

#define K_1 1024

int rm_shm(int);

int main(void)
{
		 register int		 shmid;
		 char		 		 *cp;
		 key_t		 		 key;
		 int r;

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

		 r = shmdt(cp);
		 if (r  < 0) {
			 perror("shmdt");
			 tst_resm(TFAIL, "shmdt Failed: shmid = %d, errno = %d\n",
					 shmid, errno);
			 rm_shm(shmid);
			 tst_exit();
		 }

		 tst_resm(TPASS, "shmdt first time.");

		 r = shmdt(cp);
		 if (r == 0) {
			 perror("shmdt");
			 tst_resm(TFAIL, "shmdt Failed: shmid = %d, errno = %d\n",
					 shmid, errno);
			 rm_shm(shmid);
			 tst_exit();
		 }

		 rm_shm(shmid) ;

		 tst_resm(TPASS, "shmdt second time.");

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

