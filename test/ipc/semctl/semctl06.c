/*
 *
 *   Copyright (c) International Business Machines  Corp., 2002
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

/* 06/30/2001	Port to Linux	nsharoff@us.ibm.com */
/* 10/30/2002	Port to LTP	dbarrera@us.ibm.com */


/*
 * NAME
 *	semctl06
 *
 * CALLS
 *	semctl(2) semget(2) semop(2)
 *
 * ALGORITHM
 *	Get and manipulate a set of semaphores.
 *
 * RESTRICTIONS
 *
 * WARNING
 *	If this test fail, it may be necessary to use the ipcs and ipcrm
 *	commands to remove any semaphores left in the system due to a
 *	premature exit of this test.
 */

#define DEBUG 0

#ifdef UCLINUX
#define _GNU_SOURCE /* for asprintf */
#include <stdio.h>
#endif

#include <sys/types.h>		/* needed for test		*/
#include <sys/ipc.h>		/* needed for test		*/
#include <sys/sem.h>		/* needed for test		*/
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include "test.h"
#include "usctest.h"
#include <sys/wait.h>
#include "ipcsem.h"

int local_flag=1;


#define NREPS	10
#define NPROCS	3
#define NKIDS	5
#define NSEMS	5
#define HVAL	1000
#define LVAL	100
#define FAILED	0

void setup (void);
void cleanup(void);

static key_t		keyarray[NPROCS];
static struct sembuf	semops[NSEMS];
static short		maxsemvals[NSEMS];
static int		pidarray[NPROCS];
static int		kidarray[NKIDS];
static int		tid;
static int		procstat;
static char	       *prog;
static unsigned short		semvals[NSEMS];

/*
 *  * These globals must be defined in the test.
 *   * */


char *TCID="semctl06";           /* Test program identifier.    */
int TST_TOTAL=1;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */

int exp_enos[]={0};     /* List must end with 0 */


static void term(int sig);
static void dosemas(int id);
static void dotest(key_t key);

#ifdef UCLINUX
static char *argv0;

void do_child();
static int id_uclinux;
static char *maxsemstring;
#endif

/*--------------------------------------------------------------*/
/*ARGSUSED*/
int
main(int argc, char **argv)
{
	register int i, j, ok, pid;
	int count, child, status, nwait;

#ifdef UCLINUX
	char *msg;
	if ((msg = parse_opts(argc, argv, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

	argv0 = argv[0];
	maybe_run_child(&do_child, "dS", &id_uclinux, &maxsemstring);
#endif

	prog = argv[0];
	nwait = 0;
	setup();		
/*--------------------------------------------------------------*/
	srand(getpid());

	tid = -1;

	for (i = 0; i < NPROCS; i++) {
		do {
			keyarray[i] = (key_t)rand();
			if (keyarray[i] == IPC_PRIVATE) {
				ok = 0;
				continue;
			}
			ok = 1;
			for (j = 0; j < i; j++) {
				if (keyarray[j] == keyarray[i]) {
					ok = 0;
					break;
				}
			}
		} while (ok == 0);
	}

	if ((signal(SIGTERM, term)) == SIG_ERR) {
                tst_resm(TFAIL, "\tsignal failed. errno = %d", errno);
		tst_exit();
	}

	for (i = 0; i <  NPROCS; i++) {
		if ((pid = FORK_OR_VFORK()) < 0) {
                        tst_resm(TFAIL, "\tFork failed (may be OK if under stress)");
                        tst_exit();
		}
		if (pid == 0) {
			procstat = 1;
			dotest(keyarray[i]);
			exit(0);
		}
		pidarray[i] = pid;
		nwait++;
	}

	/*
	 * Wait for children to finish.
	 */

	count = 0;
	while((child = wait(&status)) > 0) {
		if (status) {
	                tst_resm(TFAIL, "%s[%d] Test failed.  exit=0x%x", prog, child, status);
			local_flag = FAILED;
		}
		++count;
	}

	/*
	 * Should have collected all children.
	 */

	if (count != nwait) {
                tst_resm(TFAIL, "\tWrong # children waited on, count = %d", count);
		local_flag = FAILED;
	}

	if (local_flag != FAILED)
		tst_resm(TPASS, "semctl06 ran successfully!");
	else tst_resm(TFAIL, "semctl06 failed");
	
/*--------------------------------------------------------------*/
/* Clean up any files created by test before call to anyfail.	*/

	cleanup ();

	return (0); /* shut lint up */
}
/*--------------------------------------------------------------*/


static void
dotest(key_t key)
{
	int id, pid, status;
	int count, child, nwait;
	short i;
		 union semun get_arr;

	nwait = 0;
	srand(getpid());
	if ((id = semget(key, NSEMS, IPC_CREAT)) < 0) {
		tst_resm(TFAIL, "\tsemget() failed errno %d", errno);
		exit(1);
	}
	tid = id;
	for (i = 0; i < NSEMS; i++) {
		do {
			maxsemvals[i] = /*CASTOK*/(short)(rand() % HVAL);
		} while (maxsemvals[i] < LVAL);
		semops[i].sem_num = i;
		semops[i].sem_op = maxsemvals[i];
		semops[i].sem_flg = SEM_UNDO;
	}
	if (semop(id, semops, NSEMS) < 0) {
		tst_resm(TFAIL, "\tfirst semop() failed errno %d", errno);
		exit(1);
	}
			
	for (i = 0; i < NKIDS; i++) {
		if ((pid = FORK_OR_VFORK()) < 0) {
			tst_resm(TFAIL, "\tfork failed");
		}
		if (pid == 0) {
#ifdef UCLINUX
			int j;
			maxsemstring = "";
			for (j = 0; j < NSEMS; j++) {
				if (asprintf(&maxsemstring, "%s%s%d",
					     maxsemstring, (j ? ":" : ""),
					     maxsemvals[j]) < 0) {
					tst_resm(TBROK, "Could not serialize "
						 "maxsemvals");
					tst_exit();
				}
			}
			if (self_exec(argv0, "dS", id, maxsemstring) < 0) {
				tst_resm(TFAIL, "\tself_exec failed");
			}
#else
			dosemas(id);
#endif
		}
		if (pid > 0) {
			kidarray[i] = pid;
			nwait++;
		}
	}
			

	procstat = 2;
	/*
	 * Wait for children to finish.
	 */

	count = 0;
	while((child = wait(&status)) > 0) {
		if (status) {
	                tst_resm(TFAIL, "\t%s:dotest[%d] exited status = 0x%x", prog, child, status);
			local_flag = FAILED;
		}
		++count;
	}

	/*
	 * Should have collected all children.
	 */

	if (count != nwait) {
                tst_resm(TFAIL, "\tWrong # children waited on, count = %d", count);
		local_flag = FAILED;
	}

		 get_arr.array = semvals;
		 if (semctl(id, 0, GETALL, get_arr) < 0) {
                tst_resm(TFAIL, "\terror on GETALL");
		tst_resm(TFAIL, "\tsemctl() failed errno %d", errno);
	}

	if (DEBUG)
		tst_resm(TINFO, "\tchecking maxvals");
	for (i = 0; i < NSEMS; i++) {
		if (semvals[i] !=  maxsemvals[i]) {
			tst_resm(TFAIL, "\terror on i %d orig %d final %d", i, semvals[i],
					maxsemvals[i]);
			local_flag = FAILED;
		}
	}
	if (DEBUG)
		tst_resm(TINFO, "\tmaxvals checked");

	/* 4th arg must either be missing, or must be of type 'union semun'.
	 * CANNOT just be an int, else it crashes on ppc.
	 */
	get_arr.val = 0;
	if (semctl(id, 0, IPC_RMID, get_arr) < 0) {
		tst_resm(TFAIL, "\tsemctl(IPC_RMID) failed errno %d", errno);
		local_flag = FAILED;
	}
	if (local_flag == FAILED)
		exit(1);
}

#ifdef UCLINUX
void
do_child()
{
	int i;
	char *tok;
	char *endptr;

	tok = strtok(maxsemstring, ":");
	for (i = 0; i < NSEMS; i++) {
		if (strlen(tok) == 0) {
			tst_resm(TBROK, "Invalid argument to -C option");
			tst_exit();
		}

		maxsemvals[i] = strtol(tok, &endptr, 10);
		if (*endptr != '\0') {
			tst_resm(TBROK, "Invalid argument to -C option");
			tst_exit();
                }
		tok = strtok(NULL, ":");
	}

	dosemas(id_uclinux);
}
#endif

static void
dosemas(int id)
{
	int i, j;

	srand(getpid());
	for (i = 0; i < NREPS; i++) {
		for (j = 0; j < NSEMS; j++) {
			semops[j].sem_num = j;
			semops[j].sem_flg = SEM_UNDO;

			do {
				semops[j].sem_op = 
					( - /*CASTOK*/(short)(rand() % (maxsemvals[j]/2)));
			} while (semops[j].sem_op == 0);
		}
		if (semop(id, semops, NSEMS) < 0) {
			tst_resm(TFAIL, "\tsemop1 failed errno %d", errno);
			exit(1); 
		}
		for (j = 0; j < NSEMS; j++) {
			semops[j].sem_op = ( - semops[j].sem_op);
		}
		if (semop(id, semops, NSEMS) < 0) {
			tst_resm(TFAIL, "\tsemop2 failed errno %d", errno);
			exit(1); 
		}
	}
	exit(0);
}


/*ARGSUSED*/
static void
term(int sig)
{
	int i;

	if ((signal(SIGTERM, term)) == SIG_ERR) {
		tst_resm(TFAIL, "\tsignal failed. errno %d", errno);
		exit(1);
	}
	if (procstat == 0) {
		if (DEBUG)
			tst_resm(TINFO, "\ttest killing kids");
		for (i = 0; i < NPROCS; i++) {
			if (kill(pidarray[i], SIGTERM) != 0) {
				tst_resm(TFAIL, "Kill error pid = %d :",pidarray[1]);
			}
		}
		if (DEBUG)
			tst_resm(TINFO, "\ttest kids killed");
		return;
	}

	if (procstat == 1) {
		/* 4th arg must either be missing, or must be of type 'union semun'.
		 * CANNOT just be an int, else it crashes on ppc.
		 */
		union semun arg;
		arg.val = 0;
		(void)semctl(tid, 0, IPC_RMID, arg);
		exit(1);
	}

	if (tid == -1) {
		exit(1);
	}
	for (i = 0; i < NKIDS; i++) {
		if (kill(kidarray[i], SIGTERM) != 0) {
			tst_resm(TFAIL, "Kill error kid id = %d :",kidarray[1]);
		}
	}
}

/***************************************************************
 * setup() - performs all ONE TIME setup for this test.
 *****************************************************************/
void
setup()
{
        /* You will want to enable some signal handling so you can capture
	 * unexpected signals like SIGSEGV.
	 *                   */
        tst_sig(FORK, DEF_HANDLER, cleanup);


        /* Pause if that option was specified */
        /* One cavet that hasn't been fixed yet.  TEST_PAUSE contains the code to
	 * fork the test with the -c option.  You want to make sure you do this
	 * before you create your temporary directory.
	 */
        TEST_PAUSE;
}


/***************************************************************
 * cleanup() - performs all ONE TIME cleanup for this test at
 * completion or premature exit.
 ****************************************************************/
void
cleanup()
{
        /*
	 * print timing stats if that option was specified.
	 * print errno log if that option was specified.
	 */
        TEST_CLEANUP;

        /* exit with return code appropriate for results */
        tst_exit();
}

