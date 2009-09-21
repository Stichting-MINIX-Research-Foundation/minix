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
/* 11/06/2002	Port to LTP	dbarrera@us.ibm.com */

/*
 * NAME
 *	msgctl06
 *
 * CALLS
 *	msgget(2) msgctl(2)
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
#include <stdio.h>		/* needed by testhead.h		*/
#include <errno.h>		/* definitions needed for errno */
#include "test.h"
#include "usctest.h"

void setup();
void cleanup();
/*
 *  *  * These globals must be defined in the test.
 *   *   */


char *TCID="msgctl06";           /* Test program identifier.    */
int TST_TOTAL=1;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */

int exp_enos[]={0};     /* List must end with 0 */


/*
 * msgctl3_t -- union of msgctl(2)'s possible argument # 3 types.
 */
typedef union msgctl3_u {
	struct msqid_ds	*msq_ds;	/* pointer to msqid_ds struct */
	struct ipc_acl	*msq_acl;	/* pointer ACL buff and size */
} msgctl3_t;

extern int local_flag;

int	msqid, status;
struct msqid_ds	buf;


#define K 1024

/*--------------------------------------------------------------*/

int main(argc, argv)
int argc;
char *argv[];
{
	key_t		key;
	setup();

	key = 2 * K;
	TEST(msgget(key, IPC_CREAT));
	msqid = TEST_RETURN;
	if (TEST_RETURN == -1)
	{
                tst_resm(TFAIL, "msgget() failed errno = %d", errno);
	        tst_exit();
	}
	TEST(msgctl(msqid, IPC_STAT, &buf));
	status = TEST_RETURN;
	if (TEST_RETURN == -1)
	{
		(void) msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
                tst_resm(TFAIL, "msgctl(msqid, IPC_STAT, &buf) failed errno = %d", errno);
	        tst_exit();
	}

	/*
	 * Check contents of msqid_ds structure.
	 */

	if (buf.msg_qnum != 0) 
	{
                tst_resm(TFAIL, "error: unexpected nbr of messages %d", buf.msg_qnum);
	        tst_exit();
	}
	if (buf.msg_perm.uid != getuid()) 
	{
                tst_resm(TFAIL, "error: unexpected uid %d", buf.msg_perm.uid);
	        tst_exit();
	}
	if (buf.msg_perm.gid != getgid())
	{
                tst_resm(TFAIL, "error: unexpected gid %d", buf.msg_perm.gid);
	        tst_exit();
	}
	if (buf.msg_perm.cuid != getuid())
	{
                tst_resm(TFAIL, "error: unexpected cuid %d", buf.msg_perm.cuid);
	        tst_exit();
	}
	if (buf.msg_perm.cgid != getgid())
	{
                tst_resm(TFAIL, "error: unexpected cgid %d", buf.msg_perm.cgid);
	        tst_exit();
	}


	tst_resm(TPASS,"msgctl06 ran successfully!");
    /***************************************************************
     * cleanup and exit
     ***************************************************************/
	cleanup();

	return 0;
}       /* End main */    


/***************************************************************
 *  * setup() - performs all ONE TIME setup for this test.
 *   ****************************************************************/
void
setup()
{
       /* You will want to enable some signal handling so you can capture
        * unexpected signals like SIGSEGV.
        */
        tst_sig(NOFORK, DEF_HANDLER, cleanup);

        /* Pause if that option was specified */
        /* One cavet that hasn't been fixed yet.  TEST_PAUSE contains the code to
	 * fork the test with the -c option.  You want to make sure you do this
	 * before you create your temporary directory.
	 */
	TEST_PAUSE;
}


/***************************************************************
 *  *  * cleanup() - performs all ONE TIME cleanup for this test at
 *   *   *              completion or premature exit.
 *    *    ***************************************************************/
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
	tst_resm(TINFO,"Remove the message queue");
#endif
	fflush (stdout);
	(void) msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
	if ((status = msgctl(msqid, IPC_STAT, &buf)) != -1)
	{
		(void) msgctl(msqid, IPC_RMID, (struct msqid_ds *)NULL);
                tst_resm(TFAIL, "msgctl(msqid, IPC_RMID) failed");
	        tst_exit();
		
	}

	fflush (stdout);
        /* exit with return code appropriate for results */
        tst_exit();
}
