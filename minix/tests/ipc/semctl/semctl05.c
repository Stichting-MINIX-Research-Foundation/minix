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
 *	semctl05.c
 *
 * DESCRIPTION
 *	semctl05 - test for ERANGE error
 *
 * ALGORITHM
 *	create a semaphore set with read and alter permissions
 *	loop if that option was specified
 *	call semctl() with three different invalid cases
 *	check the errno value
 *	  issue a PASS message if we get ERANGE
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  semctl05 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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

#include "ipcsem.h"

char *TCID = "semctl05";
int TST_TOTAL = 3;
extern int Tst_count;

#ifdef _XLC_COMPILER
#define SEMUN_CAST 
#else
#define SEMUN_CAST (union semun)
#endif

int exp_enos[] = {ERANGE, 0};	/* 0 terminated list of expected errnos */

int sem_id_1 = -1;

#define BIGV	65535		/* a number ((2^16)-1) that should be larger */
				/* than the maximum for a semaphore value    */ 

#ifdef _XLC_COMPILER
#define SEMUN_CAST
#else
#define SEMUN_CAST (union semun)
#endif


unsigned short big_arr[] = {BIGV, BIGV, BIGV, BIGV, BIGV, BIGV, BIGV, BIGV,
			    BIGV, BIGV};

struct test_case_t {
	int count;
	int cmd;
	union semun t_arg;
} TC[3];

void setup_test_case(void)
{
	/* ERANGE - the value to set is less than zero - SETVAL */
	/* {5, SETVAL, SEMUN_CAST -1}, */
    TC[0].count = 5;
    TC[0].cmd = SETVAL;
    TC[0].t_arg.val = -1;
	
	/* ERANGE - the values to set are too large, > semaphore max value */
	/* {0, SETALL, SEMUN_CAST big_arr}, */
    TC[1].count = 0;
    TC[1].cmd = SETALL;
    TC[1].t_arg.array = big_arr;

	/* ERANGE - the value to set is too large, > semaphore max value */
	/* {5, SETVAL, SEMUN_CAST BIGV} */
    TC[2].count = 5;
    TC[2].cmd = SETVAL;
    TC[2].t_arg.val = BIGV;
}

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

			TEST(semctl(sem_id_1, TC[i].count,
				    TC[i].cmd, TC[i].t_arg));
	
			if (TEST_RETURN != -1) {
				tst_resm(TFAIL, "call succeeded unexpectedly");
				continue;
			}
	
			TEST_ERROR_LOG(TEST_ERRNO);

			switch(TEST_ERRNO) {
			case ERANGE:
				tst_resm(TPASS, "expected failure - errno = "
					 "%d : %s", TEST_ERRNO,
					 strerror(TEST_ERRNO));
				break;
			default:
				tst_resm(TFAIL, "unexpected error "
					 "- %d : %s", TEST_ERRNO,
					 strerror(TEST_ERRNO));
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
    setup_test_case();

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

	/* get an IPC resource key */
	semkey = getipckey();

	/* create a semaphore set with read and alter permissions */
	if ((sem_id_1 =
	     semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL | SEM_RA)) == -1) {
		tst_brkm(TBROK, cleanup, "couldn't create semaphore in setup");
	}
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the semaphore resouce */
	rm_sema(sem_id_1);

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

