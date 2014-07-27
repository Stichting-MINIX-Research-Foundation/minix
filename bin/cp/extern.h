/* $NetBSD: extern.h,v 1.17 2012/01/04 15:58:37 christos Exp $ */

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)extern.h	8.2 (Berkeley) 4/1/94
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

typedef struct {
	char *p_end;			/* pointer to NULL at end of path */
	char *target_end;		/* pointer to end of target base */
	char p_path[MAXPATHLEN + 1];	/* pointer to the start of a path */
} PATH_T;

extern PATH_T to;
extern uid_t myuid;
extern int Rflag, rflag, Hflag, Lflag, Pflag, fflag, iflag, lflag, pflag, Nflag;
extern mode_t myumask;
extern sig_atomic_t pinfo;

#include <sys/cdefs.h>

__BEGIN_DECLS
int	copy_fifo(struct stat *, int);
int	copy_file(FTSENT *, int);
int	copy_link(FTSENT *, int);
int	copy_special(struct stat *, int);
int	set_utimes(const char *, struct stat *);
int	setfile(struct stat *, int);
void	usage(void) __attribute__((__noreturn__));
__END_DECLS

#endif /* !_EXTERN_H_ */
