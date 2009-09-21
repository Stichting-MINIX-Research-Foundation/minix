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
 *	shmget01.c
 *
 * DESCRIPTION
 *	shmget01 - test that shmget() correctly creates a shared memory segment
 *
 * ALGORITHM
 *	loop if that option was specified
 *	use the TEST() macro to call shmget()
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing
 *		stat the shared memory resource
 *		check the size, creator pid and mode
 *	  	if correct,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	  else issue a PASS message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  shmget01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
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

#include "ipcshm.h"

char *TCID = "shmget01";
int TST_TOTAL = 1;
extern int Tst_count;

int shm_id_1 = -1;

int main(int ac, char **av)
{
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */
	struct shmid_ds buf;

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
	
		TEST(shmget(shmkey, SHM_SIZE, (IPC_CREAT | IPC_EXCL | SHM_RW)));
	
		if (TEST_RETURN == -1) {
			tst_resm(TFAIL, "%s call failed - errno = %d : %s",
				 TCID, TEST_ERRNO, strerror(TEST_ERRNO));
		} else {
			shm_id_1 = TEST_RETURN;
			if (STD_FUNCTIONAL_TEST) {
				/* do a STAT and check some info */
				if (shmctl(shm_id_1, IPC_STAT, &buf) == -1) {
					tst_resm(TBROK, "shmctl failed in "
						 "functional test");
					continue;
				}
				/* check the seqment size */
				if (buf.shm_segsz != SHM_SIZE) {
					tst_resm(TFAIL, "seqment size is not "
						 "correct");
					continue;
				}
				/* check the pid of the creator */
				if (buf.shm_cpid != getpid()) {
					tst_resm(TFAIL, "creator pid is not "
						 "correct");
					continue;
				}
				/*
				 * check the mode of the seqment
				 * mask out all but the lower 9 bits
				 */
				if ((buf.shm_perm.mode & MODE_MASK) !=
				    ((SHM_RW) & MODE_MASK)) {
					tst_resm(TFAIL, "segment mode is not "
						 "correct");
					continue;
				}
				/* if we get here, everything looks good */
				tst_resm(TPASS, "size, pid & mode are correct");
			} else {
				tst_resm(TPASS, "call succeeded");
			}
		}

		/*
		 * clean up things in case we are looping
		 */
		if (shmctl(shm_id_1, IPC_RMID, NULL) == -1) {
			tst_resm(TBROK, "couldn't remove shared memory");
		} else {
			shm_id_1 = -1;
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
	shmkey = getipckey();
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the shared memory resource */
	rm_shm(shm_id_1);

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

