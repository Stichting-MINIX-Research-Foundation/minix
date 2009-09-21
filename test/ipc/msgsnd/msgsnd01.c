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
 *	msgsnd01.c
 *
 * DESCRIPTION
 *	msgsnd01 - test that msgsnd() enqueues a message correctly
 *
 * ALGORITHM
 *	create a message queue
 *	initialize a message buffer with a known message and type
 *	loop if that option was specified
 *	enqueue the message
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing
 *		stat the message queue
 *		check for # of bytes = MSGSIZE and # of messages = 1
 *	  	if correct,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  msgsnd01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
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
 *	None
 */

#include "test.h"
#include "usctest.h"

#include "ipcmsg.h"

void cleanup(void);
void setup(void);

char *TCID = "msgsnd01";
int TST_TOTAL = 1;
extern int Tst_count;

int msg_q_1;
MSGBUF msg_buf, rd_buf;

struct msqid_ds qs_buf;

int main(int ac, char **av)
{
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */

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
	
		TEST(msgsnd(msg_q_1, &msg_buf, MSGSIZE, 0));
	
		if (TEST_RETURN == -1) {
			tst_resm(TFAIL, "%s call failed - errno = %d : %s",
				 TCID, TEST_ERRNO, strerror(TEST_ERRNO));
			continue;
		}

		if (STD_FUNCTIONAL_TEST) {

			/* get the queue status */
			if (msgctl(msg_q_1, IPC_STAT, &qs_buf) == -1) {
				tst_brkm(TBROK, cleanup, "Could not "
					 "get queue status");
			}

			if (qs_buf.msg_cbytes != MSGSIZE) {
				tst_resm(TFAIL, "queue bytes != MSGSIZE");
			}

			if (qs_buf.msg_qnum != 1) {
				tst_resm(TFAIL, "queue message != 1");
			}

			tst_resm(TPASS, "queue bytes = MSGSIZE and "
				 "queue messages = 1");	
		} else {
			tst_resm(TPASS, "call succeeded");
		}

		/*
		 * remove the message by reading from the queue
		 */
		if (msgrcv(msg_q_1, &rd_buf, MSGSIZE, 1, 0) == -1) {
			tst_brkm(TBROK, cleanup, "Could not read from queue");
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

	msgkey = getipckey();

	/* create a message queue with read/write permissions */
	if ((msg_q_1 = msgget(msgkey, IPC_CREAT | IPC_EXCL | MSG_RW)) == -1) {
		tst_brkm(TBROK, cleanup, "Can't create message queue");
	}

	/* initialize the message buffer */
	init_buf(&msg_buf, MSGTYPE, MSGSIZE);
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the message queue if it exists */
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

