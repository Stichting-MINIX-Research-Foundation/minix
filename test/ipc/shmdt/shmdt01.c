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
 *	shmdt01.c
 *
 * DESCRIPTION
 *	shmdt01 - check that shared memory is detached correctly
 *
 * ALGORITHM
 *	create a shared memory resource of size sizeof(int)
 *	attach it to the current process and give it a value
 *	call shmdt() using the TEST macro	
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing
 *		attempt to write a value to the shared memory address
 *		this should generate a SIGSEGV which will be caught in
 *		    the signal handler
 *	  	if correct,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  shmdt01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
 *     where,  -c n : Run n copies concurrently.
 *             -f   : Turn off functionality Testing.
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

#include <setjmp.h>
#include "ipcshm.h"

char *TCID = "shmdt01";
int TST_TOTAL = 1;
extern int Tst_count;

void sighandler(int);
struct shmid_ds buf;

int shm_id_1 = -1;
int *shared;		/* variable to use for shared memory attach */
int new;
int pass = 0;
sigjmp_buf env;

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
		 * Use TEST macro to make the shmdt() call
		 */

		TEST(shmdt((const void *)shared));

		if (TEST_RETURN == -1) {
			tst_resm(TFAIL, "%s call failed - errno = %d : %s",
				 TCID, TEST_ERRNO, strerror(TEST_ERRNO));
		} else {
			if (STD_FUNCTIONAL_TEST) {
				check_functionality();
			} else {
				tst_resm(TPASS, "call succeeded");
			}
		}

		/* reattach the shared memory segment in case we are looping */
		shared = (int *)shmat(shm_id_1, 0, 0);

		if (*shared == -1) {
			tst_brkm(TBROK, cleanup, "memory reattach failed");
		}

		/* also reset pass */
		pass = 0;
	}

	cleanup();

	/*NOTREACHED*/
	return(0);
}

/*
 * check_functionality() - make sure the memory is detached correctly
 */
void
check_functionality(void)
{
	/* stat the shared memory segment */
	if (shmctl(shm_id_1, IPC_STAT, &buf) == -1) {
		tst_resm(TINFO, "error = %d : %s", errno, strerror(errno));
		tst_brkm(TBROK, cleanup, "could not stat in signal handler");
	}

	if (buf.shm_nattch != 0) {
		tst_resm(TFAIL, "# of attaches is incorrect");
		return;
	}

	/*
	 * Try writing to the shared memory.  This should generate a
	 * SIGSEGV which will be caught below.
	 *
	 * This is wrapped by the sigsetjmp() call that will take care of
	 * restoring the program's context in an elegant way in conjunction
	 * with the call to siglongjmp() in the signal handler.
	 *
	 * An attempt to do the assignment without using the sigsetjmp()
	 * and siglongjmp() calls will result in an infinite loop.  Program 
	 * control is returned to the assignment statement after the execution
	 * of the signal handler and another SIGSEGV will be generated.
	 */

	if (sigsetjmp(env, 1) == 0) {
		*shared = 2;
	}

	if (pass) {
		tst_resm(TPASS, "shared memory detached correctly");
	} else {
		tst_resm(TFAIL, "shared memory was not detached correctly");
	}
}
 
/*
 * sighandler()
 */
void
sighandler(sig)
{
	/* if we have received a SIGSEGV, we are almost done */
	if (sig == SIGSEGV) {
		/* set the global variable and jump back */
		pass = 1;
		siglongjmp(env, 1);
	} else {
		tst_brkm(TBROK, cleanup, "received an unexpected signal");
	}
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
	/* capture signals */

	tst_sig(NOFORK, sighandler, cleanup);

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

	/* create a shared memory resource with read and write permissions */
	if ((shm_id_1 = shmget(shmkey, INT_SIZE, SHM_RW | IPC_CREAT |
	     IPC_EXCL)) == -1) {
		tst_brkm(TBROK, cleanup, "Failed to create shared memory "
			 "resource in setup()");
	}

	/* attach the shared memory segment */
	shared = (int *)shmat(shm_id_1, 0, 0);

	if (*shared == -1) {
		tst_brkm(TBROK, cleanup, "Couldn't attach shared memory");
	}

	/* give a value to the shared memory integer */
	*shared = 4;
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, delete the shared memory resource */
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

