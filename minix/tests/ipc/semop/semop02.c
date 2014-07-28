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
 *	semop02.c
 *
 * DESCRIPTION
 *	semop02 - test for E2BIG, EACCES, EFAULT and EINVAL errors
 *
 * ALGORITHM
 *	create a semaphore set with read and alter permissions
 *	create a semaphore set without read and alter permissions
 *	loop if that option was specified
 *	call semop with five different invalid cases
 *	check the errno value
 *	  issue a PASS message if we get E2BIG, EACCES, EFAULT or EINVAL
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  semop02 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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

#include "ipcsem.h"

char *TCID = "semop02";
int TST_TOTAL = 4;
extern int Tst_count;

int exp_enos[] = {E2BIG, EACCES, EFAULT, EINVAL, 0};

int sem_id_1 = -1;	/* a semaphore set with read & alter permissions */
int sem_id_2 = -1;	/* a semaphore set without read & alter permissions */
int bad_id = -1;
char nobody_uid[] = "nobody";
struct passwd *ltpuser;

struct sembuf s_buf[PSEMS];

int badbuf = -1;

#define NSOPS	5		/* a resonable number of operations */
#define	BIGOPS	1024		/* a value that is too large for the number */
				/* of semop operations that are permitted   */

struct test_case_t {
	int *semid;		/* the semaphore id */
	struct sembuf *t_sbuf;	/* the first in an array of sembuf structures */
	unsigned t_ops;		/* the number of elements in the above array */
	int error;		/* the expected error number */
} TC[] = {
	/* E2BIG - the number of operations is too big */
	{&sem_id_1, (struct sembuf *)&s_buf, BIGOPS, E2BIG},

	/* EACCES - the semaphore set has no access permission */
	{&sem_id_2, (struct sembuf *)&s_buf, NSOPS, EACCES},

	/* EINVAL - the number of elments (t_ops) is 0 */
	{&sem_id_1, (struct sembuf *)&s_buf, 0, EINVAL},

	/* EINVAL - the semaphore set doesn't exist */
	{&bad_id, (struct sembuf *)&s_buf, NSOPS, EINVAL}
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
			 * use the TEST macro to make the call
			 */
	
			TEST(semop(*(TC[i].semid), TC[i].t_sbuf, TC[i].t_ops));

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
				tst_resm(TFAIL, "unexpected error - "
					 "%d : %s", TEST_ERRNO,
					 strerror(TEST_ERRNO));
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
	/* Switch to nobody user for correct error code collection */
        if (geteuid() != 0) {
                tst_brkm(TBROK, tst_exit, "Test must be run as root");
        }

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

	ltpuser = getpwnam(nobody_uid);
	if (seteuid(ltpuser->pw_uid) == -1) {
		tst_resm(TINFO, "setreuid failed to "
				"to set the effective uid to %d",
				ltpuser->pw_uid);
		perror("setreuid");
	}

	/* get an IPC resource key */
	semkey = getipckey();

	/* create a semaphore set with read and alter permissions */
	if ((sem_id_1 =
	     semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL | SEM_RA)) == -1) {
		tst_brkm(TBROK, cleanup, "couldn't create semaphore in setup");
	}

	/* increment the semkey */
	semkey += 1;
	
	/* create a semaphore set without read and alter permissions */
	if ((sem_id_2 = semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL)) == -1) {
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
	/* if they exist, remove the semaphore resources */
	rm_sema(sem_id_1);
	rm_sema(sem_id_2);

	/* Remove the temporary directory */
	seteuid(getuid());
	tst_rmdir();

	/*
	 * print timing stats if that option was specified.
	 * print errno log if that option was specified.
	 */
	TEST_CLEANUP;

	/* exit with return code appropriate for results */
	tst_exit();
}

