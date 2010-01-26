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
 *	semget02.c
 *
 * DESCRIPTION
 *	semget02 - test for EACCES and EEXIST errors
 *
 * ALGORITHM
 *	create a semaphore set without read or alter permissions
 *	loop if that option was specified
 *	call semget() using two different invalid cases	
 *	check the errno value
 *	  issue a PASS message if we get EACCES or EEXIST
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  semget02 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
 *     where,  -c n : Run n copies concurrently.
 *             -e   : Turn on errno logging.
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
#include <pwd.h>

#include "../lib/ipcsem.h"

char *TCID = "semget02";
int TST_TOTAL = 2;
extern int Tst_count;

int exp_enos[] = {EACCES, EEXIST, 0};

char nobody_uid[] = "nobody";
struct passwd *ltpuser;


int sem_id_1 = -1;

struct test_case_t {
	int flags;
	int error;
} TC [] = {
	/* EACCES - the semaphore has no read or alter permissions */
	{SEM_RA, EACCES},

	/* EEXIST - the semaphore id exists and semget() was called with  */
	/* IPC_CREAT and IPC_EXCL  					  */
	{IPC_CREAT | IPC_EXCL, EEXIST}
};

int main(int ac, char **av)
{
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */
	int i;

	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

	setup();			/* global setup */

	/* The following loop checks looping state if -i option given */

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		/* reset Tst_count in case we are looping */
		Tst_count = 0;

		for (i=0; i<TST_TOTAL; i++) {
			/* use the TEST macro to make the call */
	
			TEST(semget(semkey, PSEMS, TC[i].flags));

			if (TEST_RETURN != -1) {
				sem_id_1 = TEST_RETURN;
				tst_resm(TFAIL, "call succeeded");
				continue;
			}
	
			TEST_ERROR_LOG(TEST_ERRNO);

			if (TEST_ERRNO == TC[i].error) {
				tst_resm(TPASS, "expected failure - errno "
					 "= %d : %s", TEST_ERRNO,
					 strerror(TEST_ERRNO));
			} else {
				tst_resm(TFAIL, "unexpected error - %d : %s",
					 TEST_ERRNO, strerror(TEST_ERRNO));
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
	/* Switch to nobody user for correct error code collection */
	if (geteuid() != 0) {
                tst_brkm(TBROK, tst_exit, "Test must be run as root");
        }
	
	/* capture signals */
	tst_sig(NOFORK, DEF_HANDLER, cleanup);

	/* Set up the expected error numbers for -e option */
	TEST_EXP_ENOS(exp_enos);

	/* Pause if that option was specified */
	TEST_PAUSE;

	/*
	 * Create a temporary directory and cd into it.
	 * This helps to ensure that a unique msgkey is created.
	 * See ../lib/libipc.c for more information.
	 */
	tst_tmpdir();

	 ltpuser = getpwnam(nobody_uid);
         if (seteuid(ltpuser->pw_uid) == -1) {
	 	tst_resm(TINFO, "setreuid failed to "
	                 "to set the effective uid to %d",
	                 ltpuser->pw_uid);
	        perror("setreuid");
	 }
			
	
	/* get an IPC resource key */
	semkey = getipckey();

	/* create a semaphore set without read or alter permissions */
	if ((sem_id_1 = semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL)) == -1) {
		tst_brkm(TBROK, cleanup, "couldn't create semaphore in setup");
	}
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the semaphore resource */
	rm_sema(sem_id_1);

	/* Remove the temporary directory */
	seteuid(getuid());
	tst_rmdir();

	/*
	 * print timing stats if that option was specified.
	 * print errno log if that option was specified.
	 */
	TEST_CLEANUP;

	/* exit with return code appropriate for results */
	tst_exit();
}

