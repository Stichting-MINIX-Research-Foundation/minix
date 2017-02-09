/*	$NetBSD: sysv_ipc_50.c,v 1.3 2015/05/10 07:41:15 pgoyette Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: sysv_ipc_50.c,v 1.3 2015/05/10 07:41:15 pgoyette Exp $");

#ifdef _KERNEL_OPT
#include "opt_sysv.h"
#include "opt_compat_netbsd.h"
#endif

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
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

#ifdef COMPAT_50
#include <compat/sys/ipc.h>
#ifdef SYSVMSG
#include <compat/sys/msg.h>
#endif
#ifdef SYSVSEM
#include <compat/sys/sem.h>
#endif
#ifdef SYSVSHM
#include <compat/sys/shm.h>
#endif

/*
 * Check for ipc permission
 */

int sysctl_kern_sysvipc50(SYSCTLFN_PROTO);

int
sysctl_kern_sysvipc50(SYSCTLFN_ARGS)
{
	void *where = oldp;
	size_t *sizep = oldlenp;
#ifdef SYSVMSG
	struct msg_sysctl_info50 *msgsi = NULL;
#endif
#ifdef SYSVSEM
	struct sem_sysctl_info50 *semsi = NULL;
#endif
#ifdef SYSVSHM
	struct shm_sysctl_info50 *shmsi = NULL;
#endif
	size_t infosize, dssize, tsize, buflen;
	void *bf = NULL;
	char *start;
	int32_t nds;
	int i, error, ret;

	if (namelen != 1)
		return EINVAL;

	start = where;
	buflen = *sizep;

	switch (*name) {
	case KERN_SYSVIPC_OMSG_INFO:
#ifdef SYSVMSG
		infosize = sizeof(msgsi->msginfo);
		nds = msginfo.msgmni;
		dssize = sizeof(msgsi->msgids[0]);
		break;
#else
		return EINVAL;
#endif
	case KERN_SYSVIPC_OSEM_INFO:
#ifdef SYSVSEM
		infosize = sizeof(semsi->seminfo);
		nds = seminfo.semmni;
		dssize = sizeof(semsi->semids[0]);
		break;
#else
		return EINVAL;
#endif
	case KERN_SYSVIPC_OSHM_INFO:
#ifdef SYSVSHM
		infosize = sizeof(shmsi->shminfo);
		nds = shminfo.shmmni;
		dssize = sizeof(shmsi->shmids[0]);
		break;
#else
		return EINVAL;
#endif
	default:
		return EPASSTHROUGH;
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
	bf = malloc(min(tsize, buflen), M_TEMP, M_WAITOK | M_ZERO);

	switch (*name) {
#ifdef SYSVMSG
	case KERN_SYSVIPC_OMSG_INFO:
		msgsi = (struct msg_sysctl_info50 *)bf;
		msgsi->msginfo = msginfo;
		break;
#endif
#ifdef SYSVSEM
	case KERN_SYSVIPC_OSEM_INFO:
		semsi = (struct sem_sysctl_info50 *)bf;
		semsi->seminfo = seminfo;
		break;
#endif
#ifdef SYSVSHM
	case KERN_SYSVIPC_OSHM_INFO:
		shmsi = (struct shm_sysctl_info50 *)bf;
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
			case KERN_SYSVIPC_OMSG_INFO:
				mutex_enter(&msgmutex);
				SYSCTL_FILL_MSG(msqs[i].msq_u, msgsi->msgids[i]);
				mutex_exit(&msgmutex);
				break;
#endif
#ifdef SYSVSEM
			case KERN_SYSVIPC_OSEM_INFO:
				SYSCTL_FILL_SEM(sema[i], semsi->semids[i]);
				break;
#endif
#ifdef SYSVSHM
			case KERN_SYSVIPC_OSHM_INFO:
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
		free(bf, M_TEMP);
	return error;
}
#endif
