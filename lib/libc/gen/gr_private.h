/*	$NetBSD: gr_private.h,v 1.2 2008/04/28 20:22:59 martin Exp $	*/

/*-
 * Copyright (c) 2004-2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 * Structures and functions used by various group(5) public functions
 * and their back-end implementations.
 * These are subject to change without notice and should not be used
 * outside of libc (even by third-party nss_*.so modules implementing
 * group(5) back-ends).
 */

#define _GROUP_COMPAT	/* "group" defaults to compat, so always provide it */


#ifndef __minix /* should be _REENTRANT */
	/*
	 * mutex to serialize the public group(5) functions use of the
	 * back-end implementations, which may not be reentrant.
	 */
extern 	mutex_t		__grmutex;
#endif

	/*
	 * files methods
	 */
struct __grstate_files {	/* state shared between files methods */
	int	 stayopen;	/* see getgroupent(3) */
	FILE	*fp;		/* groups file handle */
};

extern int	__grstart_files(struct __grstate_files *);
extern int	__grend_files(struct __grstate_files *);
extern int	__grscan_files(int *, struct group *, char *, size_t,
			struct __grstate_files *, int, const char *, gid_t);

	/*
	 * dns methods
	 */
struct __grstate_dns {		/* state shared between dns methods */
	int	 stayopen;	/* see getgroupent(3) */
	void	*context;	/* Hesiod context */
	int	 num;		/* group index, -1 if no more */
};

extern int	__grstart_dns(struct __grstate_dns *);
extern int	__grend_dns(struct __grstate_dns *state);
extern int	__grscan_dns(int *, struct group *, char *, size_t,
			struct __grstate_dns *, int, const char *, gid_t);

	/*
	 * nis methods
	 */
struct __grstate_nis {		/* state shared between nis methods */
	int	 stayopen;	/* see getgroupent(3) */
	char	*domain;	/* NIS domain */
	int	 done;		/* non-zero if search exhausted */
	char	*current;	/* current first/next match */
	int	 currentlen;	/* length of _nis_current */
};

extern int	__grstart_nis(struct __grstate_nis *);
extern int	__grend_nis(struct __grstate_nis *);
extern int	__grscan_nis(int *, struct group *, char *, size_t,
			struct __grstate_nis *, int, const char *, gid_t);

	/*
	 * compat methods
	 */
struct __grstate_compat {	/* state shared between compat methods */
	int	 stayopen;	/* see getgroupent(3) */
	FILE	*fp;		/* file handle */
/*
 * XXX:	convert name to a separate compatstate enum and grow name as necessary
 *	instead of using strdup & free for each + line
 */
	char	*name;		/* NULL if reading file,	*/
				/*   "" if compat "+",		*/
				/* name if compat "+name"	*/
};

extern int	__grbad_compat(void *nsrv, void *nscb, va_list ap);
extern int	__grstart_compat(struct __grstate_compat *);
extern int	__grend_compat(struct __grstate_compat *);
extern int	__grscan_compat(int *, struct group *, char *, size_t,
			struct __grstate_compat *, int, const char *, gid_t,
			int (*)(void *, struct group **), void *);
