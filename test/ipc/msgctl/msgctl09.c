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
/* 11/11/2002   Port to LTP     dbarrera@us.ibm.com */


/*
 * NAME
 *	msgctl09
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

#define _XOPEN_SOURCE 500
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "test.h"
#include "usctest.h"

#define MAXNREPS	1000
#ifndef CONFIG_COLDFIRE
#define MAXNPROCS	 1000000  /* This value is set to an arbitrary high limit. */
#else
#define MAXNPROCS	 100000   /* Coldfire can't deal with 1000000 */
#endif
#define MAXNKIDS	10
#define FAIL		1
#define PASS		0

int dotest(key_t,int);
int doreader(long,int,int);
int dowriter(long,int,int);
int fill_buffer(char*,char,int);
int verify(char*,char,int,int);
void setup();
void cleanup();

/*
 * These globals must be defined in the test.
 * */


char *TCID="msgctl09";           /* Test program identifier.    */
int TST_TOTAL=1;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */

int exp_enos[]={0};     /* List must end with 0 */


key_t	keyarray[MAXNPROCS];

struct {
	long	type;
	struct {
		char	len;
		char	pbytes[99];
		} data;
	} buffer;

int	pidarray[MAXNPROCS];
int	rkidarray[MAXNKIDS];
int	wkidarray[MAXNKIDS];
int 	tid;
int 	nprocs, nreps, nkids, MSGMNI;
int 	procstat;
void 	term(int);
#ifdef UCLINUX
static char *argv0;

void do_child_1_uclinux();
static key_t key_uclinux;
static int i_uclinux;

void do_child_2_uclinux();
static int pid_uclinux;
static int child_process_uclinux;

void do_child_3_uclinux();
static int rkid_uclinux;
#endif
void cleanup_msgqueue(int i, int tid);

/*-----------------------------------------------------------------*/
int main(argc, argv)
int	argc;
char	*argv[];
{
	register int i, j, ok, pid;
	int count, status;

#ifdef UCLINUX
	char *msg;			/* message returned from parse_opts */

	argv0 = argv[0];

	/* parse standard options */
	if ((msg = parse_opts(argc, argv, (option_t *)NULL, NULL)) != (char *)NULL)
	{
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}
	
	maybe_run_child(&do_child_1_uclinux, "ndd", 1, &key_uclinux, &i_uclinux);
	maybe_run_child(&do_child_2_uclinux, "nddd", 2, &key_uclinux,
			&pid_uclinux, &child_process_uclinux);
	maybe_run_child(&do_child_3_uclinux, "nddd", 3, &key_uclinux,
			&rkid_uclinux, &child_process_uclinux);
#endif

	setup();

	if (argc == 1 )
	{
		/* Set default parameters */
		nreps = MAXNREPS;
		nprocs = MSGMNI;
		nkids = MAXNKIDS;
	}
	else if (argc == 4 )
	{
		if ( atoi(argv[1]) > MAXNREPS )
		{
                        tst_resm(TCONF,"Requested number of iterations too large, setting to Max. of %d", MAXNREPS);
			nreps = MAXNREPS;
		}
		else
		{
			nreps = atoi(argv[1]);
		}
		if (atoi(argv[2]) > MSGMNI )
		{
                        tst_resm(TCONF,"Requested number of processes too large, setting to Max. of %d", MSGMNI);
			nprocs = MSGMNI;
		}
		else
		{
			nprocs = atoi(argv[2]);
		}
		if (atoi(argv[3]) > MAXNKIDS )
		{
                        tst_resm(TCONF,"Requested number of read/write pairs too large; setting to Max. of %d", MAXNKIDS);
			nkids = MAXNKIDS;
		}
		else
		{
			nkids = atoi(argv[3]);
		}
	}
	else
	{
                tst_resm(TCONF," Usage: %s [ number of iterations  number of processes number of read/write pairs ]", argv[0]);
		tst_exit();
	}

	procstat = 0;
	srand48((unsigned)getpid() + (unsigned)(getppid() << 16));
	tid = -1;

	/* Setup signal handleing routine */
	if (sigset(SIGTERM, term) == SIG_ERR) 
	{
                tst_resm(TFAIL, "Sigset SIGTERM failed");
                tst_exit();
	}
	/* Set up array of unique keys for use in allocating message
	 * queues
	 */
	for (i = 0; i < nprocs; i++) 
	{
		ok = 1;
		do 
		{
			/* Get random key */
			keyarray[i] = (key_t)lrand48();
			/* Make sure key is unique and not private */
			if (keyarray[i] == IPC_PRIVATE) 
			{
				ok = 0;
				continue;
			}
			for (j = 0; j < i; j++) 
			{
				if (keyarray[j] == keyarray[i]) 
				{
					ok = 0;
					break;
				}
				ok = 1;
			}
		} while (ok == 0);
	}
/*-----------------------------------------------------------------*/
	/* Fork a number of processes (nprocs), each of which will
	 * create a message queue with several (nkids) reader/writer
	 * pairs which will read and write a number (iterations)
	 * of random length messages with specific values (keys).
	 */

	for (i = 0; i <  nprocs; i++) 
	{
		fflush(stdout);
		if ((pid = FORK_OR_VFORK()) < 0) 
		{
	                tst_resm(TFAIL, "\tFork failed (may be OK if under stress)");
        	        tst_exit();
		}
		/* Child does this */
		if (pid == 0) 
		{
#ifdef UCLINUX
			if (self_exec(argv[0], "ndd", 1, keyarray[i], i) < 0)
			{
				tst_resm(TFAIL, "\tself_exec failed");
				tst_exit();
			}
#else
			procstat = 1;
			exit( dotest(keyarray[i], i) );
#endif
		}
		pidarray[i] = pid;
	}

	count = 0;
	while(1)
	{
		if (( wait(&status)) > 0)
		{
			if (status>>8 != PASS )
			{
                                tst_resm(TFAIL, "Child exit status = %d", status>>8);
                                tst_exit();
			}
			count++;
		}	
		else
		{
			if (errno != EINTR)
			{	
				break;
			}
#ifdef DEBUG
                        tst_resm(TINFO,"Signal detected during wait");
#endif
		}
	}
	/* Make sure proper number of children exited */
	if (count != nprocs)
	{
                tst_resm(TFAIL, "Wrong number of children exited, Saw %d, Expected %d", count, nprocs);
                tst_exit();
	}

        tst_resm(TPASS,"msgctl09 ran successfully!");

	cleanup();
	
        return (0);



}
/*--------------------------------------------------------------------*/

#ifdef UCLINUX
void
do_child_1_uclinux()
{
	procstat = 1;
	exit(dotest(key_uclinux, i_uclinux));
}

void
do_child_2_uclinux()
{
	procstat = 2;
	exit(doreader(key_uclinux, pid_uclinux, child_process_uclinux));
}

void
do_child_3_uclinux()
{
	procstat = 2;
	exit(dowriter(key_uclinux, rkid_uclinux, child_process_uclinux));
}
#endif

void
cleanup_msgqueue(int i, int tid)
{
	/*
	 * Decrease the value of i by 1 because it
	 * is getting incremented even if the fork
	 * is failing.
	 */

	i--;
	/*
	 * Kill all children & free message queue.
	 */
	for (; i >= 0; i--) {
		(void)kill(rkidarray[i], SIGKILL);
		(void)kill(wkidarray[i], SIGKILL);
	}

	if (msgctl(tid, IPC_RMID, 0) < 0) {
		tst_resm(TFAIL, "Msgctl error in cleanup, errno = %d", errno);
		tst_exit();
	}
}

int dotest(key, child_process)
key_t 	key;
int	child_process;
{
	int id, pid;
	int i, count, status, exit_status;

	sighold(SIGTERM);
		 if ((id = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR )) < 0)
	{
                tst_resm(TFAIL, "Msgget error in child %d, errno = %d", child_process, errno);
                tst_exit();
	}
	tid = id;
	sigrelse(SIGTERM);

	exit_status = PASS;

	for (i=0; i < nkids; i++)
	{
		fflush(stdout);
		if ((pid = FORK_OR_VFORK()) < 0) 
		{
	                tst_resm(TWARN, "Fork failure in first child of child group %d", child_process);
			cleanup_msgqueue(i, tid);
			tst_exit();
		}
		/* First child does this */
		if (pid == 0) 
		{
#ifdef UCLINUX
			if (self_exec(argv0, "nddd", 2, key, getpid(),
				      child_process) < 0) {
				tst_resm(TWARN, "self_exec failed");
				cleanup_msgqueue(i, tid);
				tst_exit();
			}
#else
			procstat = 2;
			exit( doreader( key, getpid(), child_process) );
#endif
		}
		rkidarray[i] = pid;
		fflush(stdout);
		if ((pid = FORK_OR_VFORK()) < 0) 
		{
	                tst_resm(TWARN, "Fork failure in first child of child group %d", child_process);
			/*
			 * Kill the reader child process
			 */
			(void)kill(rkidarray[i], SIGKILL);

			cleanup_msgqueue(i, tid);
               		tst_exit();
		}
		/* Second child does this */
		if (pid == 0) 
		{
#ifdef UCLINUX
			if (self_exec(argv0, "nddd", 3, key, rkidarray[i],
				      child_process) < 0) {
				tst_resm(TWARN, "\tFork failure in first child "
					 "of child group %d \n", child_process);
				/*
				 * Kill the reader child process
				 */
				(void)kill(rkidarray[i], SIGKILL);

				cleanup_msgqueue(i, tid);
				tst_exit();
			}
#else
			procstat = 2;
			exit( dowriter( key, rkidarray[i], child_process) );
#endif
		}
		wkidarray[i] = pid;
	}
	/* Parent does this */
	count = 0;
	while(1)
	{
		if (( wait(&status)) > 0)
		{
			if (status>>8 != PASS )
			{
                                tst_resm(TFAIL, "Child exit status = %d from child group %d", status>>8, child_process);
				for (i = 0; i < nkids; i++) 
				{
					kill(rkidarray[i], SIGTERM);
					kill(wkidarray[i], SIGTERM);
				}
		 		 		 		 if (msgctl(tid, IPC_RMID, 0) < 0) {
		 		 		 		 		 tst_resm(TFAIL, "Msgctl error, errno = %d", errno);
		 		 		 		 }
                                tst_exit();
			}
			count++;
		}	
		else
		{
			if (errno != EINTR)
			{	
				break;
			}
		}
	}
	/* Make sure proper number of children exited */
	if (count != (nkids * 2))
	{
		tst_resm(TFAIL, "Wrong number of children exited in child group %d, Saw %d Expected %d", child_process, count, (nkids * 2));
		 		 if (msgctl(tid, IPC_RMID, 0) < 0) {
		 		 		 tst_resm(TFAIL, "Msgctl error, errno = %d", errno);
		 		 }
                tst_exit();
	}
	if (msgctl(id, IPC_RMID, 0) < 0)
	{
		tst_resm(TFAIL, "Msgctl failure in child group %d, errno %d", child_process, errno);
                tst_exit();
	}
	exit(exit_status);
}

int doreader( key, type, child)
int type, child;
long key;
{
	int i, size;
	int id;

	if ((id = msgget(key, 0)) < 0)
	{
		tst_resm(TFAIL, "Msgget error in reader of child group %d, errno = %d", child, errno);
                tst_exit();
	}
	if (id != tid)
	{
		tst_resm(TFAIL, "Message queue mismatch in reader of child group %d for message queue id %d", child, id);
		tst_exit();
	}
	for (i = 0; i < nreps; i++) 
	{
		if ((size = msgrcv(id, &buffer, 100, type, 0)) < 0) 
		{
			tst_resm(TFAIL, "Msgrcv error in child %d, read # = %d, errno = %d", (i + 1), child, errno);
	                tst_exit();
		}
		if (buffer.type != type)
		{
                        tst_resm(TFAIL, "Size mismatch in child %d, read # = %d", child, (i + 1));
                        tst_resm(TFAIL, "\tfor message size got  %d expected  %d %s",size ,buffer.data.len);
                        tst_exit();
		}
		if (buffer.data.len + 1 != size)  
		{
			tst_resm(TFAIL, "Size mismatch in child %d, read # = %d, size = %d, expected = %d", child, (i + 1), buffer.data.len, size);
	                tst_exit();
		}
		if ( verify(buffer.data.pbytes, (key % 255), size - 1, child) ) 
		{
                        tst_resm(TFAIL, "in read # = %d,key =  %x", (i + 1), child, key);
                        tst_exit();
		}
		key++;
	}
	exit(PASS);
}

int dowriter( key, type, child)
int type,child;
long key;
{
	int i, size;
	int id;

		 if ((id = msgget(key, 0)) < 0)
	{
                tst_resm(TFAIL, "Msgget error in writer of child group %d, errno = %d", child, errno);
                tst_exit();
	}
	if (id != tid)
	{
		tst_resm(TFAIL, "Message queue mismatch in writer of child group %d", child);
		tst_resm(TFAIL, "\tfor message queue id %d expected  %d",id, tid);
                tst_exit();
	}

	for (i = 0; i < nreps; i++) 
	{
		do 
		{
			size = (lrand48() % 99);
		} while (size == 0);
		fill_buffer(buffer.data.pbytes, (key % 255), size);
		buffer.data.len = size;
		buffer.type = type;
		if (msgsnd(id, &buffer, size + 1, 0) < 0)
		{
                        tst_resm(TFAIL, "Msgsnd error in child %d, key =   %x errno  = %d", child, key, errno);
			tst_exit();
		}
		key++;
	}
	exit(PASS);
}	

int fill_buffer(buf, val, size)
register char *buf;
char	val;
register int size;
{
	register int i;

	for(i = 0; i < size; i++)
		buf[i] = val;
	return(0);
}


/*
 * verify()
 *	Check a buffer for correct values.
 */

int verify(buf, val, size, child)
	register char *buf;
	char	val;
	register int size;
	int	child;
{
	while(size-- > 0)
		if (*buf++ != val)
		{
                        tst_resm(TWARN, "Verify error in child %d, *buf = %x, val = %x, size = %d", child, *buf, val, size);
                        return(FAIL);
		}
	return(PASS);
}

/* ARGSUSED */
void
term(int sig)
{
	int i;

	if (procstat == 0) 
	{
#ifdef DEBUG
	        tst_resm(TINFO,"SIGTERM signal received, test killing kids");
#endif
		for (i = 0; i < nprocs; i++) 
		{
			if ( pidarray[i] > 0){
				if ( kill(pidarray[i], SIGTERM) < 0)
				{
	        			tst_resm(TBROK,"Kill failed to kill child %d", i);
					exit(FAIL);
				}
			}
		}
		return;
	}

	if (procstat == 2) 
	{
		fflush(stdout);
		exit(PASS);
	}

	if (tid == -1) 
	{
		exit(FAIL);
	}
	for (i = 0; i < nkids; i++) 
	{
		if (rkidarray[i] > 0)
			kill(rkidarray[i], SIGTERM);
		if (wkidarray[i] > 0)
			kill(wkidarray[i], SIGTERM);
	}
}

#define BUFSIZE 512

/** Get the number of message queues already in use */
static int get_used_msgqueues()
{
        FILE *f;
        int used_queues;
        char buff[BUFSIZE];

        f = popen("ipcs -q", "r");
        if (!f) {
                tst_resm(TBROK,"Could not run 'ipcs' to calculate used message queues");
                tst_exit();
        }
        /* FIXME: Start at -4 because ipcs prints four lines of header */
        for (used_queues = -4; fgets(buff, BUFSIZE, f); used_queues++)
                ;
        pclose(f);
        if (used_queues < 0) {
                tst_resm(TBROK,"Could not read output of 'ipcs' to calculate used message queues");
                tst_exit();
        }
        return used_queues;
}

/** Get the max number of message queues allowed on system */
static int get_max_msgqueues()
{
        FILE *f;
        char buff[BUFSIZE];

        /* Get the max number of message queues allowed on system */
        f = fopen("/proc/sys/kernel/msgmni", "r");
        if (!f){
                tst_resm(TBROK,"Could not open /proc/sys/kernel/msgmni");
                tst_exit();
        }
        if (!fgets(buff, BUFSIZE, f)) {
                tst_resm(TBROK,"Could not read /proc/sys/kernel/msgmni");
                tst_exit();
        }
        fclose(f);
        return atoi(buff);
}

/***************************************************************
 * setup() - performs all ONE TIME setup for this test.
 *****************************************************************/
void
setup()
{
	tst_tmpdir();
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

        MSGMNI = get_max_msgqueues() - get_used_msgqueues();
        if (MSGMNI <= 0){
                tst_resm(TBROK,"Max number of message queues already used, cannot create more.");
                cleanup();
        }

}


/***************************************************************
 * cleanup() - performs all ONE TIME cleanup for this test at
 * completion or premature exit.
 ****************************************************************/
void
cleanup()
{
	int status;
        /*
	 * print timing stats if that option was specified.
	 * print errno log if that option was specified.
	 */
        TEST_CLEANUP;

        /*
	 * Remove the message queue from the system
	 */
#ifdef DEBUG
        tst_resm(TINFO,"Removing the message queue");
#endif
        fflush (stdout);
        (void) msgctl(tid, IPC_RMID, (struct msqid_ds *)NULL);
	if ((status = msgctl(tid, IPC_STAT, (struct msqid_ds *)NULL)) != -1)
	        {
		                (void) msgctl(tid, IPC_RMID, (struct msqid_ds *)NULL);
			                tst_resm(TFAIL, "msgctl(tid, IPC_RMID) failed");
				                tst_exit();
					        }

        fflush (stdout);
	tst_rmdir();
        /* exit with return code appropriate for results */
        tst_exit();
}

