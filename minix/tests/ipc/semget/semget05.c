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
 *	semget05.c
 *
 * DESCRIPTION
 *	semget05 - test for ENOSPC error
 *
 * ALGORITHM
 *	create semaphore sets in a loop until the system limit is reached
 *	loop if that option was specified
 *	attempt to create yet another semaphore set
 *	check the errno value
 *	  issue a PASS message if we get ENOSPC
 *	otherwise, the tests fails
 *	  issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  semget05 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
 *     where,  -c n : Run n copies concurrently.
 *             -e   : Turn on errno logging.
 *	       -i n : Execute test n times.
 *	       -I x : Execute test for x seconds.
 *	       -P x : Pause for x seconds between iterations.
 *	       -t   : Turn on syscall timing.
 *
 * HISTORY
 *	03/2001 - Written by Wayne Boyer
 *      07/2006 - Changes By Michael Reed
 *                - Changed the value of MAXIDS for the specific machine by reading 
 *                  the system limit for SEMMNI - The maximum number of sempahore sets    
 *
 * RESTRICTIONS
 *	none
 */

#include "../lib/ipcsem.h"

char *TCID = "semget05";
int TST_TOTAL = 1;
extern int Tst_count;

/*
 * The MAXIDS value is somewhat arbitrary and may need to be increased
 * depending on the system being tested.  
 */

int MAXIDS=2048;

int exp_enos[] = {ENOSPC, 0};	/* 0 terminated list of expected errnos */
int *sem_id_arr;
int num_sems = 0;		/* count the semaphores created */

int main(int ac, char **av)
{
	int lc,getmaxid=0;				/* loop counter */
	char *msg;			/* message returned from parse_opts */
	FILE *fp;

	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

	/* Set the MAXIDS for the specific machine by reading the system limit
           for SEMMNI - The maximum number of sempahore sets                  */
	if((fp = fopen("/proc/sys/kernel/sem", "r")) != NULL) 
	  {
	    for(lc= 0; lc < 4; lc++)
	      {
		if(lc == 3)
		  {
		    if(getmaxid > MAXIDS)
		      MAXIDS=getmaxid;
		  }
	      }

	  }
	if(fp) 
		fclose(fp);

	sem_id_arr = (int*)malloc(sizeof(int)*MAXIDS);

	setup();			/* global setup */	

	/* The following loop checks looping state if -i option given */

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		/* reset Tst_count in case we are looping */
		Tst_count = 0;

		/* use the TEST macro to make the call */
	
		TEST(semget(semkey + num_sems, PSEMS,
			    IPC_CREAT | IPC_EXCL | SEM_RA));
		/*	printf("rc = %ld \n",	TEST_RETURN); */
		if (TEST_RETURN != -1) {
			tst_resm(TFAIL, "call succeeded when error expected");
			continue;
		}
	
		TEST_ERROR_LOG(TEST_ERRNO);

		switch(TEST_ERRNO) {
		case ENOSPC:
			tst_resm(TPASS, "expected failure - errno "
				 "= %d : %s", TEST_ERRNO, strerror(TEST_ERRNO));
			break;
		default:
			tst_resm(TFAIL, "unexpected error - %d : %s",
				 TEST_ERRNO, strerror(TEST_ERRNO));
			break;
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
	int sem_q;

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

	/*
	 * Use a while loop to create the maximum number of semaphore sets.
	 * If the loop exceeds MAXIDS, then break the test and cleanup.
	 */
	while((sem_q =
		 semget(semkey + num_sems, PSEMS, IPC_CREAT|IPC_EXCL)) != -1) {
		sem_id_arr[num_sems++] = sem_q;
		if (num_sems == MAXIDS) {
			tst_brkm(TBROK, cleanup, "The maximum number of "
				 "semaphore ID's has been\n\t reached.  Please "
				 "increase the MAXIDS value in the test.");
		}
	}

	/*
	 * If the errno is other than ENOSPC, then something else is wrong.
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

	/* remove the semaphore resources that were created */
	for (i=0; i<num_sems; i++) {
		rm_sema(sem_id_arr[i]);
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

