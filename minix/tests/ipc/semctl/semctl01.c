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
 *	semctl01.c
 *
 * DESCRIPTION
 *	semctl01 - test the 10 possible semctl() commands
 *
 * ALGORITHM
 *	create a semaphore set with read and alter permissions
 *	loop if that option was specified
 *	  loop through the test cases
 *	    do any setup required for the test case
 *	    make the semctl() call using the TEST() macro
 *	    check the return code
 *	      if failure, issue a FAIL message.
 *	    otherwise,
 *	      if doing functionality testing
 *		call the appropriate test function
 *	  	if correct,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  semctl01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
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

#include "ipcsem.h"

char *TCID = "semctl01";
int TST_TOTAL = 10;
extern int Tst_count;

int sem_id_1 = -1;	/* a semaphore set with read and alter permissions */

/*
 * These are the various setup and check functions for the 10 different
 * commands that are available for the semctl() call.
 */
void func_stat(int);
void set_setup(int), func_set(int);
void func_gall(int);
void cnt_setup(int), func_cnt(int);
void pid_setup(int), func_pid(int);
void gval_setup(int), func_gval(int);
void sall_setup(int), func_sall(int);
void func_sval(int);
void func_rmid(int);
void child_cnt(void);
void child_pid(void);

struct semid_ds buf;
unsigned short array[PSEMS];
struct sembuf sops;

#define INCVAL 2	/* a semaphore increment value */
#define NEWMODE	066
#define NCHILD	5
#define SEM2	2	/* semaphore to use for GETPID and GETVAL */
#define SEM4	4	/* semaphore to use for GETNCNT and GETZCNT */
#define ONE	1

#define SEMUN_CAST (union semun)

int pid_arr[NCHILD];

#ifdef UCLINUX
static char *argv0;
#endif

struct test_case_t {
	int semnum;		/* the primitive semaphore to use */
	int cmd;		/* the command to test */
	void (*func_test)(int);	/* the test function */
	union semun arg;
	void (*func_setup)(int);	/* the setup function if necessary */
} TC[10];

void setup_test_cases(void)
{
	int i;

	i = -1;

	/* {0, IPC_STAT, func_stat, &buf, NULL}, */
	i++;
	TC[i].semnum = 0;
	TC[i].cmd = IPC_STAT;
	TC[i].func_test = func_stat;
	TC[i].arg.buf = &buf;
	TC[i].func_setup = NULL;

	/* {0, IPC_SET, func_set, &buf, set_setup}, */
	i++;
	TC[i].semnum = 0;
	TC[i].cmd = IPC_SET;
	TC[i].func_test = func_set;
	TC[i].arg.buf = &buf;
	TC[i].func_setup = set_setup;

	/* {0, GETALL, func_gall, array, NULL}, */
	i++;
	TC[i].semnum = 0;
	TC[i].cmd = GETALL;
	TC[i].func_test = func_gall;
	TC[i].arg.array = array;
	TC[i].func_setup = NULL;

	/* {SEM4, GETNCNT, func_cnt, SEMUN_CAST &buf, cnt_setup}, */
	i++;
	TC[i].semnum = SEM4;
	TC[i].cmd = GETNCNT;
	TC[i].func_test = func_cnt;
	TC[i].arg.buf = &buf;
	TC[i].func_setup = cnt_setup;

	/* {SEM2, GETPID, func_pid, SEMUN_CAST &buf, pid_setup}, */
	i++;
	TC[i].semnum = SEM2;
	TC[i].cmd = GETPID;
	TC[i].func_test = func_pid;
	TC[i].arg.buf = &buf;
	TC[i].func_setup = pid_setup;

	/* {SEM2, GETVAL, func_gval, SEMUN_CAST &buf, NULL}, */
	i++;
	TC[i].semnum = SEM2;
	TC[i].cmd = GETVAL;
	TC[i].func_test = func_gval;
	TC[i].arg.buf = &buf;
	TC[i].func_setup = NULL;

	/* {SEM4, GETZCNT, func_cnt, SEMUN_CAST &buf, cnt_setup}, */
	i++;
	TC[i].semnum = SEM4;
	TC[i].cmd = GETZCNT;
	TC[i].func_test = func_cnt;
	TC[i].arg.buf = &buf;
	TC[i].func_setup = cnt_setup;

	/* {0, SETALL, func_sall, SEMUN_CAST array, sall_setup}, */
	i++;
	TC[i].semnum = 0;
	TC[i].cmd = SETALL;
	TC[i].func_test = func_sall;
	TC[i].arg.array = array;
	TC[i].func_setup = sall_setup;

	/* {SEM4, SETVAL, func_sval, SEMUN_CAST INCVAL, NULL}, */
	i++;
	TC[i].semnum = SEM4;
	TC[i].cmd = SETVAL;
	TC[i].func_test = func_sval;
	TC[i].arg.val = INCVAL;
	TC[i].func_setup = NULL;

	/* {0, IPC_RMID, func_rmid, SEMUN_CAST &buf, NULL} */
	i++;
	TC[i].semnum = 0;
	TC[i].cmd = IPC_RMID;
	TC[i].func_test = func_rmid;
	TC[i].arg.buf = &buf;
	TC[i].func_setup = NULL;
}

int main(int ac, char **av)
{
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */
	int i, j;

	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

#ifdef UCLINUX
	argv0 = av[0];
	maybe_run_child(&child_pid, "nd", 1, &sem_id_1);
	maybe_run_child(&child_cnt, "ndd", 2, &sem_id_1, &sops.sem_op);
#endif

	setup();			/* global setup */

	/* The following loop checks looping state if -i option given */

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		/* reset Tst_count in case we are looping */
		Tst_count = 0;

		/* loop through the test cases */
		for (i=0; i<TST_TOTAL; i++) {

			/*
			 * Set up any conditions if needed
			 */

			if (TC[i].func_setup != NULL) {
				/* call the setup function */
				switch (TC[i].cmd) {
				case GETNCNT:
					(*TC[i].func_setup)(-ONE);
					break;
				case GETZCNT:
					(*TC[i].func_setup)(0);
					break;
				default:
					(*TC[i].func_setup)(ONE);
					break;
				}
			}

			/*
			 * Use TEST macro to make the call
			 */

			TEST(semctl(sem_id_1, TC[i].semnum, TC[i].cmd,
				    TC[i].arg));
	
			if (TEST_RETURN == -1) {
				tst_resm(TFAIL, "%s call failed - errno = %d "
					 ": %s", TCID, TEST_ERRNO,
					 strerror(TEST_ERRNO));
			} else {
				if (STD_FUNCTIONAL_TEST) {
					/*
					 * call the appropriate test function
					 * and pass the return value where it
					 * is needed to perform certain tests.
					 */
					switch (TC[i].cmd) {
					case GETNCNT:
						/*FALLTHROUGH*/
					case GETZCNT:
						/*FALLTHROUGH*/
					case GETPID:
						/*FALLTHROUGH*/
					case GETVAL:
						(*TC[i].func_test)(TEST_RETURN);
						break;
					default:
						(*TC[i].func_test)(0);
						break;
					}
				} else {
					tst_resm(TPASS, "call succeeded");
				}
			}

			/*
			 * If testing GETNCNT or GETZCNT, clean up the children.
			 */
			switch (TC[i].cmd) {
			case GETNCNT:
				/*FALLTHROUGH*/
			case GETZCNT:
				for (j=0; j<NCHILD; j++) {
					if (kill(pid_arr[j], SIGKILL) == -1) {
						tst_brkm(TBROK, cleanup,
							"child kill failed");
					}
				}
				break;
			}
		}
		/*
		 * recreate the semaphore resource if looping
		 */
		if (TEST_LOOPING(lc)) {
			if ((sem_id_1 = semget(semkey, PSEMS,
			    IPC_CREAT | IPC_EXCL | SEM_RA)) == -1 ) {
				tst_brkm(TBROK, cleanup, "couldn't recreate "
					 "semaphore");
			}
		}
	}

	cleanup();

	/*NOTREACHED*/
        return(0);

}

/*
 * func_stat() - check the functionality of the IPC_STAT command with semctl()
 */
void
func_stat(int n)
{
	/* check the number of semaphores and the ipc_perm.mode value */
	if (buf.sem_nsems == PSEMS && buf.sem_perm.mode == (SEM_RA)) {
		tst_resm(TPASS, "buf.sem_nsems and buf.sem_perm.mode"
				" are correct");
	} else {
		tst_resm(TFAIL, "semaphore STAT info is incorrect");
	}
}

/*
 * set_setup() - set up for the IPC_SET command with semctl()
 */
void
set_setup(int n)
{
	/* set up a new mode for the semaphore set */
	buf.sem_perm.mode = SEM_RA | NEWMODE;
}

/*
 * func_set() - check the functionality of the IPC_SET command with semctl()
 */
void
func_set(int n)
{
	/* first stat the semaphore to get the new data */
    union semun arg;

    arg.buf = &buf;
	if (semctl(sem_id_1, 0, IPC_STAT, arg) == -1) {
		tst_resm(TBROK, "stat failed in func_set()");
		return;
	}

	/* check that the new mode is what we set */
	if (buf.sem_perm.mode == (SEM_RA | NEWMODE)) {
		tst_resm(TPASS, "buf.sem_perm.mode is correct");
	} else {
		tst_resm(TFAIL, "semaphore mode info is incorrect");
	}
}

/*
 * func_gall() - check the functionality of the GETALL command with semctl()
 */
void
func_gall(int n)
{
	int i;

	/* the initial value of the primitive semaphores should be zero */
	for (i=0 ; i<PSEMS; i++) {
		if (array[i] != 0) {
			tst_resm(TFAIL, "semaphore %d has unexpected value", i);
			return;
		}
	}
	tst_resm(TPASS, "semaphores have expected values");
}

/*
 * cnt_setup() - set up for the GETNCNT and GETZCNT commands with semctl()
 */
void
cnt_setup(int opval)
{
	int pid, i;

	sops.sem_num = SEM4;
	sops.sem_flg = 0;

	/*
	 * if seting up for GETZCNT, the semaphore value needs to be positive
	 */
	if (opval == 0) {
		/* initialize the semaphore value to ONE */
		sops.sem_op = ONE;
		if (semop(sem_id_1, &sops, 1) == -1) {
			tst_brkm(TBROK, cleanup, "semop #1 failed - cnt_setup");
		}
	}

	sops.sem_op = opval;	/* set the correct operation */

	for (i=0; i<NCHILD; i++) {
		/* fork five children to wait */
		if ((pid = FORK_OR_VFORK()) == -1) {
			tst_brkm(TBROK, cleanup, "fork failed in cnt_setup");
		}
	
		if (pid == 0) {		/* child */
#ifdef UCLINUX
			if (self_exec(argv0, "ndd", 2, sem_id_1,
				      sops.sem_op) < 0) {
				tst_brkm(TBROK, cleanup, "self_exec failed "
					 "in cnt_setup");
			}
#else
			child_cnt();
#endif
		} else {		/* parent */
			/* take a quick nap so that commands execute orderly */
			usleep(50000);

			/* save the pid so we can kill it later */
			pid_arr[i] = pid;
		}
	}
}

void
child_cnt(void)
{
	sops.sem_num = SEM4;
	sops.sem_flg = 0;

	/*
	 * Do a semop that will cause the child to sleep.
	 * The child process will be killed in the func_ncnt
	 * routine which should cause an error to be return
	 * by the semop() call.
	 */
	if (semop(sem_id_1, &sops, 1) != -1) {
		tst_resm(TBROK, "semop succeeded - cnt_setup");
	}
	exit(0);
}

/*
 * func_cnt() - check the functionality of the GETNCNT and GETZCNT commands
 *	        with semctl()
 */
void
func_cnt(int rval)
{

	if (rval == NCHILD) {
		tst_resm(TPASS, "number of sleeping processes is correct");
	} else {
		tst_resm(TFAIL, "number of sleeping processes is not correct");
	}
}

/*
 * pid_setup() - set up for the GETPID command with semctl()
 */
void
pid_setup(int n)
{
	int pid;

	/*
	 * Fork a child to do a semop that will pass. 
	 */
	if ((pid = FORK_OR_VFORK()) == -1) {
		tst_brkm(TBROK, cleanup, "fork failed in pid_setup()");
	}

	if (pid == 0) {		/* child */
#ifdef UCLINUX
		if (self_exec(argv0, "nd", 1, sem_id_1) < 0) {
			tst_brkm(TBROK, cleanup, "self_exec failed "
				 "in pid_setup()");
		}
#else
		child_pid();
#endif
	} else {		/* parent */
		/* take a quick nap so that commands execute orderly */
		usleep(50000);

		pid_arr[SEM2] = pid;
	}
}

void
child_pid(void)
{
	sops.sem_num = SEM2;	/* semaphore to change */
	sops.sem_op = ONE;	/* operation is to increment semaphore */
	sops.sem_flg = 0;

	/*
	 * Do a semop that will increment the semaphore.
	 */
	if (semop(sem_id_1, &sops, 1) == -1) {
		tst_resm(TBROK, "semop failed - pid_setup");
	}
	exit(0);
}

/*
 * func_pid() - check the functionality of the GETPID command with semctl()
 */
void
func_pid(int rval)
{
	/* compare the rval (pid) to the saved pid from the setup */
	if (rval == pid_arr[SEM2]) {
		tst_resm(TPASS, "last pid value is correct");
	} else {
		tst_resm(TFAIL, "last pid value is not correct");
	}
}

/*
 * func_gval() - check the functionality of the GETVAL command with semctl()
 */
void
func_gval(int rval)
{
	/*
	 * This is a simple test.  The semaphore value should be equal
	 * to ONE as it was set in the last test (GETPID).
	 */
	if (rval == 1) {
		tst_resm(TPASS, "semaphore value is correct");
	} else {
		tst_resm(TFAIL, "semaphore value is not correct");
	}

}

/*
 * all_setup() - set up for the SETALL command with semctl()
 */
void
sall_setup(int n)
{
	int i;

	for (i=0; i<PSEMS; i++) {
		/* initialize the array values to 3 */
		array[i] = 3;
	}
}

/*
 * func_sall() - check the functionality of the SETALL command with semctl()
 */
void
func_sall(int n)
{
	int i;
	unsigned short rarray[PSEMS];
    union semun arg;

	/*
	 * do a GETALL and compare the values to those set above
	 */

    arg.array = rarray;
	if (semctl(sem_id_1, 0, GETALL, arg) == -1) {
		tst_brkm(TBROK, cleanup, "semctl failed in func_sall");
	}

	for (i=0; i<PSEMS; i++) {
		if (array[i] != rarray[i]) {
			tst_resm(TFAIL, "semaphore values are not correct");
			return;
		}
	}

	tst_resm(TPASS, "semaphore values are correct");
}

/*
 * func_sval() - check the functionality of the SETVAL command with semctl()
 */
void
func_sval(int n)
{
	int semv;
	union semun arr;

	/*
	 * do a GETVAL and compare it to the value set above
	 */

	if ((semv = semctl(sem_id_1, SEM4, GETVAL, arr)) == -1) {
		tst_brkm(TBROK, cleanup, "semctl failed in func_sval");
	}

	if (semv != INCVAL) {
		tst_resm(TFAIL, "semaphore value is not what was set");
	} else {
		tst_resm(TPASS, "semaphore value is correct");
	}
}

/*
 * func_rmid() - check the functionality of the IPC_RMID command with semctl()
 */
void
func_rmid(int n)
{

	/*
	 * do a semop() - we should get EINVAL
	 */
	if (semop(sem_id_1, &sops, 1) != -1) {
		tst_resm(TFAIL, "semop succeeded on expected fail");
	}

	if (errno != EINVAL) {
		tst_resm(TFAIL, "returned errno - %d - is not expected", errno);
	} else {
		tst_resm(TPASS, "semaphore appears to be removed");
	}

	sem_id_1 = -1;
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
	setup_test_cases();

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

	/* get an IPC resource key */
	semkey = getipckey();

	/* create a semaphore set with read and alter permissions */
	if ((sem_id_1 =
	     semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL | SEM_RA)) == -1 ) {
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
	/* if it exists, remove the semaphore resource */
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
