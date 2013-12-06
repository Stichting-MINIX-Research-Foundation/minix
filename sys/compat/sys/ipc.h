/*	$NetBSD: ipc.h,v 1.4 2011/05/24 18:29:23 joerg Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ipc.h	8.4 (Berkeley) 2/19/95
 */

/*
 * SVID compatible ipc.h file
 */

#ifndef _COMPAT_SYS_IPC_H_
#define _COMPAT_SYS_IPC_H_

__BEGIN_DECLS
/*
 * Old IPC permission structure used before NetBSD 1.5.
 */
struct ipc_perm14 {
	unsigned short	cuid;	/* creator user id */
	unsigned short	cgid;	/* creator group id */
	unsigned short	uid;	/* user id */
	unsigned short	gid;	/* group id */
	unsigned short	mode;	/* r/w permission */
	unsigned short	seq;	/* sequence # (to generate unique
				   msg/sem/shm id) */
	key_t	key;		/* user specified msg/sem/shm key */
};

static __inline void	__ipc_perm14_to_native(const struct ipc_perm14 *, struct ipc_perm *);
static __inline void	__native_to_ipc_perm14(const struct ipc_perm *, struct ipc_perm14 *);
static __inline void
__ipc_perm14_to_native(const struct ipc_perm14 *operm, struct ipc_perm *perm)
{

#define	CVT(x)	perm->x = operm->x
	CVT(uid);
	CVT(gid);
	CVT(cuid);
	CVT(cgid);
	CVT(mode);
#undef CVT
}

static inline void
__native_to_ipc_perm14(const struct ipc_perm *perm, struct ipc_perm14 *operm)
{

#define	CVT(x)	operm->x = perm->x
	CVT(uid);
	CVT(gid);
	CVT(cuid);
	CVT(cgid);
	CVT(mode);
#undef CVT

	/*
	 * Not part of the API, but some programs might look at it.
	 */
	operm->seq = perm->_seq;
	operm->key = perm->_key;
}

#if defined(_KERNEL) && defined(SYSCTLFN_ARGS)
int sysctl_kern_sysvipc50(SYSCTLFN_ARGS);
#define	KERN_SYSVIPC_OMSG_INFO		1	/* msginfo and msqid_ds50 */
#define	KERN_SYSVIPC_OSEM_INFO		2	/* seminfo and semid_ds50 */
#define	KERN_SYSVIPC_OSHM_INFO		3	/* shminfo and shmid_ds50 */
#endif

__END_DECLS

#endif /* !_COMPAT_SYS_IPC_H_ */
