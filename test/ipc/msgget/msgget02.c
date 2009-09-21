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
 *	msgget02.c
 *
 * DESCRIPTION
 *	msgget02 - test for EEXIST and ENOENT errors
 *
 * ALGORITHM
 *	create a message queue
 *	loop if that option was specified
 *	try to recreate the same queue - test #1
 *	try to access a queue that doesn't exist - tests #2 & #3
 *	check the errno value
 *	  issue a PASS message if we get EEXIST or ENOENT depening on test
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	  break any remaining tests
 *	  call cleanup
 *
 * USAGE:  <for command-line>
 *  msgget02 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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

#include "test.h"
#include "usctest.h"

#include "ipcmsg.h"

char *TCID = "msgget02";
int TST_TOTAL = 3;
extern int Tst_count;

struct test_case_t {
        int error;      
        int msg_incr;
        int flags;
} TC[] = {
        {EEXIST, 0, IPC_CREAT | IPC_EXCL},
        {ENOENT, 1, IPC_PRIVATE},
        {ENOENT, 1, IPC_EXCL}
};

int exp_enos[] = {EEXIST, ENOENT, 0};

int msg_q_1 = -1;		/* The message queue id created in setup */

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

		/* loop through the test cases */

		for (i=0; i<TST_TOTAL; i++) {

			TEST(msgget(msgkey + TC[i].msg_incr, TC[i].flags));

			if (TEST_RETURN != -1) {
				tst_resm(TFAIL, "msgget() call succeeded "
					 "on expected fail");
				continue;
			}

			TEST_ERROR_LOG(TEST_ERRNO);

			switch(TEST_ERRNO) {
			case ENOENT:
				/*FALLTHROUGH*/
			case EEXIST:
				if (TEST_ERRNO == TC[i].error) {
					tst_resm(TPASS, "expected failure - "
					 	 "errno = %d : %s", TEST_ERRNO,
					 	 strerror(TEST_ERRNO));
					break;
				}
				/*FALLTHROUGH*/
			default:
				tst_resm(TFAIL, "call failed with an "
					 "unexpected error - %d : %s",
					 TEST_ERRNO, strerror(TEST_ERRNO));
				break;
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

	msgkey = getipckey();

	/* now we have a key, so let's create a message queue */
	if ((msg_q_1 = msgget(msgkey, IPC_CREAT | IPC_EXCL)) == -1) {
		tst_brkm(TBROK, cleanup, "Can't create message queue" );
	}
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the message queue that was created. */
	rm_queue(msg_q_1);

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

