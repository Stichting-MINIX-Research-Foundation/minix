/* $NetBSD: filemon.h,v 1.7 2015/09/06 06:01:00 dholland Exp $ */
/*
 * Copyright (c) 2010, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef FILEMON_SET_FD

#include <sys/ioccom.h>

#ifndef _PATH_FILEMON
#define _PATH_FILEMON "/dev/filemon"
#endif
#define FILEMON_SET_FD		_IOWR('S', 1, int)
#define FILEMON_SET_PID		_IOWR('S', 2, pid_t)

#define FILEMON_VERSION		5

#ifdef _KERNEL
struct filemon {
	pid_t fm_pid;		/* The process ID being monitored. */
	char fm_fname1[MAXPATHLEN];/* Temporary filename buffer. */
	char fm_fname2[MAXPATHLEN];/* Temporary filename buffer. */
	char fm_msgbufr[32 + 2 * MAXPATHLEN];	/* Output message buffer. */
	int fm_fd;			/* Output fd */
	struct file *fm_fp;	/* Output file pointer. */
	krwlock_t fm_mtx;		/* Lock mutex for this filemon. */
	TAILQ_ENTRY(filemon) fm_link;	/* Link into the in-use list. */
};

struct filemon * filemon_lookup(struct proc *);
void filemon_output(struct filemon *, char *, size_t);
void filemon_wrapper_install(void);
int  filemon_wrapper_deinstall(void);
void filemon_printf(struct filemon *, const char *, ...) __printflike(2, 3);
#endif

#endif
