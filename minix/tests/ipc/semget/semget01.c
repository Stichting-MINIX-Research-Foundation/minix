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
 *	semget01.c
 *
 * DESCRIPTION
 *	semget01 - test that semget() correclty creates a semaphore set
 *
 * ALGORITHM
 *	loop if that option was specified
 *	call semget() to create the semaphore set
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing
 *		stat the semaphore set
 *		if the number of primitive semaphores is correct and
 *		   the semaphore uid == the process uid
 *	  	then,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  semget01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
 *     where,  -c n : Run n copies concurrently.
 *             -f   : Turn off functionality Testing.
 *	       -i n : Execute test n times.
 *	       -I x : Execute test for x seconds.
 *	       -P x : Pause for x seconds between iterations.
 *	       -t   : Turn on syscall timing.
 *
 * HISTORY
 *	03/2001 - Written by Wayne Boyer
 *
 * RESTRICTIONS
 *	none
 */

#include "ipcsem.h"
#include "usctest.h"


char *TCID = "semget01";
int TST_TOTAL = 1;
extern int Tst_count;

int sem_id_1 = -1;

int main(int ac, char **av)
{
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */
	void check_functionality(void);

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

		TEST(semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL | SEM_RA));
	
		if (TEST_RETURN == -1) {
			tst_resm(TFAIL, "%s call failed - errno = %d : %s",
				 TCID, TEST_ERRNO, strerror(TEST_ERRNO));
		} else {
			/* get the semaphore ID */
			sem_id_1 = TEST_RETURN;

			if (STD_FUNCTIONAL_TEST) {
				check_functionality();	
			} else {
				tst_resm(TPASS, "semaphore was created");
			}
		}

		/*
		 * remove the semaphore that was created and mark the ID
		 * as invalid.
		 */
		if (sem_id_1 != -1) {
			rm_sema(sem_id_1);
			sem_id_1 = -1;
		}
	}

	cleanup();

	/*NOTREACHED*/
	return(0);
}

/*
 * check_functionality() - check the functionality of the tested system call.
 */
void
check_functionality(void)
{
	struct semid_ds semary;
	union semun un_arg;		/* union defined in ipcsem.h */

	/* STAT the semaphore */
	un_arg.buf = &semary;
	if (semctl(sem_id_1, 0, IPC_STAT, un_arg) == -1) {
		tst_brkm(TBROK, cleanup, "Could not stat the semaphore");
		return;
	}

	if (un_arg.buf->sem_nsems != PSEMS) {
		tst_resm(TFAIL, "# of semaphores in set != # given to create");
		return;
	}

	if (un_arg.buf->sem_perm.cuid != geteuid()) {
		tst_resm(TFAIL, "semaphore uid != process uid");
		return;
	}

	tst_resm(TPASS, "basic semaphore values are okay");
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
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

