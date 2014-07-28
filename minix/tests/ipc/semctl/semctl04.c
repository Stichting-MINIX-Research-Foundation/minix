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
 *	semctl04.c
 *
 * DESCRIPTION
 *	semctl04 - test for EPERM error
 *
 * ALGORITHM
 *	create a semaphore set without read or alter permissions
 *	get the user id for "nobody"
 *	fork a child process
 *	if child
 *	  set the ID of the child process to that of "nobody"
 *	  loop if that option was specified
 *	    call semctl() with two different invalid cases	
 *	    check the errno value
 *	      issue a PASS message if we get EPERM
 *	    otherwise, the tests fails
 *	      issue a FAIL message
 *	  call cleanup
 *	if parent
 *	  wait for child to exit
 *	  remove the semaphore set
 *
 * USAGE:  <for command-line>
 *  semctl04 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
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
 *	test must be run as root
 */

#include "ipcsem.h"

#include <pwd.h>
#include <sys/wait.h>

char *TCID = "semctl04";
int TST_TOTAL = 2;
extern int Tst_count;

int exp_enos[] = {EPERM, 0};	/* 0 terminated list of expected errnos */

int sem_id_1 = -1;

uid_t ltp_uid;
char *ltp_user = "nobody";

int TC[] = {IPC_SET, IPC_RMID};

int main(int ac, char **av)
{
	char *msg;			/* message returned from parse_opts */
	pid_t pid;
	void do_child(void);

	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

	setup();			/* global setup */

	if ((pid = FORK_OR_VFORK()) == -1) {
		tst_brkm(TBROK, cleanup, "could not fork");
	}

	if (pid == 0) {		/* child */
		/* set the user ID of the child to the non root user */
		if (setuid(ltp_uid) == -1) {
			tst_resm(TBROK, "setuid() failed");
			exit(1);
		}

		do_child();

	} else {
		if (waitpid(pid, NULL, 0) == -1) {
			tst_resm(TBROK, "waitpid() failed");
			tst_resm(TINFO, "waitpid() error = %d : %s", errno,
				 strerror(errno));
		}

		/* if it exists, remove the semaphore resouce */
		rm_sema(sem_id_1);

		/* Remove the temporary directory */
		tst_rmdir();
	}
	cleanup();

	return(0);
}

/*
 * do_child() - make the TEST call as the child process
 */
void
do_child(void)
{
	int lc;				/* loop counter */
	int i;
	union semun arg;
	struct semid_ds perm; 

	/* The following loop checks looping state if -i option given */

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		/* reset Tst_count in case we are looping */
		Tst_count = 0;

		for (i=0; i<TST_TOTAL; i++) {

			if (TC[i] == IPC_SET)  {
				arg.buf =  &perm;
				memset(&perm, 0, sizeof perm); 
				perm.sem_perm.uid = getuid() + 1;
				perm.sem_perm.gid = getgid() + 1; 
				perm.sem_perm.mode = 0666; 
			}


			TEST(semctl(sem_id_1, 0, TC[i], arg));
	
			if (TEST_RETURN != -1) {
				tst_resm(TFAIL, "call succeeded unexpectedly");
				continue;
			}
	
			TEST_ERROR_LOG(TEST_ERRNO);

			switch(TEST_ERRNO) {
			case EPERM:
				tst_resm(TPASS, "expected failure - errno ="
					 " %d : %s", TEST_ERRNO,
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
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
	/* check for root as user id of process */
	check_root();

	/* capture signals */
	tst_sig(FORK, DEF_HANDLER, cleanup);

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

	/* create a semaphore set without read or alter permissions */
	if ((sem_id_1 = semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL)) == -1) {
		tst_brkm(TBROK, cleanup, "couldn't create semaphore in setup");
	}

	/* get the userid for a non root user */
	ltp_uid = getuserid(ltp_user);
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/*
	 * print timing stats if that option was specified.
	 * print errno log if that option was specified.
	 */
	TEST_CLEANUP;

	/* exit with return code appropriate for results */
	tst_exit();
}

