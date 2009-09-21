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
 *	msgget01.c
 *
 * DESCRIPTION
 *	msgget01 - create a message queue, write a message to it and
 *		   read it back.
 *
 * ALGORITHM
 *	loop if that option was specified
 *	create a message queue
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing by writting a message to the queue,
 *	  reading it back and comparing the two.
 *	  	if the messages are the same,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  msgget01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
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

#include "ipcmsg.h"

#include <string.h>

char *TCID = "msgget01";
int TST_TOTAL = 1;
extern int Tst_count;

int msg_q_1 = -1;		/* to hold the message queue ID */

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
		 * Use TEST macro to make the call to create the message queue
		 */
	
		TEST(msgget(msgkey, IPC_CREAT | IPC_EXCL | MSG_RD | MSG_WR));
	
		if (TEST_RETURN == -1) {
			tst_resm(TFAIL, "%s call failed - errno = %d : %s",
				 TCID, TEST_ERRNO, strerror(TEST_ERRNO));
		} else {
			msg_q_1 = TEST_RETURN;
			if (STD_FUNCTIONAL_TEST) {
				/*
				 * write a message to the queue.
				 * read back the message.
				 * PASS the test if they are the same.
				 */
				check_functionality();
			} else {
				tst_resm(TPASS, "message queue was created");
			}
		}

		/*
 		 * remove the message queue that was created and mark the ID
		 * as invalid.
		 */
		if (msg_q_1 != -1) {
			rm_queue(msg_q_1);
			msg_q_1 = -1;
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
check_functionality()
{
	int i=0;
	MSGBUF snd_buf, rcv_buf;

	/* EAGLE: Houston, Tranquility Base here. The Eagle has landed! */
	char *queue_msg =
		 "Qston, check_functionality here.  The message has queued!";

	/*
	 * copy our message into the buffer and then set the type.
	 */
	do {
		snd_buf.mtext[i++] = *queue_msg;
	} while(*queue_msg++ != (char)NULL);

	snd_buf.mtype = MSGTYPE;

	/* send the message */
	if(msgsnd(msg_q_1, &snd_buf, MSGSIZE, 0) == -1) {
		tst_brkm(TBROK, cleanup, "Could not send a message in the "
			 "check_functionality() routine.");
	}

	/* receive the message */
	if(msgrcv(msg_q_1, &rcv_buf, MSGSIZE, MSGTYPE, IPC_NOWAIT) == -1) {
		tst_brkm(TBROK, cleanup, "Could not read a messages in the "
			 "check_functionality() routine.");
	}

	if(strcmp(snd_buf.mtext, rcv_buf.mtext) == 0) {
		tst_resm(TPASS, "message received = message sent");
	} else {
		tst_resm(TFAIL, "message received != message sent");
	}
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

