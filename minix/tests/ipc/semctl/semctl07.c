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

/* 06/30/2001	Port to Linux	nsharoff@us.ibm.com */
/* 10/30/2002	Port to LTP	dbarrera@us.ibm.com */

/*
 * NAME
 *	semctl07
 *
 * CALLS
 *	semctl(2) semget(2)
 *
 * ALGORITHM
 *	Get and manipulate a set of semaphores.
 *
 * RESTRICTIONS
 *
 */


#include <sys/types.h>		/* needed for test		*/
#include <sys/ipc.h>		/* needed for test		*/
#include <sys/sem.h>		/* needed for test		*/
#include <signal.h>		/* needed for test		*/
#include <errno.h>		/* needed for test		*/
#include <stdio.h>		/* needed by testhead.h		*/
#include <sys/wait.h>		/* needed by testhead.h		*/
#include "ipcsem.h"
#include "test.h"
#include "usctest.h"


void setup(void);
void cleanup(void);


/*
 *These globals must be defined in the test.
 */


char *TCID="semctl07";           /* Test program identifier.    */
int TST_TOTAL=1;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */

int exp_enos[]={0};     /* List must end with 0 */



/*--------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	key_t key;
	int semid, nsems, status;
	struct semid_ds buf_ds;

	union semun {
		int val;
		struct semid_ds *buf;
		short *array;
	};

	union semun arg;

	setup();		/* temp file is now open	*/
/*--------------------------------------------------------------*/
	key = nsems = 1;
	if ((semid = semget(key, nsems, SEM_RA|IPC_CREAT)) == -1) {
		tst_resm(TFAIL, "semget() failed errno = %d", errno);
		tst_exit();
	}
	arg.buf = &buf_ds;
	if ((status = semctl(semid, 0, IPC_STAT, arg)) == -1) {
		tst_resm(TFAIL, "semctl() failed errno = %d", errno);
		semctl(semid, 1, IPC_RMID, arg);
		tst_exit();
	}

	/*
	 * Check contents of semid_ds structure.
	 */

	if (arg.buf->sem_nsems != nsems) {
		tst_resm(TFAIL, "error: unexpected number of sems %d", arg.buf->sem_nsems);
		tst_exit();
	}
	if (arg.buf->sem_perm.uid != getuid()) {
		tst_resm(TFAIL, "error: unexpected uid %d", arg.buf->sem_perm.uid);
		tst_exit();
	}
	if (arg.buf->sem_perm.gid != getgid()) {
		tst_resm(TFAIL, "error: unexpected gid %d", arg.buf->sem_perm.gid);
		tst_exit();
	}
	if (arg.buf->sem_perm.cuid != getuid()) {
		tst_resm(TFAIL, "error: unexpected cuid %d", arg.buf->sem_perm.cuid);
		tst_exit();
	}
	if (arg.buf->sem_perm.cgid != getgid()) {
		tst_resm(TFAIL, "error: unexpected cgid %d", arg.buf->sem_perm.cgid);
		tst_exit();
	}
	if ((status = semctl(semid, 0, GETVAL, arg)) == -1) {
		tst_resm(TFAIL, "semctl(GETVAL) failed errno = %d", errno);
		tst_exit();
	}
	arg.val = 1;
	if ((status = semctl(semid, 0, SETVAL, arg)) == -1) {
		tst_resm(TFAIL, "SEMCTL(SETVAL) failed errno = %d", errno);
		tst_exit();
	}
	if ((status = semctl(semid, 0, GETVAL, arg)) == -1) {
		tst_resm(TFAIL, "semctl(GETVAL) failed errno = %d", errno);
		tst_exit();
	}
	if (status != arg.val) {
		tst_resm(TFAIL, "error: unexpected value %d", status);
		tst_exit();
	}
	if ((status = semctl(semid, 0, GETPID, arg)) == -1) {
		tst_resm(TFAIL, "semctl(GETPID) failed errno = %d", errno);
		tst_exit();
	}
	status = getpid();
	if (status == 0) 
	{
		tst_resm(TFAIL, "error: unexpected pid %d", status);
		tst_exit();
	}
	if ((status = semctl(semid, 0, GETNCNT, arg)) == -1) {
		tst_resm(TFAIL, "semctl(GETNCNT) failed errno = %d", errno);
		tst_exit();
	}
	if (status != 0) {
		tst_resm(TFAIL, "error: unexpected semncnt %d", status);
		tst_exit();
	}
	if ((status = semctl(semid, 0, GETZCNT, arg)) == -1) {
		tst_resm(TFAIL, "semctl(GETZCNT) failed errno = %d", errno);
		tst_exit();
	}
	if (status != 0) {
		tst_resm(TFAIL, "error: unexpected semzcnt %d", status);
		tst_exit();
	}
		
	semctl(semid, 0, IPC_RMID, arg);
	if ((status = semctl(semid, 0, GETPID, arg)) != -1) {
		tst_resm(TFAIL, "semctl(IPC_RMID) failed");
		tst_exit();
	}

	tst_resm(TPASS, "semctl07 ran successfully!");
/*--------------------------------------------------------------*/
/* Clean up any files created by test before exit.		*/
/*--------------------------------------------------------------*/

	cleanup();
	return (0);
}
/*--------------------------------------------------------------*/

/***************************************************************
 * setup() - performs all ONE TIME setup for this test.
 *****************************************************************/
void
setup()
{
        /* You will want to enable some signal handling so you can capture
	 * unexpected signals like SIGSEGV.
	 *                   */
        tst_sig(NOFORK, DEF_HANDLER, cleanup);


        /* Pause if that option was specified */
        /* One cavet that hasn't been fixed yet.  TEST_PAUSE contains the code to
	 * fork the test with the -c option.  You want to make sure you do this
	 * before you create your temporary directory.
	 */
        TEST_PAUSE;
}


/***************************************************************
 * cleanup() - performs all ONE TIME cleanup for this test at
 * completion or premature exit.
 ****************************************************************/
void
cleanup()
{
        /*
	 * print timing stats if that option was specified.
	 * print errno log if that option was specified.
	 */
        TEST_CLEANUP;

        /* exit with return code appropriate for results */
        tst_exit();
}

