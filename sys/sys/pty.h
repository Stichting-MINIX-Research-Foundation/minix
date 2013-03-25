/*	$NetBSD: pty.h,v 1.8 2008/04/28 20:24:11 martin Exp $	*/

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
int pty_fill_ptmget(struct lwp *, dev_t, int, int, void *);
int pty_grant_slave(struct lwp *, dev_t);
dev_t pty_makedev(char, int);
int pty_vn_open(struct vnode *, struct lwp *);
struct ptm_pty *pty_sethandler(struct ptm_pty *);
#endif

struct ptm_pty {
	int (*allocvp)(struct ptm_pty *, struct lwp *, struct vnode **, dev_t,
	    char);
	int (*makename)(struct ptm_pty *, struct lwp *, char *, size_t, dev_t, char);
	void (*getvattr)(struct ptm_pty *, struct lwp *, struct vattr *);
	void *arg;
};

extern int npty;

#ifdef COMPAT_BSDPTY
extern struct ptm_pty ptm_bsdpty;
#endif

#endif /* _SYS_PTY_H_ */
