/*	$NetBSD: ipc.h,v 1.33 2012/03/13 18:41:02 elad Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
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

#ifndef _SYS_IPC_H_
#define _SYS_IPC_H_

#include <sys/featuretest.h>
#include <sys/types.h>

/* Data structure used to pass permission information to IPC operations. */
struct ipc_perm {
	key_t key;			/* Key. */
	uid_t		uid;	/* user id */
	gid_t		gid;	/* group id */
	uid_t		cuid;	/* creator user id */
	gid_t		cgid;	/* creator group id */
	unsigned short int mode;	/* Reader/write permission. */
	unsigned short int __seq;	/* Sequence number. */
};

/* X/Open required constants (same values as system 5) */
#define	IPC_CREAT	001000	/* create entry if key does not exist */
#define	IPC_EXCL	002000	/* fail if key exists */
#define	IPC_NOWAIT	004000	/* error if request must wait */

#define	IPC_PRIVATE	(key_t)0 /* private key */

#define	IPC_RMID	0	/* remove identifier */
#define	IPC_SET		1	/* set options */
#define	IPC_STAT	2	/* get options */

#ifdef __minix
#define IPC_INFO	3	/* See ipcs. */
#endif /* !__minix */

/*
 * Macros to convert between ipc ids and array indices or sequence ids.
 * The first of these is used by ipcs(1), and so is defined outside the
 * kernel as well.
 */
#if defined(_NETBSD_SOURCE)
#define	IXSEQ_TO_IPCID(ix,perm)	(((perm._seq) << 16) | (ix & 0xffff))
#endif

#include <sys/cdefs.h>

__BEGIN_DECLS
key_t	ftok(const char *, int);
__END_DECLS

#endif /* !_SYS_IPC_H_ */
