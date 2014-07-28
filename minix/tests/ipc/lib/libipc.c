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
 *	libmsg.c
 *
 * DESCRIPTION
 *	common routines for the IPC system call tests.
 *
 *	The library contains the following routines:
 *
 *	getipckey()
 *	rm_queue()
 *	init_buf()
 *	rm_sema()
 *	check_root()
 *	getuserid()
 *	rm_shm()
 */

#define LIBIPC
#if 0
#include "ipcmsg.h"
#endif
#include "ipcsem.h"

#include <pwd.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>


/*
 * getipckey() - generates and returns a message key used by the "get"
 *		 calls to create an IPC resource.
 */
int
getipckey()
{
	const char a = 'a';
	int ascii_a = (int)a;
	char *curdir = NULL;
	char curdira[PATH_MAX];
	size_t size = sizeof(curdira);
	key_t ipc_key;
	struct timeval tv;

	if (NULL == (curdir = getcwd(curdira, size))) {
		tst_brkm(TBROK, cleanup, "Can't get current directory "
			 "in getipckey()");
	}

	/*
	 * Get a Sys V IPC key
	 *
	 * ftok() requires a character as a second argument.  This is
	 * refered to as a "project identifier" in the man page.  In
	 * order to maximize the chance of getting a unique key, the
	 * project identifier is a "random character" produced by
	 * generating a random number between 0 and 25 and then adding
	 * that to the ascii value of 'a'.  The "seed" for the random
	 * number is the microsecond value that is set in the timeval
	 * structure after calling gettimeofday().
	 */
	(void) gettimeofday(&tv, NULL);
	srandom((unsigned int)tv.tv_usec);

	if ((ipc_key = ftok(curdir, ascii_a + random()%26)) == -1) {
		tst_brkm(TBROK, cleanup, "Can't get msgkey from ftok()");
	}

	return(ipc_key);
}

#if 0
/*
 * rm_queue() - removes a message queue.
 */
void
rm_queue(int queue_id)
{
	if (queue_id == -1) {		/* no queue to remove */
		return;
	}

	if (msgctl(queue_id, IPC_RMID, NULL) == -1) {
		tst_resm(TINFO, "WARNING: message queue deletion failed.");
		tst_resm(TINFO, "This could lead to IPC resource problems.");
		tst_resm(TINFO, "id = %d", queue_id);
	}
}
#endif

#if 0
/*
 * init_buf() - initialize the message buffer with some text and a type.
 */
void
init_buf(MSGBUF *m_buf, int type, int size)
{
	int i;
	int ascii_a = (int)'a';		/* the ascii value for 'a' */

	/* this fills the message with a repeating alphabet string */
	for (i=0; i<size; i++) {
		m_buf->mtext[i] = ascii_a + (i % 26);
	}

	/* terminate the message */
	m_buf->mtext[i] = (char)NULL;

	/* if the type isn't valid, set it to 1 */
	if (type < 1) {
		m_buf->mtype = 1;
	} else {
		m_buf->mtype = type;
	}
}
#endif

/*
 * rm_sema() - removes a semaphore.
 */
void
rm_sema(int sem_id)
{
	union semun arr;

	if (sem_id == -1) {		/* no semaphore to remove */
		return;
	}

	if (semctl(sem_id, 0, IPC_RMID, arr) == -1) {
		tst_resm(TINFO, "WARNING: semaphore deletion failed.");
		tst_resm(TINFO, "This could lead to IPC resource problems.");
		tst_resm(TINFO, "id = %d", sem_id);
	}
}

/*
 * check_root() - make sure the process ID is root
 */
void
check_root(void)
{
	if (geteuid() != 0) {
		tst_brkm(TBROK, cleanup, "test must be run as root");
	}
}

/*
 * getuserid() - return the integer value for the "user" id
 */
int
getuserid(char *user)
{
	struct passwd *ent;

	/* allocate some space for the passwd struct */
	if ((ent = (struct passwd *)malloc(sizeof(struct passwd))) == NULL) {
	     tst_brkm(TBROK, cleanup, "couldn't allocate space for passwd"
		      " structure");
        }

	/* get the uid value for the user */
	if ((ent = getpwnam(user)) == NULL) {
		tst_brkm(TBROK, cleanup, "Couldn't get password entry for %s",
			 user);
	}

	return(ent->pw_uid);
}

/*
 * rm_shm() - removes a shared memory segment.
 */
void
rm_shm(int shm_id)
{
	if (shm_id == -1) {		/* no segment to remove */
		return;
	}

	/*
	 * check for # of attaches ? 
	 */

	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		tst_resm(TINFO, "WARNING: shared memory deletion failed.");
		tst_resm(TINFO, "This could lead to IPC resource problems.");
		tst_resm(TINFO, "id = %d", shm_id);
	}
}
