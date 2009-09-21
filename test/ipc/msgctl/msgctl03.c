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
 *	msgctl03.c
 *
 * DESCRIPTION
 *	msgctl03 - create a message queue, then issue the IPC_RMID command
 *
 * ALGORITHM
 *	create a message queue
 *	loop if that option was specified
 *	call msgctl() with the IPC_RMID command
 *	check the return code
 *	  if failure, issue a FAIL message and break remaining tests
 *	otherwise,
 *	  if doing functionality testing
 *		issue an IPC_STAT on the queue that was just removed
 *	  	if the call fails
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	  else issue a PASS message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  msgctl03 [-c n] [-f] [-I x] [-P x] [-t]
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
 *	This test does not support looping.
 */

#include "test.h"
#include "usctest.h"

#include "ipcmsg.h"

char *TCID = "msgctl03";
int TST_TOTAL = 1;
extern int Tst_count;

int msg_q_1 = -1;                      /* to hold the message queue id */

struct msqid_ds qs_buf;

int main(int ac, char **av)
{
	char *msg;			/* message returned from parse_opts */

	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

	setup();			/* global setup */

	/*
	 * Remove the queue that was created in setup()
	 */

	TEST(msgctl(msg_q_1, IPC_RMID, NULL));
	
	if (TEST_RETURN == -1) {
		tst_brkm(TFAIL, cleanup, "%s call failed - errno = %d"
			 " : %s", TCID, TEST_ERRNO, strerror(TEST_ERRNO));
	} else {
		if (STD_FUNCTIONAL_TEST) {
			/*
			 * if the queue is gone, then an IPC_STAT msgctl()
			 * call should generate an EINVAL error.
			 */
			if ((msgctl(msg_q_1, IPC_STAT, &qs_buf) == -1)){
				if (errno == EINVAL) {	
					tst_resm(TPASS, "The queue is gone");
				} else {
					tst_resm(TFAIL, "IPC_RMID succeeded ,"
						 " but functional test did not"
						 " get expected EINVAL error");
				}
			}
		} else {
			tst_resm(TPASS, "msgctl() call succeeded");
		}
	}

	msg_q_1 = -1;

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

	/* get a message key */
	msgkey = getipckey();

	/* now we have a key, so let's create a message queue */
	if ((msg_q_1 = msgget(msgkey, IPC_CREAT | IPC_EXCL | MSG_RW)) == -1) {
		tst_brkm(TBROK, cleanup, "Can't create message queue");
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

