/*	$NetBSD: t_sysv.c,v 1.2 2012/11/06 18:31:53 pgoyette Exp $	*/

/*-
 * Copyright (c) 1999, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Test the SVID-compatible Message Queue facility.
 */

#include <atf-c.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/param.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>

volatile int did_sigsys, did_sigchild;
volatile int child_status, child_count;

void	sigsys_handler(int);
void	sigchld_handler(int);

key_t	get_ftok(int);

void	print_msqid_ds(struct msqid_ds *, mode_t);
void	receiver(void);

void	print_semid_ds(struct semid_ds *, mode_t);
void	waiter(void);

void	print_shmid_ds(struct shmid_ds *, mode_t);
void	sharer(void);

#define	MESSAGE_TEXT_LEN	256

struct mymsg {
	long	mtype;
	char	mtext[MESSAGE_TEXT_LEN];
};

const char *m1_str = "California is overrated.";
const char *m2_str = "The quick brown fox jumped over the lazy dog.";

size_t	pgsize;

#define	MTYPE_1		1
#define	MTYPE_1_ACK	2

#define	MTYPE_2		3
#define	MTYPE_2_ACK	4

int	sender_msqid = -1;
int	sender_semid = -1;
int	sender_shmid = -1;
pid_t	child_pid;

key_t	msgkey, semkey, shmkey;

int	maxloop = 1;

union semun {
	int	val;		/* value for SETVAL */
	struct	semid_ds *buf;	/* buffer for IPC_{STAT,SET} */
	u_short	*array;		/* array for GETALL & SETALL */
};


void
sigsys_handler(int signo)
{

	did_sigsys = 1;
}

void
sigchld_handler(int signo)
{
	int c_status;

	did_sigchild = 1;
	/*
	 * Reap the child and return its status
	 */
	if (wait(&c_status) == -1)
		child_status = -errno;
	else
		child_status = c_status;

	child_count--;
}

key_t get_ftok(int id)
{
	int fd;
	char token_key[64], token_dir[64];
	char *tmpdir;
	key_t key;

	strlcpy(token_key, "/tmp/t_sysv.XXXXXX", sizeof(token_key));
	tmpdir = mkdtemp(token_key);
	ATF_REQUIRE_MSG(tmpdir != NULL, "mkdtemp() failed: %d", errno);

	strlcpy(token_dir, tmpdir, sizeof(token_dir));
	strlcpy(token_key, tmpdir, sizeof(token_key));
	strlcat(token_key, "/token_key", sizeof(token_key));

	/* Create the file, since ftok() requires it to exist! */

	fd = open(token_key, O_RDWR | O_CREAT | O_EXCL);
	if (fd == -1) {
		rmdir(tmpdir);
		atf_tc_fail("open() of temp file failed: %d", errno);
		return (key_t)-1;
	} else
		close(fd);

	key = ftok(token_key, id);

	ATF_REQUIRE_MSG(unlink(token_key) != -1, "unlink() failed: %d", errno);
	ATF_REQUIRE_MSG(rmdir(token_dir) != -1, "rmdir() failed: %d", errno);

	return key;
}

ATF_TC_WITH_CLEANUP(msg);
ATF_TC_HEAD(msg, tc)
{  

	atf_tc_set_md_var(tc, "timeout", "3");
	atf_tc_set_md_var(tc, "descr", "Checks sysvmsg passing");
}

ATF_TC_BODY(msg, tc)
{
	struct sigaction sa;
	struct msqid_ds m_ds;
	struct mymsg m;
	sigset_t sigmask;
	int loop;
	int c_status;

	/*
	 * Install a SIGSYS handler so that we can exit gracefully if
	 * System V Message Queue support isn't in the kernel.
	 */
	did_sigsys = 0;
	sa.sa_handler = sigsys_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ATF_REQUIRE_MSG(sigaction(SIGSYS, &sa, NULL) != -1,
	    "sigaction SIGSYS: %d", errno);

	/*
	 * Install a SIGCHLD handler to deal with all possible exit
	 * conditions of the receiver.
	 */
	did_sigchild = 0;
	child_count = 0;
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ATF_REQUIRE_MSG(sigaction(SIGCHLD, &sa, NULL) != -1,
	    "sigaction SIGCHLD: %d", errno);

	msgkey = get_ftok(4160);
	ATF_REQUIRE_MSG(msgkey != (key_t)-1, "get_ftok failed");

	sender_msqid = msgget(msgkey, IPC_CREAT | 0640);
	ATF_REQUIRE_MSG(sender_msqid != -1, "msgget: %d", errno);

	if (did_sigsys) {
		atf_tc_skip("SYSV Message Queue not supported");
		return;
	}

	ATF_REQUIRE_MSG(msgctl(sender_msqid, IPC_STAT, &m_ds) != -1,
	"msgctl IPC_STAT 1: %d", errno);

	print_msqid_ds(&m_ds, 0640);

	m_ds.msg_perm.mode = (m_ds.msg_perm.mode & ~0777) | 0600;

	ATF_REQUIRE_MSG(msgctl(sender_msqid, IPC_SET, &m_ds) != -1,
	    "msgctl IPC_SET: %d", errno);

	memset(&m_ds, 0, sizeof(m_ds));

	ATF_REQUIRE_MSG(msgctl(sender_msqid, IPC_STAT, &m_ds) != -1,
	    "msgctl IPC_STAT 2: %d", errno);

	ATF_REQUIRE_MSG((m_ds.msg_perm.mode & 0777) == 0600,
	    "IPC_SET of mode didn't hold");

	print_msqid_ds(&m_ds, 0600);

	switch ((child_pid = fork())) {
	case -1:
		atf_tc_fail("fork: %d", errno);
		return;

	case 0:
		child_count++;
		receiver();
		break;

	default:
		break;
	}

	for (loop = 0; loop < maxloop; loop++) {
		/*
		 * Send the first message to the receiver and wait for the ACK.
		 */
		m.mtype = MTYPE_1;
		strcpy(m.mtext, m1_str);
		ATF_REQUIRE_MSG(msgsnd(sender_msqid, &m, sizeof(m), 0) != -1,
		    "sender: msgsnd 1: %d", errno);

		ATF_REQUIRE_MSG(msgrcv(sender_msqid, &m, sizeof(m),
				       MTYPE_1_ACK, 0) == sizeof(m),
		    "sender: msgrcv 1 ack: %d", errno);

		print_msqid_ds(&m_ds, 0600);

		/*
		 * Send the second message to the receiver and wait for the ACK.
		 */
		m.mtype = MTYPE_2;
		strcpy(m.mtext, m2_str);
		ATF_REQUIRE_MSG(msgsnd(sender_msqid, &m, sizeof(m), 0) != -1,
		    "sender: msgsnd 2: %d", errno);

		ATF_REQUIRE_MSG(msgrcv(sender_msqid, &m, sizeof(m),
				       MTYPE_2_ACK, 0) == sizeof(m),
		    "sender: msgrcv 2 ack: %d", errno);
	}

	/*
	 * Wait for child to finish
	 */
	sigemptyset(&sigmask);
	(void) sigsuspend(&sigmask);

	/*
	 * ...and any other signal is an unexpected error.
	 */
	if (did_sigchild) {
		c_status = child_status;
		if (c_status < 0)
			atf_tc_fail("waitpid: %d", -c_status);
		else if (WIFEXITED(c_status) == 0)
			atf_tc_fail("child abnormal exit: %d", c_status);
		else if (WEXITSTATUS(c_status) != 0)
			atf_tc_fail("c status: %d", WEXITSTATUS(c_status));
		else {
			ATF_REQUIRE_MSG(msgctl(sender_msqid, IPC_STAT, &m_ds)
			    != -1, "msgctl IPC_STAT: %d", errno);

			print_msqid_ds(&m_ds, 0600);
			atf_tc_pass();
		}
	} else
		atf_tc_fail("sender: received unexpected signal");
}

ATF_TC_CLEANUP(msg, tc)
{

	/*
	 * Remove the message queue if it exists.
	 */
	if (sender_msqid != -1)
		ATF_REQUIRE_MSG(msgctl(sender_msqid, IPC_RMID, NULL) != -1,
		    "msgctl IPC_RMID: %d", errno);
	sender_msqid = -1;
}

void
print_msqid_ds(mp, mode)
	struct msqid_ds *mp;
	mode_t mode;
{
	uid_t uid = geteuid();
	gid_t gid = getegid();

	printf("PERM: uid %d, gid %d, cuid %d, cgid %d, mode 0%o\n",
	    mp->msg_perm.uid, mp->msg_perm.gid,
	    mp->msg_perm.cuid, mp->msg_perm.cgid,
	    mp->msg_perm.mode & 0777);

	printf("qnum %lu, qbytes %lu, lspid %d, lrpid %d\n",
	    mp->msg_qnum, (u_long)mp->msg_qbytes, mp->msg_lspid,
	    mp->msg_lrpid);

	printf("stime: %s", ctime(&mp->msg_stime));
	printf("rtime: %s", ctime(&mp->msg_rtime));
	printf("ctime: %s", ctime(&mp->msg_ctime));

	/*
	 * Sanity check a few things.
	 */

	ATF_REQUIRE_MSG(mp->msg_perm.uid == uid && mp->msg_perm.cuid == uid,
	    "uid mismatch");

	ATF_REQUIRE_MSG(mp->msg_perm.gid == gid && mp->msg_perm.cgid == gid,
	    "gid mismatch");

	ATF_REQUIRE_MSG((mp->msg_perm.mode & 0777) == mode, "mode mismatch");
}

void
receiver()
{
	struct mymsg m;
	int msqid, loop;

	if ((msqid = msgget(msgkey, 0)) == -1)
		err(1, "receiver: msgget");

	for (loop = 0; loop < maxloop; loop++) {
		/*
		 * Receive the first message, print it, and send an ACK.
		 */
		if (msgrcv(msqid, &m, sizeof(m), MTYPE_1, 0) != sizeof(m))
			err(1, "receiver: msgrcv 1");

		printf("%s\n", m.mtext);
		if (strcmp(m.mtext, m1_str) != 0)
			err(1, "receiver: message 1 data isn't correct");

		m.mtype = MTYPE_1_ACK;

		if (msgsnd(msqid, &m, sizeof(m), 0) == -1)
			err(1, "receiver: msgsnd ack 1");

		/*
		 * Receive the second message, print it, and send an ACK.
		 */

		if (msgrcv(msqid, &m, sizeof(m), MTYPE_2, 0) != sizeof(m))
			err(1, "receiver: msgrcv 2");

		printf("%s\n", m.mtext);
		if (strcmp(m.mtext, m2_str) != 0)
			err(1, "receiver: message 2 data isn't correct");

		m.mtype = MTYPE_2_ACK;

		if (msgsnd(msqid, &m, sizeof(m), 0) == -1)
			err(1, "receiver: msgsnd ack 2");
	}

	exit(0);
}

/*
 * Test the SVID-compatible Semaphore facility.
 */

ATF_TC_WITH_CLEANUP(sem);
ATF_TC_HEAD(sem, tc)
{  

	atf_tc_set_md_var(tc, "timeout", "3");
	atf_tc_set_md_var(tc, "descr", "Checks sysvmsg passing");
}

ATF_TC_BODY(sem, tc)
{
	struct sigaction sa;
	union semun sun;
	struct semid_ds s_ds;
	sigset_t sigmask;
	int i;
	int c_status;

	/*
	 * Install a SIGSYS handler so that we can exit gracefully if
	 * System V Semaphore support isn't in the kernel.
	 */
	did_sigsys = 0;
	sa.sa_handler = sigsys_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ATF_REQUIRE_MSG(sigaction(SIGSYS, &sa, NULL) != -1,
	    "sigaction SIGSYS: %d", errno);

	/*
	 * Install a SIGCHLD handler to deal with all possible exit
	 * conditions of the receiver.
	 */
	did_sigchild = 0;
	child_count = 0;
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ATF_REQUIRE_MSG(sigaction(SIGCHLD, &sa, NULL) != -1,
	    "sigaction SIGCHLD: %d", errno);

	semkey = get_ftok(4160);
	ATF_REQUIRE_MSG(semkey != (key_t)-1, "get_ftok failed");

	sender_semid = semget(semkey, 1, IPC_CREAT | 0640);
	ATF_REQUIRE_MSG(sender_semid != -1, "semget: %d", errno);

	if (did_sigsys) {
		atf_tc_skip("SYSV Semaphore not supported");
		return;
	}
	
	sun.buf = &s_ds;
	ATF_REQUIRE_MSG(semctl(sender_semid, 0, IPC_STAT, sun) != -1,
	    "semctl IPC_STAT: %d", errno);

	print_semid_ds(&s_ds, 0640);

	s_ds.sem_perm.mode = (s_ds.sem_perm.mode & ~0777) | 0600;

	sun.buf = &s_ds;
	ATF_REQUIRE_MSG(semctl(sender_semid, 0, IPC_SET, sun) != -1,
	    "semctl IPC_SET: %d", errno);

	memset(&s_ds, 0, sizeof(s_ds));

	sun.buf = &s_ds;
	ATF_REQUIRE_MSG(semctl(sender_semid, 0, IPC_STAT, sun) != -1,
	    "semctl IPC_STAT: %d", errno);

	ATF_REQUIRE_MSG((s_ds.sem_perm.mode & 0777) == 0600,
	    "IPC_SET of mode didn't hold");

	print_semid_ds(&s_ds, 0600);

	for (child_count = 0; child_count < 5; child_count++) {
		switch ((child_pid = fork())) {
		case -1:
			atf_tc_fail("fork: %d", errno);
			return;

		case 0:
			waiter();
			break;

		default:
			break;
		}
	}

	/*
	 * Wait for all of the waiters to be attempting to acquire the
	 * semaphore.
	 */
	for (;;) {
		i = semctl(sender_semid, 0, GETNCNT);
		if (i == -1)
			atf_tc_fail("semctl GETNCNT: %d", i);
		if (i == 5)
			break;
	}

	/*
	 * Now set the thundering herd in motion by initializing the
	 * semaphore to the value 1.
	 */
	sun.val = 1;
	ATF_REQUIRE_MSG(semctl(sender_semid, 0, SETVAL, sun) != -1,
	    "sender: semctl SETVAL to 1: %d", errno);

	/*
	 * Wait for all children to finish
	 */
	sigemptyset(&sigmask);
	for (;;) {
		(void) sigsuspend(&sigmask);
		if (did_sigchild) {
			c_status = child_status;
			if (c_status < 0)
				atf_tc_fail("waitpid: %d", -c_status);
			else if (WIFEXITED(c_status) == 0)
				atf_tc_fail("c abnormal exit: %d", c_status);
			else if (WEXITSTATUS(c_status) != 0)
				atf_tc_fail("c status: %d",
				    WEXITSTATUS(c_status));
			else {
				sun.buf = &s_ds;
				ATF_REQUIRE_MSG(semctl(sender_semid, 0,
						    IPC_STAT, sun) != -1,
				    "semctl IPC_STAT: %d", errno);

				print_semid_ds(&s_ds, 0600);
				atf_tc_pass();
			}
			if (child_count <= 0)
				break;
			did_sigchild = 0;
		} else {
			atf_tc_fail("sender: received unexpected signal");
			break;
		}
	}
}

ATF_TC_CLEANUP(sem, tc)
{

	/*
	 * Remove the semaphore if it exists
	 */
	if (sender_semid != -1)
		ATF_REQUIRE_MSG(semctl(sender_semid, 0, IPC_RMID) != -1,
		    "semctl IPC_RMID: %d", errno);
	sender_semid = -1;
}

void
print_semid_ds(sp, mode)
	struct semid_ds *sp;
	mode_t mode;
{
	uid_t uid = geteuid();
	gid_t gid = getegid();

	printf("PERM: uid %d, gid %d, cuid %d, cgid %d, mode 0%o\n",
	    sp->sem_perm.uid, sp->sem_perm.gid,
	    sp->sem_perm.cuid, sp->sem_perm.cgid,
	    sp->sem_perm.mode & 0777);

	printf("nsems %u\n", sp->sem_nsems);

	printf("otime: %s", ctime(&sp->sem_otime));
	printf("ctime: %s", ctime(&sp->sem_ctime));

	/*
	 * Sanity check a few things.
	 */

	ATF_REQUIRE_MSG(sp->sem_perm.uid == uid && sp->sem_perm.cuid == uid,
	    "uid mismatch");

	ATF_REQUIRE_MSG(sp->sem_perm.gid == gid && sp->sem_perm.cgid == gid,
	    "gid mismatch");

	ATF_REQUIRE_MSG((sp->sem_perm.mode & 0777) == mode,
	    "mode mismatch %o != %o", (sp->sem_perm.mode & 0777), mode);
}

void
waiter()
{
	struct sembuf s;
	int semid;

	if ((semid = semget(semkey, 1, 0)) == -1)
		err(1, "waiter: semget");

	/*
	 * Attempt to acquire the semaphore.
	 */
	s.sem_num = 0;
	s.sem_op = -1;
	s.sem_flg = SEM_UNDO;

	if (semop(semid, &s, 1) == -1)
		err(1, "waiter: semop -1");

	printf("WOO!  GOT THE SEMAPHORE!\n");
	sleep(1);

	/*
	 * Release the semaphore and exit.
	 */
	s.sem_num = 0;
	s.sem_op = 1;
	s.sem_flg = SEM_UNDO;

	if (semop(semid, &s, 1) == -1)
		err(1, "waiter: semop +1");

	exit(0);
}

/*
 * Test the SVID-compatible Shared Memory facility.
 */

ATF_TC_WITH_CLEANUP(shm);
ATF_TC_HEAD(shm, tc)
{  

	atf_tc_set_md_var(tc, "timeout", "3");
	atf_tc_set_md_var(tc, "descr", "Checks sysv shared memory");
}

ATF_TC_BODY(shm, tc)
{
	struct sigaction sa;
	struct shmid_ds s_ds;
	sigset_t sigmask;
	char *shm_buf;
	int c_status;

	/*
	 * Install a SIGSYS handler so that we can exit gracefully if
	 * System V Shared Memory support isn't in the kernel.
	 */
	did_sigsys = 0;
	sa.sa_handler = sigsys_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ATF_REQUIRE_MSG(sigaction(SIGSYS, &sa, NULL) != -1,
	    "sigaction SIGSYS: %d", errno);

	/*
	 * Install a SIGCHLD handler to deal with all possible exit
	 * conditions of the sharer.
	 */
	did_sigchild = 0;
	child_count = 0;
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ATF_REQUIRE_MSG(sigaction(SIGCHLD, &sa, NULL) != -1,
	    "sigaction SIGCHLD: %d", errno);

	pgsize = sysconf(_SC_PAGESIZE);

	shmkey = get_ftok(4160);
	ATF_REQUIRE_MSG(shmkey != (key_t)-1, "get_ftok failed");

	ATF_REQUIRE_MSG((sender_shmid = shmget(shmkey, pgsize,
					       IPC_CREAT | 0640)) != -1,
	    "shmget: %d", errno);

	ATF_REQUIRE_MSG(shmctl(sender_shmid, IPC_STAT, &s_ds) != -1,
	    "shmctl IPC_STAT: %d", errno);

	print_shmid_ds(&s_ds, 0640);

	s_ds.shm_perm.mode = (s_ds.shm_perm.mode & ~0777) | 0600;

	ATF_REQUIRE_MSG(shmctl(sender_shmid, IPC_SET, &s_ds) != -1,
	    "shmctl IPC_SET: %d", errno);

	memset(&s_ds, 0, sizeof(s_ds));

	ATF_REQUIRE_MSG(shmctl(sender_shmid, IPC_STAT, &s_ds) != -1,
	    "shmctl IPC_STAT: %d", errno);

	ATF_REQUIRE_MSG((s_ds.shm_perm.mode & 0777) == 0600,
	    "IPC_SET of mode didn't hold");

	print_shmid_ds(&s_ds, 0600);

	shm_buf = shmat(sender_shmid, NULL, 0);
	ATF_REQUIRE_MSG(shm_buf != (void *) -1, "sender: shmat: %d", errno);

	/*
	 * Write the test pattern into the shared memory buffer.
	 */
	strcpy(shm_buf, m2_str);

	switch ((child_pid = fork())) {
	case -1:
		atf_tc_fail("fork: %d", errno);
		return;

	case 0:
		sharer();
		break;

	default:
		break;
	}

	/*
	 * Wait for child to finish
	 */
	sigemptyset(&sigmask);
	(void) sigsuspend(&sigmask);

	if (did_sigchild) {
		c_status = child_status;
		if (c_status < 0)
			atf_tc_fail("waitpid: %d", -c_status);
		else if (WIFEXITED(c_status) == 0)
			atf_tc_fail("c abnormal exit: %d", c_status);
		else if (WEXITSTATUS(c_status) != 0)
			atf_tc_fail("c status: %d", WEXITSTATUS(c_status));
		else {
			ATF_REQUIRE_MSG(shmctl(sender_shmid, IPC_STAT,
					       &s_ds) != -1,
			    "shmctl IPC_STAT: %d", errno);

			print_shmid_ds(&s_ds, 0600);
			atf_tc_pass();
		}
	} else
		atf_tc_fail("sender: received unexpected signal");
}

ATF_TC_CLEANUP(shm, tc)
{

	/*
	 * Remove the shared memory area if it exists.
	 */
	if (sender_shmid != -1)
		ATF_REQUIRE_MSG(shmctl(sender_shmid, IPC_RMID, NULL) != -1,
		    "shmctl IPC_RMID: %d", errno);
	sender_shmid = -1;
}

void
print_shmid_ds(sp, mode)
	struct shmid_ds *sp;
	mode_t mode;
{
	uid_t uid = geteuid();
	gid_t gid = getegid();

	printf("PERM: uid %d, gid %d, cuid %d, cgid %d, mode 0%o\n",
	    sp->shm_perm.uid, sp->shm_perm.gid,
	    sp->shm_perm.cuid, sp->shm_perm.cgid,
	    sp->shm_perm.mode & 0777);

	printf("segsz %lu, lpid %d, cpid %d, nattch %u\n",
	    (u_long)sp->shm_segsz, sp->shm_lpid, sp->shm_cpid,
	    sp->shm_nattch);

	printf("atime: %s", ctime(&sp->shm_atime));
	printf("dtime: %s", ctime(&sp->shm_dtime));
	printf("ctime: %s", ctime(&sp->shm_ctime));

	/*
	 * Sanity check a few things.
	 */

	ATF_REQUIRE_MSG(sp->shm_perm.uid == uid && sp->shm_perm.cuid == uid,
	    "uid mismatch");

	ATF_REQUIRE_MSG(sp->shm_perm.gid == gid && sp->shm_perm.cgid == gid,
	    "gid mismatch");

	ATF_REQUIRE_MSG((sp->shm_perm.mode & 0777) == mode, "mode mismatch");
}

void
sharer()
{
	int shmid;
	void *shm_buf;

	shmid = shmget(shmkey, pgsize, 0);
	ATF_REQUIRE_MSG(shmid != -1, "receiver: shmget:%d", errno);

	shm_buf = shmat(shmid, NULL, 0);
	ATF_REQUIRE_MSG(shm_buf != (void *) -1, "receiver: shmat: %d", errno);

	printf("%s\n", (const char *)shm_buf);
	
	ATF_REQUIRE_MSG(strcmp((const char *)shm_buf, m2_str) == 0,
	    "receiver: data isn't correct");

	exit(0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, msg); 
	ATF_TP_ADD_TC(tp, sem); 
	ATF_TP_ADD_TC(tp, shm); 

	return atf_no_error();
}

