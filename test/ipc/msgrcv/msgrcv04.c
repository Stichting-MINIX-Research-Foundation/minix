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
 *	msgrcv04.c
 *
 * DESCRIPTION
 *	msgrcv04 - test for E2BIG and ENOMSG errors
 *
 * ALGORITHM
 *	create a message queue with read/write permissions
 *	initialize a message buffer with a known message and type
 *	enqueue the message
 *	loop if that option was specified
 *	call msgrcv() using two different invalid cases	
 *	check the errno value
 *	  issue a PASS message if we get E2BIG or ENOMSG
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  msgrcv04 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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

void cleanup(void);
void setup(void);

char *TCID = "msgrcv04";
int TST_TOTAL = 2;
extern int Tst_count;

int exp_enos[] = {E2BIG, ENOMSG, 0};

int msg_q_1 = -1;		/* The message queue id created in setup */

#define SMSIZE	512

MSGBUF snd_buf, rcv_buf;

struct test_case_t {
	int size;
	int type;
	int flags;
	int error;
} TC[] = {
	/*
	 * E2BIG - The receive buffer is too small for the message and
	 *	   MSG_NOERROR isn't asserted in the flags.
	 */
	{SMSIZE, 1, 0, E2BIG},

	/*
	 * ENOMSG - There is no message with the requested type and
	 *	    IPC_NOWAIT is asserted in the flags.
	 */
	{MSGSIZE, 2, IPC_NOWAIT, ENOMSG}
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

			/*
			 * Use the TEST macro to make the call
			 */
	
			TEST(msgrcv(msg_q_1, &rcv_buf, TC[i].size, TC[i].type,
			     TC[i].flags));
	
			if (TEST_RETURN != -1) {
				tst_resm(TFAIL, "call succeeded unexpectedly");
				continue;
			}
	
			TEST_ERROR_LOG(TEST_ERRNO);

			if (TEST_ERRNO == TC[i].error) {
				tst_resm(TPASS, "expected failure - errno = "
					 "%d : %s", TEST_ERRNO,
					 strerror(TEST_ERRNO));
			} else {
				tst_resm(TFAIL, "call failed with an "
					 "unexpected error - %d : %s",
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

	/* create a message queue with read/write permission */
	if ((msg_q_1 = msgget(msgkey, IPC_CREAT | IPC_EXCL | MSG_RW)) == -1) {
		tst_brkm(TBROK, cleanup, "Can't create message queue");
	}

	/* initialize a buffer */
	init_buf(&snd_buf, MSGTYPE, MSGSIZE);

	/* put the message on the queue */
	if (msgsnd(msg_q_1, &snd_buf, MSGSIZE, 0) == -1) {
		tst_brkm(TBROK, cleanup, "Can't enqueue message");
	}
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the message queue that was created */
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

