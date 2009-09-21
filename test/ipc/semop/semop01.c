/*
 *
 *   Copyright (c) International Business Machines  Corp., 2001
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

/*
 * NAME
 *	semop01.c
 *
 * DESCRIPTION
 *	semop01 - test that semop() basic functionality is correct
 *
 * ALGORITHM
 *	create a semaphore set and initialize some values
 *	loop if that option was specified
 *	call semop() to set values for the primitive semaphores
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing
 *		get the semaphore values and compare with expected values
 *	  	if correct,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	  else issue a PASS message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  semop01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
 *     where,  -c n : Run n copies concurrently.
 *             -f   : Turn off functionality Testing.
 *	       -i n : Execute test n times.
 *	       -I x : Execute test for x seconds.
 *	       -P x : Pause for x seconds between iterations.
 *	       -t   : Turn on syscall timing.
 *
 * HISTORY
 *	03/2001  - Written by Wayne Boyer
 *	17/01/02 - Modified. Manoj Iyer, IBM Austin. TX. manjo@austin.ibm.com
 *	           4th argument to semctl() system call was modified according
 *	           to man pages. 
 *	           In my opinion The test should not even have compiled but
 *	           it was working due to some mysterious reason.
 *
 * RESTRICTIONS
 *	none
 */

#include "ipcsem.h"

#define NSEMS	4	/* the number of primitive semaphores to test */

char *TCID = "semop01";
int TST_TOTAL = 1;
extern int Tst_count;

int sem_id_1 = -1;	/* a semaphore set with read & alter permissions */


struct sembuf sops[PSEMS];	/* an array of sembuf structures */


int main(int ac, char **av)
{
        union semun get_arr;
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */
	int i;
	int fail = 0;

        get_arr.array = malloc(sizeof(unsigned short int) * PSEMS);
	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

	setup();			/* global setup */

	/* The following loop checks looping state if -i option given */

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		/* reset Tst_count in case we are looping */
		Tst_count = 0;

		/*
		 * Use TEST macro to make the call
		 */

		TEST(semop(sem_id_1, sops, NSEMS));
	
		if (TEST_RETURN == -1) {
			tst_resm(TFAIL, "%s call failed - errno = %d "
				 ": %s", TCID, TEST_ERRNO,
				 strerror(TEST_ERRNO));
		} else {
			if (STD_FUNCTIONAL_TEST) {

				/* get the values and make sure they */
				/* are the same as what was set      */
				if (semctl(sem_id_1, 0, GETALL, get_arr) ==
				    -1) {
					tst_brkm(TBROK, cleanup, "semctl() "
						 "failed in functional test");
				}

				for (i=0; i<NSEMS; i++) {
					if (get_arr.array[i] != i*i) {
						fail = 1;
					}
				}
				if (fail) {
					tst_resm(TFAIL, "semaphore values"
						 " are not expected");
				} else {
					tst_resm(TPASS, "semaphore values"
						 " are correct");
				}
		
			} else {
				tst_resm(TPASS, "call succeeded");
			}
		}


		/*
		 * clean up things in case we are looping
		 */
		get_arr.val = 0;
		for (i=0; i<NSEMS; i++) {
			if(semctl(sem_id_1, i, SETVAL, get_arr) == -1) {
				tst_brkm(TBROK, cleanup, "semctl failed");
			}
		}
	}

	cleanup();

	/*NOTREACHED*/
	return(0);
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
	int i;

	/* capture signals */
	tst_sig(NOFORK, DEF_HANDLER, cleanup);

	/* Pause if that option was specified */
	TEST_PAUSE;

	/*
	 * Create a temporary directory and cd into it.
	 * This helps to ensure that a unique msgkey is created.
	 * See ../lib/libipc.c for more information.
	 */
	tst_tmpdir();

	/* get an IPC resource key */
	semkey = getipckey();

	/* create a semaphore set with read and alter permissions */
	if ((sem_id_1 =
	     semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL | SEM_RA)) == -1) {
		tst_brkm(TBROK, cleanup, "couldn't create semaphore in setup");
	}
	
	/* set up some values for the first four primitive semaphores */
	for (i=0; i<NSEMS; i++){
		sops[i].sem_num = i;
		sops[i].sem_op = i*i;	/* 0, 1, 4, 9, */
		sops[i].sem_flg = SEM_UNDO;
	}
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the semaphore resouce */
	rm_sema(sem_id_1);

	/* Remove the temporary directory */
	tst_rmdir();

	/*
	 * print timing stats if that option was specified.
	 * print errno log if that option was specified.
	 */
	TEST_CLEANUP;

	/* exit with return code appropriate for results */
	tst_exit();
}

