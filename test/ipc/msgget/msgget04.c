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
 *	msgget04.c
 *
 * DESCRIPTION
 *	msgget04 - test for an EACCES error by creating a message queue
 *		   with no read or write permission and then attempting
 *		   to access it with various permissions.
 *
 * ALGORITHM
 *	Create a message queue with no read or write permission
 *	loop if that option was specified
 *	Try to access the message queue with various permissions
 *	check the errno value
 *	  issue a PASS message if we get EACCES
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	  break any remaining tests
 *	  call cleanup
 *
 * USAGE:  <for command-line>
 *  msgget04 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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
#include "test.h"
#include "usctest.h"

#include "ipcmsg.h"

char *TCID = "msgget04";
int TST_TOTAL = 3;
extern int Tst_count;

char nobody_uid[] = "nobody";
struct passwd *ltpuser;


int exp_enos[] = {EACCES, 0};	/* 0 terminated list of expected errnos */

int msg_q_1 = -1;		/* to hold the message queue id */

int test_flags[] = {MSG_RD, MSG_WR, MSG_RD | MSG_WR};

int main(int ac, char **av)
{
	int lc;			/* loop counter */
	char *msg;		/* message returned from parse_opts */
	int i;			/* a counter */

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
			/*
			 * Try to access the message queue with specified
			 * permissions.
			 */

			TEST(msgget(msgkey, test_flags[i]));
	
			if (TEST_RETURN != -1) {
				tst_resm(TFAIL, "call succeeded "
					 "when EACCES error expected");
				continue;
			}
	
			TEST_ERROR_LOG(TEST_ERRNO);

			switch(TEST_ERRNO) {
			case EACCES:
				tst_resm(TPASS, "expected failure - errno = "
					 "%d : %s", TEST_ERRNO,
					 strerror(TEST_ERRNO));
				break;
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

	 /* Switch to nobody user for correct error code collection */
        if (geteuid() != 0) {
                tst_brkm(TBROK, tst_exit, "Test must be run as root");
        }
        ltpuser = getpwnam(nobody_uid);
        if (setuid(ltpuser->pw_uid) == -1) {
                tst_resm(TINFO, "setuid failed to "
                         "to set the effective uid to %d",
                         ltpuser->pw_uid);
                perror("setuid");
        }


	/*
	 * Create a temporary directory and cd into it.
	 * This helps to ensure that a unique msgkey is created.
	 * See ../lib/libipc.c for more information.
	 */
	tst_tmpdir();

	msgkey = getipckey();

	/*
	 * Create the message queue without specifying permissions.
	 */
	if ((msg_q_1 = msgget(msgkey, IPC_CREAT|IPC_EXCL)) == -1) {
		tst_brkm(TBROK, cleanup, "Could not create message queue"
			 " - errno = %d : %s", errno, strerror(errno));
	}
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the message queue */
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

