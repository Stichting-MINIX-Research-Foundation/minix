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
 *	msgget03.c
 *
 * DESCRIPTION
 *	msgget03 - test for an ENOSPC error by using up all available
 *		   message queues.
 *
 * ALGORITHM
 *	Get all the message queues that can be allocated
 *	loop if that option was specified
 *	Try to get one more message queue
 *	check the errno value
 *	  issue a PASS message if we get ENOSPC
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	  break any remaining tests
 *	  call cleanup
 *
 * USAGE:  <for command-line>
 *  msgget03 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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

char *TCID = "msgget03";
int TST_TOTAL = 1;
extern int Tst_count;

int maxmsgs = 0;

int exp_enos[] = {ENOSPC, 0};	/* 0 terminated list of expected errnos */

int *msg_q_arr = NULL;		/* hold the id's that we create */
int num_queue = 0;		/* count the queues created */

static int get_max_msgqueues();

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
		 * Try to create another message queue.  This should
		 * give us an ENOSPC error.
		 */

		TEST(msgget(msgkey + num_queue + 1, IPC_CREAT|IPC_EXCL));
	
		if (TEST_RETURN != -1) {
			tst_resm(TFAIL, "call succeeded when error expected");
			continue;
		}
	
		TEST_ERROR_LOG(TEST_ERRNO);

		switch(TEST_ERRNO) {
		case ENOSPC:
			tst_resm(TPASS, "expected failure - errno = %d : %s",
				 TEST_ERRNO, strerror(TEST_ERRNO));
			break;
		default:
			tst_resm(TFAIL, "call failed with an "
				 "unexpected error - %d : %s",
				 TEST_ERRNO, strerror(TEST_ERRNO));
			break;		
		}
	}

	cleanup();

	/*NOTREACHED*/
	return(0);
}

/** Get the max number of message queues allowed on system */
int get_max_msgqueues()
{
        FILE *f;
        char buff[512];

        /* Get the max number of message queues allowed on system */
        f = fopen("/proc/sys/kernel/msgmni", "r");
        if (!f){
                tst_brkm(TBROK, cleanup, "Could not open /proc/sys/kernel/msgmni");
        }
        if (!fgets(buff, 512, f)) {
                tst_brkm(TBROK, cleanup, "Could not read /proc/sys/kernel/msgmni");
        }
        fclose(f);
        return atoi(buff);
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
	int msg_q;

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

	maxmsgs = get_max_msgqueues();

	msg_q_arr = (int *)calloc(maxmsgs, sizeof (int));
	if (msg_q_arr == NULL) {
		tst_brkm(TBROK, cleanup, "Couldn't allocate memory "
				"for msg_q_arr: calloc() failed");
	}

	/*
	 * Use a while loop to create the maximum number of queues.
	 * When we get an error, check for ENOSPC.
	 */
	while((msg_q = msgget(msgkey + num_queue, IPC_CREAT|IPC_EXCL)) != -1) {
		msg_q_arr[num_queue] = msg_q;
		if (num_queue == maxmsgs) {
			tst_resm(TINFO, "The maximum number of message"
				 " queues (%d) has been reached", maxmsgs);
			break;
		}
		num_queue++;
	}

	/*
	 * if we have something other than ENOSPC, then something else is
	 * wrong.
	 */

	if (errno != ENOSPC) {
		tst_brkm(TBROK, cleanup, "Didn't get ENOSPC in test setup"
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
	int i;

	/*
	 * remove the message queues if they were created
	 */

	if (msg_q_arr != NULL) {
		for (i=0; i<num_queue; i++) {
			rm_queue(msg_q_arr[i]);
		}
		(void) free(msg_q_arr);
	}

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

