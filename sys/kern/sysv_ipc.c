/*	$NetBSD: sysv_ipc.c,v 1.28 2015/05/13 02:06:25 pgoyette Exp $	*/

/*-
 * Copyright (c) 1998, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysv_ipc.c,v 1.28 2015/05/13 02:06:25 pgoyette Exp $");

#ifdef _KERNEL_OPT
#include "opt_sysv.h"
#endif

#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/syscallvar.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ipc.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

static int (*kern_sysvipc50_sysctl_p)(SYSCTLFN_ARGS);

/*
 * Values in support of System V compatible shared memory.	XXX
 * (originally located in sys/conf/param.c)
 */
#ifdef SYSVSHM
#if !defined(SHMMAX) && defined(SHMMAXPGS)
#define	SHMMAX	SHMMAXPGS	/* shminit() performs a `*= PAGE_SIZE' */
#elif !defined(SHMMAX)
#define SHMMAX 0
#endif
#ifndef	SHMMIN
#define	SHMMIN	1
#endif
#ifndef	SHMMNI
#define	SHMMNI	128		/* <64k, see IPCID_TO_IX in ipc.h */
#endif
#ifndef	SHMSEG
#define	SHMSEG	128
#endif

struct	shminfo shminfo = {
	SHMMAX,
	SHMMIN,
	SHMMNI,
	SHMSEG,
	0
};
#endif

/*
 * Values in support of System V compatible semaphores.
 */
#ifdef SYSVSEM
struct	seminfo seminfo = {
	SEMMAP,		/* # of entries in semaphore map */
	SEMMNI,		/* # of semaphore identifiers */
	SEMMNS,		/* # of semaphores in system */
	SEMMNU,		/* # of undo structures in system */
	SEMMSL,		/* max # of semaphores per id */
	SEMOPM,		/* max # of operations per semop call */
	SEMUME,		/* max # of undo entries per process */
	SEMUSZ,		/* size in bytes of undo structure */
	SEMVMX,		/* semaphore maximum value */
	SEMAEM		/* adjust on exit max value */
};
#endif

/*
 * Values in support of System V compatible messages.
 */
#ifdef SYSVMSG
struct	msginfo msginfo = {
	MSGMAX,		/* max chars in a message */
	MSGMNI,		/* # of message queue identifiers */
	MSGMNB,		/* max chars in a queue */
	MSGTQL,		/* max messages in system */
	MSGSSZ,		/* size of a message segment */
			/* (must be small power of 2 greater than 4) */
	MSGSEG		/* number of message segments */
};
#endif

MODULE(MODULE_CLASS_EXEC, sysv_ipc, NULL);
 
#ifdef _MODULE
SYSCTL_SETUP_PROTO(sysctl_ipc_setup);
SYSCTL_SETUP_PROTO(sysctl_ipc_shm_setup);
SYSCTL_SETUP_PROTO(sysctl_ipc_sem_setup);
SYSCTL_SETUP_PROTO(sysctl_ipc_msg_setup);

static struct sysctllog *sysctl_sysvipc_clog = NULL;
#endif

static const struct syscall_package sysvipc_syscalls[] = {
	{ SYS___shmctl50, 0, (sy_call_t *)sys___shmctl50 },
	{ SYS_shmat, 0, (sy_call_t *)sys_shmat },
	{ SYS_shmdt, 0, (sy_call_t *)sys_shmdt },
	{ SYS_shmget, 0, (sy_call_t *)sys_shmget },
	{ SYS_____semctl50, 0, (sy_call_t *)sys_____semctl50 },
	{ SYS_semget, 0, (sy_call_t *)sys_semget },
	{ SYS_semop, 0, (sy_call_t *)sys_semop },
	{ SYS_semconfig, 0, (sy_call_t *)sys_semconfig },
	{ SYS___msgctl50, 0, (sy_call_t *)sys___msgctl50 },
	{ SYS_msgget, 0, (sy_call_t *)sys_msgget },
	{ SYS_msgsnd, 0, (sy_call_t *)sys_msgsnd },
	{ SYS_msgrcv, 0, (sy_call_t *)sys_msgrcv },
	{ 0, 0, NULL }
};

static int
sysv_ipc_modcmd(modcmd_t cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
		/* Link the system calls */
		error = syscall_establish(NULL, sysvipc_syscalls);

		/* Initialize all sysctl sub-trees */
#ifdef _MODULE
		sysctl_ipc_setup(&sysctl_sysvipc_clog);
#ifdef	SYSVMSG
		sysctl_ipc_msg_setup(&sysctl_sysvipc_clog);
#endif
#ifdef	SYSVSHM
		sysctl_ipc_shm_setup(&sysctl_sysvipc_clog);
#endif
#ifdef	SYSVSEM
		sysctl_ipc_sem_setup(&sysctl_sysvipc_clog);
#endif
		/* Assume no compat sysctl routine for now */
		kern_sysvipc50_sysctl_p = NULL;

		/* Initialize each sub-component */
#ifdef SYSVSHM
		shminit();
#endif
#ifdef SYSVSEM
		seminit();
#endif
#ifdef SYSVMSG
		msginit();
#endif
#endif /* _MODULE */
		break;
	case MODULE_CMD_FINI:
		/*
		 * Make sure no subcomponents are active.  Each one
		 * tells us if it is busy, and if it was _not_ busy,
		 * we assume it has already done its own clean-up.
		 * So we might need to re-init any components that
		 * are successfully fini'd if we find one that is 
		 * still busy.
		 */
#ifdef SYSVSHM
		if (shmfini()) {
			return EBUSY;
		}
#endif
#ifdef SYSVSEM
		if (semfini()) {
#ifdef SYSVSHM
			shminit();
#endif
			return EBUSY;
		}
#endif
#ifdef SYSVMSG
		if (msgfini()) {
#ifdef SYSVSEM
			seminit();
#endif
#ifdef SYSVSHM
			shminit();
#endif
			return EBUSY;
		}
#endif

		/* Unlink the system calls. */
		error = syscall_disestablish(NULL, sysvipc_syscalls);
		if (error)
			return error;

#ifdef _MODULE
		/* Remove the sysctl sub-trees */
		sysctl_teardown(&sysctl_sysvipc_clog);
#endif  

		/* Remove the kauth listener */
		sysvipcfini();
		break;
	default:
		return ENOTTY;
	}
	return error;
}

void
sysvipc50_set_compat_sysctl(int (*compat_sysctl)(SYSCTLFN_PROTO))
{

	kern_sysvipc50_sysctl_p = compat_sysctl;
}

static kauth_listener_t sysvipc_listener = NULL;

static int
sysvipc_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	mode_t mask;
	int ismember = 0;
	struct ipc_perm *perm;
	int mode;
	enum kauth_system_req req;

	req = (enum kauth_system_req)arg0;

	if (!(action == KAUTH_SYSTEM_SYSVIPC &&
	      req == KAUTH_REQ_SYSTEM_SYSVIPC_BYPASS))
		return KAUTH_RESULT_DEFER;

	perm = arg1;
	mode = (int)(uintptr_t)arg2;

	if (mode == IPC_M) {
		if (kauth_cred_geteuid(cred) == perm->uid ||
		    kauth_cred_geteuid(cred) == perm->cuid)
			return (KAUTH_RESULT_ALLOW);
		return (KAUTH_RESULT_DEFER); /* EPERM */
	}

	mask = 0;

	if (kauth_cred_geteuid(cred) == perm->uid ||
	    kauth_cred_geteuid(cred) == perm->cuid) {
		if (mode & IPC_R)
			mask |= S_IRUSR;
		if (mode & IPC_W)
			mask |= S_IWUSR;
		return ((perm->mode & mask) == mask ? KAUTH_RESULT_ALLOW : KAUTH_RESULT_DEFER /* EACCES */);
	}

	if (kauth_cred_getegid(cred) == perm->gid ||
	    (kauth_cred_ismember_gid(cred, perm->gid, &ismember) == 0 && ismember) ||
	    kauth_cred_getegid(cred) == perm->cgid ||
	    (kauth_cred_ismember_gid(cred, perm->cgid, &ismember) == 0 && ismember)) {
		if (mode & IPC_R)
			mask |= S_IRGRP;
		if (mode & IPC_W)
			mask |= S_IWGRP;
		return ((perm->mode & mask) == mask ? KAUTH_RESULT_ALLOW : KAUTH_RESULT_DEFER /* EACCES */);
	}

	if (mode & IPC_R)
		mask |= S_IROTH;
	if (mode & IPC_W)
		mask |= S_IWOTH;
	return ((perm->mode & mask) == mask ? KAUTH_RESULT_ALLOW : KAUTH_RESULT_DEFER /* EACCES */);
}

/*
 * Check for ipc permission
 */

int
ipcperm(kauth_cred_t cred, struct ipc_perm *perm, int mode)
{
	int error;

	error = kauth_authorize_system(cred, KAUTH_SYSTEM_SYSVIPC,
	    KAUTH_REQ_SYSTEM_SYSVIPC_BYPASS, perm, KAUTH_ARG(mode), NULL);
	if (error == 0)
		return (0);

	/* Adjust EPERM and EACCES errors until there's a better way to do this. */
	if (mode != IPC_M)
		error = EACCES;

	return error;
}

void
sysvipcfini(void)
{

	KASSERT(sysvipc_listener != NULL);
	kauth_unlisten_scope(sysvipc_listener);
}

void
sysvipcinit(void)
{

	if (sysvipc_listener != NULL)
		return;

	sysvipc_listener = kauth_listen_scope(KAUTH_SCOPE_SYSTEM,
	    sysvipc_listener_cb, NULL);
}

static int
sysctl_kern_sysvipc(SYSCTLFN_ARGS)
{
	void *where = oldp;
	size_t sz, *sizep = oldlenp;
#ifdef SYSVMSG
	struct msg_sysctl_info *msgsi = NULL;
#endif
#ifdef SYSVSEM
	struct sem_sysctl_info *semsi = NULL;
#endif
#ifdef SYSVSHM
	struct shm_sysctl_info *shmsi = NULL;
#endif
	size_t infosize, dssize, tsize, buflen;
	void *bf = NULL;
	char *start;
	int32_t nds;
	int i, error, ret;

/*
 * If compat_sysv module has loaded the compat sysctl, call it.  If
 * it handles the request completely (either success or error), just
 * return.  Otherwise fallthrough to the non-compat_sysv sysctl code.
 */
	if (kern_sysvipc50_sysctl_p != NULL) {
		error = (*kern_sysvipc50_sysctl_p)(SYSCTLFN_CALL(rnode));
		if (error != EPASSTHROUGH)
			return error;
	}

	if (namelen != 1)
		return EINVAL;

	start = where;
	buflen = *sizep;

	switch (*name) {
	case KERN_SYSVIPC_MSG_INFO:
#ifdef SYSVMSG
		infosize = sizeof(msgsi->msginfo);
		nds = msginfo.msgmni;
		dssize = sizeof(msgsi->msgids[0]);
		break;
#else
		return EINVAL;
#endif
	case KERN_SYSVIPC_SEM_INFO:
#ifdef SYSVSEM
		infosize = sizeof(semsi->seminfo);
		nds = seminfo.semmni;
		dssize = sizeof(semsi->semids[0]);
		break;
#else
		return EINVAL;
#endif
	case KERN_SYSVIPC_SHM_INFO:
#ifdef SYSVSHM
		infosize = sizeof(shmsi->shminfo);
		nds = shminfo.shmmni;
		dssize = sizeof(shmsi->shmids[0]);
		break;
#else
		return EINVAL;
#endif
	default:
		return EINVAL;
	}
	/*
	 * Round infosize to 64 bit boundary if requesting more than just
	 * the info structure or getting the total data size.
	 */
	if (where == NULL || *sizep > infosize)
		infosize = roundup(infosize, sizeof(quad_t));
	tsize = infosize + nds * dssize;

	/* Return just the total size required. */
	if (where == NULL) {
		*sizep = tsize;
		return 0;
	}

	/* Not enough room for even the info struct. */
	if (buflen < infosize) {
		*sizep = 0;
		return ENOMEM;
	}
	sz = min(tsize, buflen);
	bf = kmem_zalloc(sz, KM_SLEEP);

	switch (*name) {
#ifdef SYSVMSG
	case KERN_SYSVIPC_MSG_INFO:
		msgsi = (struct msg_sysctl_info *)bf;
		msgsi->msginfo = msginfo;
		break;
#endif
#ifdef SYSVSEM
	case KERN_SYSVIPC_SEM_INFO:
		semsi = (struct sem_sysctl_info *)bf;
		semsi->seminfo = seminfo;
		break;
#endif
#ifdef SYSVSHM
	case KERN_SYSVIPC_SHM_INFO:
		shmsi = (struct shm_sysctl_info *)bf;
		shmsi->shminfo = shminfo;
		break;
#endif
	}
	buflen -= infosize;

	ret = 0;
	if (buflen > 0) {
		/* Fill in the IPC data structures.  */
		for (i = 0; i < nds; i++) {
			if (buflen < dssize) {
				ret = ENOMEM;
				break;
			}
			switch (*name) {
#ifdef SYSVMSG
			case KERN_SYSVIPC_MSG_INFO:
				mutex_enter(&msgmutex);
				SYSCTL_FILL_MSG(msqs[i].msq_u, msgsi->msgids[i]);
				mutex_exit(&msgmutex);
				break;
#endif
#ifdef SYSVSEM
			case KERN_SYSVIPC_SEM_INFO:
				SYSCTL_FILL_SEM(sema[i], semsi->semids[i]);
				break;
#endif
#ifdef SYSVSHM
			case KERN_SYSVIPC_SHM_INFO:
				SYSCTL_FILL_SHM(shmsegs[i], shmsi->shmids[i]);
				break;
#endif
			}
			buflen -= dssize;
		}
	}
	*sizep -= buflen;
	error = copyout(bf, start, *sizep);
	/* If copyout succeeded, use return code set earlier. */
	if (error == 0)
		error = ret;
	if (bf)
		kmem_free(bf, sz);
	return error;
}

SYSCTL_SETUP(sysctl_ipc_setup, "sysctl kern.ipc subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "ipc",
		SYSCTL_DESCR("SysV IPC options"),
		NULL, 0, NULL, 0,
		CTL_KERN, KERN_SYSVIPC, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_STRUCT, "sysvipc_info",
		SYSCTL_DESCR("System V style IPC information"),
		sysctl_kern_sysvipc, 0, NULL, 0,
		CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_INFO, CTL_EOL);
}
