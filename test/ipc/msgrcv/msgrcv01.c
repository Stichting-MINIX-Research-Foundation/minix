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
 *	msgrcv01.c
 *
 * DESCRIPTION
 *	msgrcv01 - test that msgrcv() receives the expected message
 *
 * ALGORITHM
 *	create a message queue
 *	initialize a message buffer with a known message and type
 *	loop if that option was specified
 *	fork a child to receive the message
 *	parent enqueues the message then exits
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing
 *		build a new message and compare it to the one received
 *	  	if they are the same,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  msgrcv01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
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

#include <string.h>
#include <sys/wait.h>

#include "test.h"
#include "usctest.h"

#include "ipcmsg.h"

void cleanup(void);
void setup(void);
void do_child(void);

char *TCID = "msgrcv01";
int TST_TOTAL = 1;
extern int Tst_count;

int msg_q_1;
MSGBUF snd_buf, rcv_buf, cmp_buf;

pid_t c_pid;

int main(int ac, char **av)
{
    int lc;			/* loop counter */
    char *msg;			/* message returned from parse_opts */
    void check_functionality(void);
    int status, e_code;

    /* parse standard options */
    if ((msg =
	 parse_opts(ac, av, (option_t *) NULL, NULL)) != (char *) NULL) {
	tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
    }

#ifdef UCLINUX
    maybe_run_child(&do_child, "d", &msg_q_1);
#endif

    setup();			/* global setup */

    /* The following loop checks looping state if -i option given */

    for (lc = 0; TEST_LOOPING(lc); lc++) {
	/* reset Tst_count in case we are looping */
	Tst_count = 0;

	/*
	 * fork a child to read from the queue while the parent
	 * enqueues the message to be read.
	 */
	if ((c_pid = FORK_OR_VFORK()) == -1) {
	    tst_brkm(TBROK, cleanup, "could not fork");
	}

	if (c_pid == 0) {	/* child */
#ifdef UCLINUX
	    if (self_exec(av[0], "d", msg_q_1) < 0) {
		tst_brkm(TBROK, cleanup, "could not self_exec");
	    }
#else
	    do_child();
#endif
	} else {		/* parent */
	    /* put the message on the queue */
	    if (msgsnd(msg_q_1, &snd_buf, MSGSIZE, 0) == -1) {
		tst_brkm(TBROK, cleanup, "Couldn't enqueue" " message");
	    }
	    /* wait for the child to finish */
	    wait(&status);
	    /* make sure the child returned a good exit status */
	    e_code = status >> 8;
	    if (e_code != 0) {
		tst_resm(TFAIL, "Failures reported above");
	    }

	}
    }

    cleanup();

    /** NOT REACHED **/
    return(0);

}

/*
 * do_child()
 */
void
do_child()
{
    int retval = 0;

    TEST(msgrcv(msg_q_1, &rcv_buf, MSGSIZE, 1, 0));
    
    if (TEST_RETURN == -1) {
	retval = 1;
	tst_resm(TFAIL, "%s call failed - errno = %d : %s",
		 TCID, TEST_ERRNO, strerror(TEST_ERRNO));
    } else {
	if (STD_FUNCTIONAL_TEST) {
	    /*
	     * Build a new message and compare it
	     * with the one received.
	     */
	    init_buf(&cmp_buf, MSGTYPE, MSGSIZE);
	    
	    if (strcmp(rcv_buf.mtext, cmp_buf.mtext) == 0) {
		tst_resm(TPASS,
			 "message received = " "message sent");
	    } else {
		retval = 1;
		tst_resm(TFAIL,
			 "message received != " "message sent");
	    }
	} else {
	    tst_resm(TPASS, "call succeeded");
	}
    }
    exit(retval);
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void setup(void)
{
    /* capture signals */
    tst_sig(FORK, DEF_HANDLER, cleanup);

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

    /* initialize the message buffer */
    init_buf(&snd_buf, MSGTYPE, MSGSIZE);
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void cleanup(void)
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
