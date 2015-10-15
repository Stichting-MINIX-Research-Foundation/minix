/*	$NetBSD: pty.h,v 1.11 2014/10/15 15:00:03 christos Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
#ifndef _SYS_PTY_H_
#define _SYS_PTY_H_

int pty_isfree(int, int);
int pty_check(int);

#ifndef NO_DEV_PTM
void ptmattach(int);
int pty_fill_ptmget(struct lwp *, dev_t, int, int, void *, struct mount *);
int pty_grant_slave(struct lwp *, dev_t, struct mount *);
dev_t pty_makedev(char, int);
struct ptm_pty *pty_sethandler(struct ptm_pty *);
int pty_getmp(struct lwp *, struct mount **);

/*
 * Ptm_pty is used for switch ptm{x} driver between BSDPTY, PTYFS.
 * Functions' argument (struct mount *) is used only PTYFS,
 * in the case BSDPTY can be NULL.
 */
struct ptm_pty {
	int (*allocvp)(struct mount *, struct lwp *, struct vnode **, dev_t,
	    char);
	int (*makename)(struct mount *, struct lwp *, char *, size_t, dev_t, char);
	void (*getvattr)(struct mount *, struct lwp *, struct vattr *);
	int (*getmp)(struct lwp *, struct mount **);
};

#ifdef COMPAT_BSDPTY
extern struct ptm_pty ptm_bsdpty;
#endif

#endif /* NO_DEV_PTM */

extern int npty;

#endif /* _SYS_PTY_H_ */
