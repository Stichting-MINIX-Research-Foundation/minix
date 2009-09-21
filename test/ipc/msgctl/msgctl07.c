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
/* 11/06/2002   Port to LTP     dbarrera@us.ibm.com */


/*
 * NAME
 *	msgctl07
 *
 * CALLS
 *	msgget(2) msgctl(2) msgop(2)
 *
 * ALGORITHM
 *	Get and manipulate a message queue.
 *
 * RESTRICTIONS
 *
 */


#include <sys/types.h>		/* needed for test		*/
#include <sys/ipc.h>		/* needed for test		*/
#include <sys/msg.h>		/* needed for test		*/
#include <signal.h>		/* needed for test		*/
#include <wait.h>		/* needed for test		*/
#include <stdio.h>		/* needed by testhead.h		*/
#include "test.h"
#include "usctest.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

typedef void (*sighandler_t)(int);
volatile int ready;

#define K 1024
#define BYTES 100
#define SECS 10

void setup();
void cleanup();
void do_child_1();
void do_child_2();

/*
 *  *  *  * These globals must be defined in the test.
 *   *   *   */

char *TCID="msgctl07";           /* Test program identifier.    */
int TST_TOTAL=1;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */

/* Used by main() and do_child_1(): */
static int msqid;
struct my_msgbuf {
	long type;
	char text[BYTES];
} p1_msgp, p2_msgp, p3_msgp, c1_msgp, c2_msgp, c3_msgp;


/*--------------------------------------------------------------*/

int main(argc, argv)
int argc;
char *argv[];
{
	key_t key;
	int pid, status;
	int i, j, k;
	sighandler_t alrm();

#ifdef UCLINUX
	char *msg;

        /* parse standard options */
        if ((msg = parse_opts(argc, argv, (option_t *)NULL, NULL)) !=
							(char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
        }

	maybe_run_child(&do_child_1, "ndd", 1, &msqid, &c1_msgp.type);
	maybe_run_child(&do_child_2, "ndddd", 2, &msqid, &c1_msgp.type,
			&c2_msgp.type, &c3_msgp.type);
#endif

	key = 2 * K;
	if ((msqid = msgget(key, IPC_CREAT)) == -1) 
        {
                tst_resm(TFAIL, "msgget() failed errno = %d", errno);
                tst_exit();

	}

	pid = FORK_OR_VFORK();
	if (pid < 0) {
		(void) msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
                tst_resm(TFAIL, "\tFork failed (may be OK if under stress)");
                tst_exit();
	}
	else if (pid == 0) {
#ifdef UCLINUX
		if (self_exec(argv[0], "ndd", 1, msqid, c1_msgp.type) < 0) {
			tst_resm(TFAIL, "\tself_exec failed");
			tst_exit();
		}
#else
		do_child_1();
#endif
	}
	else {
		struct sigaction act;

		memset(&act, 0, sizeof(act));
		act.sa_handler = (sighandler_t) alrm;
		sigemptyset(&act.sa_mask);
		sigaddset(&act.sa_mask, SIGALRM);
		if ((sigaction(SIGALRM, &act, NULL)) < 0) {
			kill(pid, SIGKILL);
			(void)msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
        	        tst_resm(TFAIL, "signal failed. errno = %d", errno);
                	tst_exit();
		}
		ready = 0;
		alarm(SECS);
		while (!ready)		/* make the child wait */
			;
		for (i=0; i<BYTES; i++) 
			p1_msgp.text[i] = 'i';
		p1_msgp.type = 1;
		if (msgsnd(msqid, &p1_msgp, BYTES, 0) == -1) {
			kill(pid, SIGKILL);
			(void)msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
        	        tst_resm(TFAIL, "msgsnd() failed. errno = %d", errno);
                	tst_exit();
		}
		wait(&status);
	}
	if ((status >> 8) == 1)
	{
       	        tst_resm(TFAIL, "test failed. status = %d", (status >> 8));
               	tst_exit();
	}
	
	pid = FORK_OR_VFORK();
	if (pid < 0) {
		(void) msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
                tst_resm(TFAIL, "\tFork failed (may be OK if under stress)");
		tst_exit();
	}
	else if (pid == 0) {
#ifdef UCLINUX
		if (self_exec(argv[0], "ndddd", 1, msqid, c1_msgp.type,
			      c2_msgp.type, c3_msgp.type) < 0) {
			tst_resm(TFAIL, "\tself_exec failed");
			tst_exit();
		}
#else
		do_child_2();
#endif
	}
	else {
		struct sigaction act;

		memset(&act, 0, sizeof(act));
		act.sa_handler = (sighandler_t) alrm;
		sigemptyset(&act.sa_mask);
		sigaddset(&act.sa_mask, SIGALRM);
		if ((sigaction(SIGALRM, &act, NULL)) < 0) {
			kill(pid, SIGKILL);
			(void)msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
        	        tst_resm(TFAIL, "signal failed. errno = %d", errno);
                	tst_exit();
		}
		ready = 0;
		alarm(SECS);
		while (!ready)		/* make the child wait */
			;
		for (i=0; i<BYTES; i++) 
			p1_msgp.text[i] = 'i';
		p1_msgp.type = 1;
		if (msgsnd(msqid, &p1_msgp, BYTES, 0) == -1) {
			kill(pid, SIGKILL);
			(void)msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
        	        tst_resm(TFAIL, "msgsnd() failed. errno = %d", errno);
                	tst_exit();
		}
		for (j=0; j<BYTES; j++) 
			p2_msgp.text[j] = 'j';
		p2_msgp.type = 2;
		if (msgsnd(msqid, &p2_msgp, BYTES, 0) == -1) {
			kill(pid, SIGKILL);
			(void)msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
        	        tst_resm(TFAIL, "msgsnd() failed. errno = %d", errno);
                	tst_exit();
		}
		for (k=0; k<BYTES; k++) 
			p3_msgp.text[k] = 'k';
		p3_msgp.type = 3;
		if (msgsnd(msqid, &p3_msgp, BYTES, 0) == -1) {
			kill(pid, SIGKILL);
			(void)msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
        	        tst_resm(TFAIL, "msgsnd() failed. errno = %d", errno);
                	tst_exit();
		}
		wait(&status);
	}
	if ((status >> 8) == 1)
	{
       	        tst_resm(TFAIL, "test failed. status = %d", (status >> 8));
               	tst_exit();
	}
        /*
	 * Remove the message queue from the system
	 */
#ifdef DEBUG
        tst_resm(TINFO,"Removing the message queue");
#endif
        fflush (stdout);
        (void) msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
        if ((status = msgctl(msqid, IPC_STAT, (struct msqid_ds *)NULL)) != -1)
        {
                (void) msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
                tst_resm(TFAIL, "msgctl(msqid, IPC_RMID) failed");
                tst_exit();

        }

        fflush (stdout);
        tst_resm(TPASS,"msgctl07 ran successfully!");
        return (0);

}
/*--------------------------------------------------------------*/

sighandler_t alrm(sig)
int sig;
{
	ready++;
	return(0);
}
/*--------------------------------------------------------------*/

void
do_child_1()
{	
	int i;
	int size;

	if ((size = msgrcv(msqid, &c1_msgp, BYTES, 0, 0)) == -1)
	{
		tst_resm(TFAIL, "msgrcv() failed errno = %d", errno);
		tst_exit();
	}
	if (size != BYTES) 
	{
		tst_resm(TFAIL, "error: received %d bytes expected %d", size, BYTES);
		tst_exit();
	}
	for (i=0; i<BYTES; i++) 
		if (c1_msgp.text[i] != 'i') 
		{
			tst_resm(TFAIL, "error: corrup message");
			tst_exit();
		}
	exit(0);
}

void
do_child_2()
{
	int i, j, k;
	int size;

	if ((size = msgrcv(msqid, &c3_msgp, BYTES, 3, 0)) == -1) {
		tst_resm(TFAIL, "msgrcv() failed errno = %d", errno);
		tst_exit();
	}
	if (size != BYTES) {
		tst_resm(TFAIL, "error: received %d bytes expected %d", size, BYTES);
		tst_exit();
	}
	for (k=0; k<BYTES; k++) 
		if (c3_msgp.text[k] != 'k') {
			tst_resm(TFAIL, "error: corrupt message");
			tst_exit();
		}
	if ((size = msgrcv(msqid, &c2_msgp, BYTES, 2, 0)) == -1) {
		tst_resm(TFAIL, "msgrcv() failed errno = %d", errno);
		tst_exit();
	}
	if (size != BYTES) {
		tst_resm(TFAIL, "error: received %d bytes expected %d", size, BYTES);
		tst_exit();
	}
	for (j=0; j<BYTES; j++) 
		if (c2_msgp.text[j] != 'j') {
			tst_resm(TFAIL, "error: corrupt message");
			tst_exit();
		}
	if ((size = msgrcv(msqid, &c1_msgp, BYTES, 1, 0)) == -1) {
		tst_resm(TFAIL, "msgrcv() failed errno = %d", errno);
		tst_exit();
	}
	if (size != BYTES) {
		tst_resm(TFAIL, "error: received %d bytes expected %d", size, BYTES);
		tst_exit();
	}
	for (i=0; i<BYTES; i++) 
		if (c1_msgp.text[i] != 'i') {
			tst_resm(TFAIL, "error: corrupt message");
			tst_exit();
		}
	
	exit(0);
}

/***************************************************************
 * setup() - performs all ONE TIME setup for this test.
 ****************************************************************/
void
setup()
{
        /* You will want to enable some signal handling so you can capture
	 * unexpected signals like SIGSEGV.
         */
        tst_sig(FORK, DEF_HANDLER, cleanup);

	/* Pause if that option was specified */
	/* One cavet that hasn't been fixed yet.  TEST_PAUSE contains the code to
	 * fork the test with the -c option.  You want to make sure you do this
         * before you create your temporary directory.
         */
	TEST_PAUSE;
}


/***************************************************************
 *  * cleanup() - performs all ONE TIME cleanup for this test at
 *   *              completion or premature exit.
 *    ***************************************************************/
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

