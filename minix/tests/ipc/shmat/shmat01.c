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
 *	shmat01.c
 *
 * DESCRIPTION
 *	shmat01 - test that shmat() works correctly
 *
 * ALGORITHM
 *	create a shared memory resouce with read/write permissions
 *	loop if that option was specified
 *	call shmat() with the TEST() macro using three valid conditions
 *	check the return code
 *	  if failure, issue a FAIL message.
 *	otherwise,
 *	  if doing functionality testing
 *		check for the correct conditions after the call
 *	  	if correct,
 *			issue a PASS message
 *		otherwise
 *			issue a FAIL message
 *	call cleanup
 *
 * USAGE:  <for command-line>
 *  shmat01 [-c n] [-f] [-i n] [-I x] [-P x] [-t]
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

#include "ipcshm.h"

char *TCID = "shmat01";
int TST_TOTAL = 3;
extern int Tst_count;

#define CASE0		10		/* values to write into the shared */
#define CASE1		20		/* memory location.		   */

#ifdef __ia64__
#define UNALIGNED      0x5ff00eee      /* an address not evenly divisible by */
#elif defined __XTENSA__               /* SHMLBA which defaults to 0x8048e8b */
/* TASK_SIZE on Xtensa is only 0x40000000 */
#define UNALIGNED      0x28ffeeee
#elif defined __arm__
#define UNALIGNED      0x28ffeeee
#else
#define UNALIGNED      0x5fffeeee
#endif

int shm_id_1 = -1;

void	*addr;				/* for result of shmat-call */

struct test_case_t {
	int *shmid;
	void *addr;
	int flags;
} TC[] = {
	/* a straight forward read/write attach */
	{&shm_id_1, 0, 0},

       /* an attach using non aligned memory */
	{&shm_id_1, (void *)UNALIGNED, SHM_RND},

	/* a read only attach */
	{&shm_id_1, 0, SHM_RDONLY}
};

int main(int ac, char **av)
{
	int lc;				/* loop counter */
	char *msg;			/* message returned from parse_opts */
	int i;
	void check_functionality(int);

	/* parse standard options */
	if ((msg = parse_opts(ac, av, (option_t *)NULL, NULL)) != (char *)NULL){
		tst_brkm(TBROK, cleanup, "OPTION PARSING ERROR - %s", msg);
	}

	setup();			/* global setup */

	/* The following loop checks looping state if -i option given */

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		/* reset Tst_count in case we are looping */
		Tst_count = 0;

		/* loop through the test cases */
		for (i=0; i<TST_TOTAL; i++) {

			/*
			 * Use TEST macro to make the call
			 */
			errno = 0;
			addr = shmat(*(TC[i].shmid), (void *)(TC[i].addr),
				   TC[i].flags);
			TEST_ERRNO = errno;

			if (addr == (void *)-1) {
				tst_brkm(TFAIL, cleanup, "%s call failed - "
					 "errno = %d : %s", TCID, TEST_ERRNO,
					 strerror(TEST_ERRNO));
			} else {
				if (STD_FUNCTIONAL_TEST) {
					check_functionality(i);
				} else {
					tst_resm(TPASS, "call succeeded");
				}
			}

			/*
			 * clean up things in case we are looping - in
			 * this case, detach the shared memory
			 */
			if (shmdt((const void *)addr) == -1) {
				tst_brkm(TBROK, cleanup,
					 "Couldn't detach shared memory");
			}
		}
	}

	cleanup();

	/*NOTREACHED*/
	return(0);
}

/*
 * check_functionality - check various conditions to make sure they 
 *			 are correct.
 */
void
check_functionality(int i)
{
	void *orig_add;
	int *shared;
	int fail = 0;
	struct shmid_ds buf;

	shared = (int *)addr;

	/* stat the shared memory ID */
	if (shmctl(shm_id_1, IPC_STAT, &buf) == -1) {
		tst_brkm(TBROK, cleanup, "couldn't stat shared memory");
	}

	/* check the number of attaches */
	if (buf.shm_nattch != 1) {
		tst_resm(TFAIL, "# of attaches is incorrect");
		return;
	}

	/* check the size of the segment */
	if (buf.shm_segsz != INT_SIZE) {
		tst_resm(TFAIL, "segment size is incorrect");
		return;
	}

	/* check for specific conditions depending on the type of attach */
	switch(i) {
	case 0:
		/*
		 * Check the functionality of the first call by simply
		 * "writing" a value to the shared memory space.
		 * If this fails the program will get a SIGSEGV, dump
		 * core and exit.
		 */

		*shared = CASE0;
		break;
	case 1:
		/*
		 * Check the functionality of the second call by writing
		 * a value to the shared memory space and then checking
		 * that the original address given was rounded down as
		 * specified in the man page.
		 */

		*shared = CASE1;
		orig_add = (void *)((unsigned long)addr + (((unsigned long)TC[i].addr)%SHMLBA));
		if (orig_add != TC[i].addr) {
			tst_resm(TFAIL, "shared memory address is not "
				 "correct");
			fail = 1;
		}
		break;
	case 2:
		/*
		 * This time the shared memory is read only.  Read the value
		 * and check that it is equal to the value set in case #2,
		 * because shared memory is persistent.
		 */

		if (*shared != CASE1) {
			tst_resm(TFAIL, "shared memory value isn't correct");
			fail = 1;
		}
		break;
	}

	if (!fail) {
		tst_resm(TPASS, "conditions and functionality are correct");
	}
}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void
setup(void)
{
	/* capture signals */
	tst_sig(NOFORK, DEF_HANDLER, cleanup);

	/* Pause if that option was specified */
	TEST_PAUSE;

	/*
	 * Create a temporary directory and cd into it.
	 * This helps to ensure that a unique msgkey is created.
	 * See ../lib/libipc.c for more information.
	 */
	tst_tmpdir();

	/* Get an IPC resouce key */
	shmkey = getipckey();

	/* create a shared memory resource with read and write permissions */
	if ((shm_id_1 = shmget(shmkey++, INT_SIZE, SHM_RW | IPC_CREAT |
	     IPC_EXCL)) == -1) {
		tst_brkm(TBROK, cleanup, "Failed to create shared memory "
			 "resource 1 in setup()");
	}
}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void
cleanup(void)
{
	/* if it exists, remove the shared memory resource */
	rm_shm(shm_id_1);

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

