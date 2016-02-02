/*	$NetBSD: ipcrm.c,v 1.16 2009/01/18 01:06:42 lukem Exp $	*/

/*
 * Copyright (c) 1994 Adam Glass
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Adam Glass BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <sys/sysctl.h>

#ifdef __minix
#include <errno.h>
#endif /* __minix */

#define IPC_TO_STR(x) (x == 'Q' ? "msq" : (x == 'M' ? "shm" : "sem"))
#define IPC_TO_STRING(x) (x == 'Q' ? "message queue" : \
	(x == 'M' ? "shared memory segment" : "semaphore"))

static sig_atomic_t signaled;

static void	usage(void) __dead;
static int	msgrm(key_t, int);
static int	shmrm(key_t, int);
static int	semrm(key_t, int);
static int	msgrmall(void);
static int	shmrmall(void);
static int	semrmall(void);
static void	not_configured(int);

static void 
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-M shmkey] [-m shmid] [-Q msgkey]\n",
	    getprogname());
	(void)fprintf(stderr, "\t[-q msqid] [-S semkey] [-s semid] ...\n");
	exit(1);
}

static int 
msgrm(key_t key, int id)
{
#ifndef __minix
	if (key) {
		id = msgget(key, 0);
		if (id == -1)
			return -1;
	}
	return msgctl(id, IPC_RMID, NULL);
#else /* __minix */
	errno = ENOSYS;
	return -1;
#endif /* __minix */
}

static int 
shmrm(key_t key, int id)
{
	if (key) {
		id = shmget(key, 0, 0);
		if (id == -1)
			return -1;
	}
	return shmctl(id, IPC_RMID, NULL);
}

static int 
semrm(key_t key, int id)
{

	if (key) {
		id = semget(key, 0, 0);
		if (id == -1)
			return -1;
	}
	return semctl(id, 0, IPC_RMID, NULL);
}

static int
msgrmall(void)
{
	int mib[4];
	struct msg_sysctl_info *msgsi;
	int32_t i;
	size_t len;
	int result = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_INFO;
	mib[3] = KERN_SYSVIPC_MSG_INFO;

	if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1)
		err(1, "sysctl(KERN_SYSVIPC_MSG_INFO)");

	if ((msgsi = malloc(len)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 4, msgsi, &len, NULL, 0) == -1) {
		free(msgsi);
		err(1, "sysctl(KERN_SYSVIPC_MSG_INFO)");
	}

	for (i = 0; i < msgsi->msginfo.msgmni; i++) {
		struct msgid_ds_sysctl *msgptr = &msgsi->msgids[i];
		if (msgptr->msg_qbytes != 0)
			result -= msgrm((key_t)0,
			    (int)IXSEQ_TO_IPCID(i, msgptr->msg_perm));
	}
	free(msgsi);
	return result;
}

static int
shmrmall(void)
{
	int mib[4];
	struct shm_sysctl_info *shmsi;
	size_t i, len;
	int result = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_INFO;
	mib[3] = KERN_SYSVIPC_SHM_INFO;

	if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1)
		err(1, "sysctl(KERN_SYSVIPC_SHM_INFO)");

	if ((shmsi = malloc(len)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 4, shmsi, &len, NULL, 0) == -1) {
		free(shmsi);
		err(1, "sysctl(KERN_SYSVIPC_SHM_INFO)");
	}

	for (i = 0; i < shmsi->shminfo.shmmni; i++) {
		struct shmid_ds_sysctl *shmptr = &shmsi->shmids[i];
		if (shmptr->shm_perm.mode & 0x0800)
			result -= shmrm((key_t)0,
			    (int)IXSEQ_TO_IPCID(i, shmptr->shm_perm));
	}
	free(shmsi);
	return result;
}

static int
semrmall(void)
{
	int mib[4];
	struct sem_sysctl_info *semsi;
	size_t len;
	int32_t i;
	int result = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_INFO;
	mib[3] = KERN_SYSVIPC_SEM_INFO;

	if (sysctl(mib, 4, NULL, &len, NULL, 0) == -1)
		err(1, "sysctl(KERN_SYSVIPC_SEM_INFO)");

	if ((semsi = malloc(len)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 4, semsi, &len, NULL, 0) == -1) {
		free(semsi);
		err(1, "sysctl(KERN_SYSVIPC_SEM_INFO)");
	}

	for (i = 0; i < semsi->seminfo.semmni; i++) {
		struct semid_ds_sysctl *semptr = &semsi->semids[i];
		if ((semptr->sem_perm.mode & SEM_ALLOC) != 0)
			result -= semrm((key_t)0,
			    (int)IXSEQ_TO_IPCID(i, semptr->sem_perm));
	}
	free(semsi);
	return result;
}

static void 
/*ARGSUSED*/
not_configured(int n)
{
	signaled++;
}

int 
main(int argc, char *argv[])
{
	int     c, result, errflg, target_id;
	key_t   target_key;

	setprogname(argv[0]);
	errflg = 0;
	(void)signal(SIGSYS, not_configured);
	while ((c = getopt(argc, argv, "q:m:s:Q:M:S:")) != -1) {
		signaled = 0;
		target_id = 0;
		target_key = 0;
		result = 0;

		if (optarg != NULL && strcmp(optarg, "all") == 0) {
			switch (c) {
			case 'm':
			case 'M':
				result = shmrmall();
				break;
			case 'q':
			case 'Q':
				result = msgrmall();
				break;
			case 's':
			case 'S':
				result = semrmall();
				break;
			default:
				usage();
			}
		} else {
			switch (c) {
			case 'q':
			case 'm':
			case 's':
				target_id = atoi(optarg);
				break;
			case 'Q':
			case 'M':
			case 'S':
				target_key = atol(optarg);
				if (target_key == IPC_PRIVATE) {
					warnx("can't remove private %ss",
					    IPC_TO_STRING(c));
					continue;
				}
				break;
			default:
				usage();
			}
			switch (c) {
			case 'q':
				result = msgrm((key_t)0, target_id);
				break;
			case 'm':
				result = shmrm((key_t)0, target_id);
				break;
			case 's':
				result = semrm((key_t)0, target_id);
				break;
			case 'Q':
				result = msgrm(target_key, 0);
				break;
			case 'M':
				result = shmrm(target_key, 0);
				break;
			case 'S':
				result = semrm(target_key, 0);
				break;
			}
		}
		if (result < 0) {
			if (!signaled) {
				if (target_id) {
					warn("%sid(%d): ",
					    IPC_TO_STR(toupper(c)), target_id);
					errflg++;
				} else if (target_key) {
					warn("%skey(%ld): ", IPC_TO_STR(c),
					    (long)target_key);
					errflg++;
				}
			} else {
				errflg++;
				warnx("%ss are not configured in "
				    "the running kernel",
				    IPC_TO_STRING(toupper(c)));
			}
		}
	}

	if (optind != argc) {
		warnx("Unknown argument: %s", argv[optind]);
		usage();
	}
	return errflg;
}
