/*	$NetBSD: ipcs.c,v 1.43 2014/06/11 14:57:55 joerg Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge.
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
 * Copyright (c) 1994 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/inttypes.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define	SHMINFO		1
#define	SHMTOTAL	2
#define	MSGINFO		4
#define	MSGTOTAL	8
#define	SEMINFO		16
#define	SEMTOTAL	32

#define BIGGEST		1
#define CREATOR		2
#define OUTSTANDING	4
#define PID		8
#define TIME		16

static int	display = 0;
static int	option = 0;

static void	cvt_time(time_t, char *, size_t);
static char    *fmt_perm(u_short);
static void	msg_sysctl(void);
static void	sem_sysctl(void);
static void	shm_sysctl(void);
static void	show_msginfo(time_t, time_t, time_t, int, u_int64_t, mode_t,
    uid_t, gid_t, uid_t, gid_t, u_int64_t, u_int64_t, u_int64_t, pid_t, pid_t);
static void	show_msginfo_hdr(void);
static void	show_msgtotal(struct msginfo *);
static void	show_seminfo_hdr(void);
static void	show_seminfo(time_t, time_t, int, u_int64_t, mode_t, uid_t,
    gid_t, uid_t, gid_t, int16_t);
static void	show_semtotal(struct seminfo *);
static void	show_shminfo(time_t, time_t, time_t, int, u_int64_t, mode_t,
    uid_t, gid_t, uid_t, gid_t, u_int32_t, u_int64_t, pid_t, pid_t);
static void	show_shminfo_hdr(void);
static void	show_shmtotal(struct shminfo *);
static void	usage(void) __dead;
static void	unconfsem(void);
static void	unconfmsg(void);
static void	unconfshm(void);

static void
unconfsem(void)
{
	warnx("SVID semaphores facility not configured in the system");
}

static void
unconfmsg(void)
{
	warnx("SVID messages facility not configured in the system");
}

static void
unconfshm(void)
{
	warnx("SVID shared memory facility not configured in the system");
}

static char *
fmt_perm(u_short mode)
{
	static char buffer[12];

	buffer[0] = '-';
	buffer[1] = '-';
	buffer[2] = ((mode & 0400) ? 'r' : '-');
	buffer[3] = ((mode & 0200) ? 'w' : '-');
	buffer[4] = ((mode & 0100) ? 'a' : '-');
	buffer[5] = ((mode & 0040) ? 'r' : '-');
	buffer[6] = ((mode & 0020) ? 'w' : '-');
	buffer[7] = ((mode & 0010) ? 'a' : '-');
	buffer[8] = ((mode & 0004) ? 'r' : '-');
	buffer[9] = ((mode & 0002) ? 'w' : '-');
	buffer[10] = ((mode & 0001) ? 'a' : '-');
	buffer[11] = '\0';
	return (&buffer[0]);
}

static void
cvt_time(time_t t, char *buf, size_t buflen)
{
	struct tm *tm;

	if (t == 0)
		(void)strlcpy(buf, "no-entry", buflen);
	else {
		tm = localtime(&t);
		(void)snprintf(buf, buflen, "%2d:%02d:%02d",
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	}
}
int
main(int argc, char *argv[])
{
	int i;
	time_t now;

	while ((i = getopt(argc, argv, "MmQqSsabcoptT")) != -1)
		switch (i) {
		case 'M':
			display |= SHMTOTAL;
			break;
		case 'm':
			display |= SHMINFO;
			break;
		case 'Q':
			display |= MSGTOTAL;
			break;
		case 'q':
			display |= MSGINFO;
			break;
		case 'S':
			display |= SEMTOTAL;
			break;
		case 's':
			display |= SEMINFO;
			break;
		case 'T':
			display |= SHMTOTAL | MSGTOTAL | SEMTOTAL;
			break;
		case 'a':
			option |= BIGGEST | CREATOR | OUTSTANDING | PID | TIME;
			break;
		case 'b':
			option |= BIGGEST;
			break;
		case 'c':
			option |= CREATOR;
			break;
		case 'o':
			option |= OUTSTANDING;
			break;
		case 'p':
			option |= PID;
			break;
		case 't':
			option |= TIME;
			break;
		default:
			usage();
		}

	if (argc - optind > 0)
		usage();

	(void)time(&now);
	(void)printf("IPC status from <running system> as of %s\n",
	    /* and extra \n from ctime(3) */
	    ctime(&now));

        if (display == 0)
		display = SHMINFO | MSGINFO | SEMINFO;

	if (display & (MSGINFO | MSGTOTAL))
		msg_sysctl();
	if (display & (SHMINFO | SHMTOTAL))
		shm_sysctl();
	if (display & (SEMINFO | SEMTOTAL))
		sem_sysctl();
	return 0;
}

static void
show_msgtotal(struct msginfo *msginfo)
{
	(void)printf("msginfo:\n");
	(void)printf("\tmsgmax: %6d\t(max characters in a message)\n",
	    msginfo->msgmax);
	(void)printf("\tmsgmni: %6d\t(# of message queues)\n",
	    msginfo->msgmni);
	(void)printf("\tmsgmnb: %6d\t(max characters in a message queue)\n",
	    msginfo->msgmnb);
	(void)printf("\tmsgtql: %6d\t(max # of messages in system)\n",
	    msginfo->msgtql);
	(void)printf("\tmsgssz: %6d\t(size of a message segment)\n",
	    msginfo->msgssz);
	(void)printf("\tmsgseg: %6d\t(# of message segments in system)\n\n",
	    msginfo->msgseg);
}

static void
show_shmtotal(struct shminfo *shminfo)
{
	(void)printf("shminfo:\n");
	(void)printf("\tshmmax: %" PRIu64 "\t(max shared memory segment size)\n",
	    shminfo->shmmax);
	(void)printf("\tshmmin: %7d\t(min shared memory segment size)\n",
	    shminfo->shmmin);
	(void)printf("\tshmmni: %7d\t(max number of shared memory identifiers)\n",
	    shminfo->shmmni);
	(void)printf("\tshmseg: %7d\t(max shared memory segments per process)\n",
	    shminfo->shmseg);
	(void)printf("\tshmall: %7d\t(max amount of shared memory in pages)\n\n",
	    shminfo->shmall);
}

static void
show_semtotal(struct seminfo *seminfo)
{
	(void)printf("seminfo:\n");
	(void)printf("\tsemmap: %6d\t(# of entries in semaphore map)\n",
	    seminfo->semmap);
	(void)printf("\tsemmni: %6d\t(# of semaphore identifiers)\n",
	    seminfo->semmni);
	(void)printf("\tsemmns: %6d\t(# of semaphores in system)\n",
	    seminfo->semmns);
	(void)printf("\tsemmnu: %6d\t(# of undo structures in system)\n",
	    seminfo->semmnu);
	(void)printf("\tsemmsl: %6d\t(max # of semaphores per id)\n",
	    seminfo->semmsl);
	(void)printf("\tsemopm: %6d\t(max # of operations per semop call)\n",
	    seminfo->semopm);
	(void)printf("\tsemume: %6d\t(max # of undo entries per process)\n",
	    seminfo->semume);
	(void)printf("\tsemusz: %6d\t(size in bytes of undo structure)\n",
	    seminfo->semusz);
	(void)printf("\tsemvmx: %6d\t(semaphore maximum value)\n",
	    seminfo->semvmx);
	(void)printf("\tsemaem: %6d\t(adjust on exit max value)\n\n",
	    seminfo->semaem);
}

static void
show_msginfo_hdr(void)
{
	(void)printf("Message Queues:\n");
	(void)printf("T        ID     KEY        MODE       OWNER    GROUP");
	if (option & CREATOR)
		(void)printf("  CREATOR   CGROUP");
	if (option & OUTSTANDING)
		(void)printf(" CBYTES  QNUM");
	if (option & BIGGEST)
		(void)printf(" QBYTES");
	if (option & PID)
		(void)printf(" LSPID LRPID");
	if (option & TIME)
		(void)printf("    STIME    RTIME    CTIME");
	(void)printf("\n");
}

static void
show_msginfo(time_t s_time, time_t r_time, time_t c_time, int ipcid,
    u_int64_t key,
    mode_t mode, uid_t uid, gid_t gid, uid_t cuid, gid_t cgid,
    u_int64_t cbytes, u_int64_t qnum, u_int64_t qbytes, pid_t lspid,
    pid_t lrpid)
{
	char s_time_buf[100], r_time_buf[100], c_time_buf[100];

	if (option & TIME) {
		cvt_time(s_time, s_time_buf, sizeof(s_time_buf));
		cvt_time(r_time, r_time_buf, sizeof(r_time_buf));
		cvt_time(c_time, c_time_buf, sizeof(c_time_buf));
	}

	(void)printf("q %9d %10lld %s %8s %8s", ipcid, (long long)key, fmt_perm(mode),
	    user_from_uid(uid, 0), group_from_gid(gid, 0));

	if (option & CREATOR)
		(void)printf(" %8s %8s", user_from_uid(cuid, 0),
		    group_from_gid(cgid, 0));

	if (option & OUTSTANDING)
		(void)printf(" %6lld %5lld", (long long)cbytes, (long long)qnum);

	if (option & BIGGEST)
		(void)printf(" %6lld", (long long)qbytes);

	if (option & PID)
		(void)printf(" %5d %5d", lspid, lrpid);

	if (option & TIME)
		(void)printf(" %s %s %s", s_time_buf, r_time_buf, c_time_buf);

	(void)printf("\n");
}

static void
show_shminfo_hdr(void)
{
	(void)printf("Shared Memory:\n");
	(void)printf("T        ID     KEY        MODE       OWNER    GROUP");
	if (option & CREATOR)
		(void)printf("  CREATOR   CGROUP");
	if (option & OUTSTANDING)
		(void)printf(" NATTCH");
	if (option & BIGGEST)
		(void)printf("   SEGSZ");
	if (option & PID)
		(void)printf("  CPID  LPID");
	if (option & TIME)
		(void)printf("    ATIME    DTIME    CTIME");
	(void)printf("\n");
}

static void
show_shminfo(time_t atime, time_t dtime, time_t c_time, int ipcid, u_int64_t key,
    mode_t mode, uid_t uid, gid_t gid, uid_t cuid, gid_t cgid,
    u_int32_t nattch, u_int64_t segsz, pid_t cpid, pid_t lpid)
{
	char atime_buf[100], dtime_buf[100], c_time_buf[100];

	if (option & TIME) {
		cvt_time(atime, atime_buf, sizeof(atime_buf));
		cvt_time(dtime, dtime_buf, sizeof(dtime_buf));
		cvt_time(c_time, c_time_buf, sizeof(c_time_buf));
	}

	(void)printf("m %9d %10lld %s %8s %8s", ipcid, (long long)key, fmt_perm(mode),
	    user_from_uid(uid, 0), group_from_gid(gid, 0));

	if (option & CREATOR)
		(void)printf(" %8s %8s", user_from_uid(cuid, 0),
		    group_from_gid(cgid, 0));

	if (option & OUTSTANDING)
		(void)printf(" %6d", nattch);

	if (option & BIGGEST)
		(void)printf(" %7llu", (long long)segsz);

	if (option & PID)
		(void)printf(" %5d %5d", cpid, lpid);

	if (option & TIME)
		(void)printf(" %s %s %s",
		    atime_buf,
		    dtime_buf,
		    c_time_buf);

	(void)printf("\n");
}

static void
show_seminfo_hdr(void)
{
	(void)printf("Semaphores:\n");
	(void)printf("T        ID     KEY        MODE       OWNER    GROUP");
	if (option & CREATOR)
		(void)printf("  CREATOR   CGROUP");
	if (option & BIGGEST)
		(void)printf(" NSEMS");
	if (option & TIME)
		(void)printf("    OTIME    CTIME");
	(void)printf("\n");
}

static void
show_seminfo(time_t otime, time_t c_time, int ipcid, u_int64_t key, mode_t mode,
    uid_t uid, gid_t gid, uid_t cuid, gid_t cgid, int16_t nsems)
{
	char c_time_buf[100], otime_buf[100];

	if (option & TIME) {
		cvt_time(otime, otime_buf, sizeof(otime_buf));
		cvt_time(c_time, c_time_buf, sizeof(c_time_buf));
	}

	(void)printf("s %9d %10lld %s %8s %8s", ipcid, (long long)key, fmt_perm(mode),
	    user_from_uid(uid, 0), group_from_gid(gid, 0));

	if (option & CREATOR)
		(void)printf(" %8s %8s", user_from_uid(cuid, 0),
		    group_from_gid(cgid, 0));

	if (option & BIGGEST)
		(void)printf(" %5d", nsems);

	if (option & TIME)
		(void)printf(" %s %s", otime_buf, c_time_buf);

	(void)printf("\n");
}

static void
msg_sysctl(void)
{
	struct msg_sysctl_info *msgsi;
	void *buf;
	int mib[4];
	size_t len;
	int i, valid;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_MSG;
	len = sizeof(valid);
	if (sysctl(mib, 3, &valid, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_MSG)");
		return;
	}
	if (!valid) {
		unconfmsg();
		return;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_INFO;
	mib[3] = KERN_SYSVIPC_MSG_INFO;

	if (!(display & MSGINFO)) {
		/* totals only */
		len = sizeof(struct msginfo);
	} else {
		if (sysctl(mib, 4, NULL, &len, NULL, 0) < 0) {
			warn("sysctl(KERN_SYSVIPC_MSG_INFO)");
			return;
		}
	}

	if ((buf = malloc(len)) == NULL)
		err(1, "malloc");
	msgsi = (struct msg_sysctl_info *)buf;
	if (sysctl(mib, 4, msgsi, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_MSG_INFO)");
		goto done;
	}

	if (display & MSGTOTAL)
		show_msgtotal(&msgsi->msginfo);

	if (display & MSGINFO) {
		show_msginfo_hdr();
		for (i = 0; i < msgsi->msginfo.msgmni; i++) {
			struct msgid_ds_sysctl *msqptr = &msgsi->msgids[i];
			if (msqptr->msg_qbytes != 0)
				show_msginfo(msqptr->msg_stime,
				    msqptr->msg_rtime,
				    msqptr->msg_ctime,
				    IXSEQ_TO_IPCID(i, msqptr->msg_perm),
				    msqptr->msg_perm._key,
				    msqptr->msg_perm.mode,
				    msqptr->msg_perm.uid,
				    msqptr->msg_perm.gid,
				    msqptr->msg_perm.cuid,
				    msqptr->msg_perm.cgid,
				    msqptr->_msg_cbytes,
				    msqptr->msg_qnum,
				    msqptr->msg_qbytes,
				    msqptr->msg_lspid,
				    msqptr->msg_lrpid);
		}
		(void)printf("\n");
	}
done:
	free(buf);
}

static void
shm_sysctl(void)
{
	struct shm_sysctl_info *shmsi;
	void *buf;
	int mib[4];
	size_t len;
	uint32_t i;
	long valid;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_SHM;
	len = sizeof(valid);
	if (sysctl(mib, 3, &valid, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_SHM)");
		return;
	}
	if (!valid) {
		unconfshm();
		return;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_INFO;
	mib[3] = KERN_SYSVIPC_SHM_INFO;

	if (!(display & SHMINFO)) {
		/* totals only */
		len = sizeof(struct shminfo);
	} else {
		if (sysctl(mib, 4, NULL, &len, NULL, 0) < 0) {
			warn("sysctl(KERN_SYSVIPC_SHM_INFO)");
			return;
		}
	}

	if ((buf = malloc(len)) == NULL)
		err(1, "malloc");
	shmsi = (struct shm_sysctl_info *)buf;
	if (sysctl(mib, 4, shmsi, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_SHM_INFO)");
		goto done;
	}

	if (display & SHMTOTAL)
		show_shmtotal(&shmsi->shminfo);

	if (display & SHMINFO) {
		show_shminfo_hdr();
		for (i = 0; i < shmsi->shminfo.shmmni; i++) {
			struct shmid_ds_sysctl *shmptr = &shmsi->shmids[i];
			if (shmptr->shm_perm.mode & 0x0800)
				show_shminfo(shmptr->shm_atime,
				    shmptr->shm_dtime,
				    shmptr->shm_ctime,
				    IXSEQ_TO_IPCID(i, shmptr->shm_perm),
				    shmptr->shm_perm._key,
				    shmptr->shm_perm.mode,
				    shmptr->shm_perm.uid,
				    shmptr->shm_perm.gid,
				    shmptr->shm_perm.cuid,
				    shmptr->shm_perm.cgid,
				    shmptr->shm_nattch,
				    shmptr->shm_segsz,
				    shmptr->shm_cpid,
				    shmptr->shm_lpid);
		}
		(void)printf("\n");
	}
done:
	free(buf);
}

static void
sem_sysctl(void)
{
	struct sem_sysctl_info *semsi;
	void *buf;
	int mib[4];
	size_t len;
	int i, valid;

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_SEM;
	len = sizeof(valid);
	if (sysctl(mib, 3, &valid, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_SEM)");
		return;
	}
	if (!valid) {
		unconfsem();
		return;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_SYSVIPC;
	mib[2] = KERN_SYSVIPC_INFO;
	mib[3] = KERN_SYSVIPC_SEM_INFO;

	if (!(display & SEMINFO)) {
		/* totals only */
		len = sizeof(struct seminfo);
	} else {
		if (sysctl(mib, 4, NULL, &len, NULL, 0) < 0) {
			warn("sysctl(KERN_SYSVIPC_SEM_INFO)");
			return;
		}
	}

	if ((buf = malloc(len)) == NULL)
		err(1, "malloc");
	semsi = (struct sem_sysctl_info *)buf;
	if (sysctl(mib, 4, semsi, &len, NULL, 0) < 0) {
		warn("sysctl(KERN_SYSVIPC_SEM_INFO)");
		goto done;
	}

	if (display & SEMTOTAL)
		show_semtotal(&semsi->seminfo);

	if (display & SEMINFO) {
		show_seminfo_hdr();
		for (i = 0; i < semsi->seminfo.semmni; i++) {
			struct semid_ds_sysctl *semaptr = &semsi->semids[i];
			if ((semaptr->sem_perm.mode & SEM_ALLOC) != 0)
				show_seminfo(semaptr->sem_otime,
				    semaptr->sem_ctime,
				    IXSEQ_TO_IPCID(i, semaptr->sem_perm),
				    semaptr->sem_perm._key,
				    semaptr->sem_perm.mode,
				    semaptr->sem_perm.uid,
				    semaptr->sem_perm.gid,
				    semaptr->sem_perm.cuid,
				    semaptr->sem_perm.cgid,
				    semaptr->sem_nsems);
		}
		(void)printf("\n");
	}
done:
	free(buf);
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "Usage: %s [-abcmopqstMQST]\n",
	    getprogname());
	exit(1);
}
