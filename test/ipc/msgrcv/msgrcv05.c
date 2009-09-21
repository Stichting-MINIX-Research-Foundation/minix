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
 *	msgrcv05.c
 *
 * DESCRIPTION
 *	msgrcv05 - test for EINTR error
 *
 * ALGORITHM
 *	create a message queue with read/write permissions
 *	loop if that option was specified
 *	fork a child who attempts to read a non-existent message with msgrcv()
 *	parent sends a SIGHUP to the child, then waits for the child to complete
 *	check the errno value
 *	  issue a PASS message if we get EINTR
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	child exits, parent calls cleanup
 *
 * USAGE:  <for command-line>
 *  msgrcv05 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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

#include <sys/types.h>
#include <sys/wait.h>

void do_child(void);
void cleanup(void);
void setup(void);
void sighandler(int);
#ifdef UCLINUX
void do_child_uclinux(void);
#endif

char *TCID = "msgrcv05";
int TST_TOTAL = 1;
extern int Tst_count;

int exp_enos[] = {EINTR, 0};	/* 0 terminated list of expected errnos */

int msg_q_1 = -1;		/* The message queue id created in setup */

MSGBUF rcv_buf;
pid_t c_pid;

int main(int ac, char **av)
{
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */

	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

#ifdef UCLINUX
	maybe_run_child(&do_child_uclinux, "d", &msg_q_1);
#endif

	setup();			/* global setup */

	/* The following loop checks looping state if -i option given */

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		/* reset Tst_count in case we are looping */
		Tst_count = 0;

		/*
		 * fork a child that will attempt to read a non-existent
		 * message from the queue
		 */
		if ((c_pid = FORK_OR_VFORK()) == -1) {
			tst_brkm(TBROK, cleanup, "could not fork");
		}

		if (c_pid == 0) {		/* child */
			/*
			 * Attempt to read a message without IPC_NOWAIT.
			 * With no message to read, the child sleeps.
			 */
#ifdef UCLINUX
			if (self_exec(av[0], "d", msg_q_1) < 0) {
				tst_brkm(TBROK, cleanup, "could not self_exec");
			}
#else
			do_child();
#endif
		} else {			/* parent */
			usleep(250000);

			/* send a signal that must be caught to the child */
			if (kill(c_pid, SIGHUP) == -1) {
				tst_brkm(TBROK, cleanup, "kill failed");
			}

			waitpid(c_pid, NULL, 0);
		}
	}

	cleanup();

	/*NOTREACHED*/
	return(0);
}

/*
 * do_child()
 */
void
do_child()
{
	TEST(msgrcv(msg_q_1, &rcv_buf, MSGSIZE, 1, 0));

	if (TEST_RETURN != -1) {
		tst_resm(TFAIL, "call succeeded when error expected");
		exit(-1);
	}
	
	TEST_ERROR_LOG(TEST_ERRNO);
	
	switch(TEST_ERRNO) {
	case EINTR:
		tst_resm(TPASS, "expected failure - errno = %d : %s", TEST_ERRNO,
			 strerror(TEST_ERRNO));
		break;
	default:
		tst_resm(TFAIL, "call failed with an unexpected error - %d : %s",
			 TEST_ERRNO, strerror(TEST_ERRNO));
		break;
	}			

	exit(0);
}

#ifdef UCLINUX
/*
 * do_child_uclinux() - capture signals again, then run do_child()
 */
void
do_child_uclinux()
{
	tst_sig(FORK, sighandler, cleanup);

	do_child();
}
#endif

/*
 * sighandler() - handle signals
 */
void
sighandler(int sig)
{
	/* we don't need to do anything here */
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
	/* capture signals */
	tst_sig(FORK, sighandler, cleanup);

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
