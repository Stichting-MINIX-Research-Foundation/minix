/*	$NetBSD: chpass.h,v 1.12 2005/02/17 17:09:48 xtraeme Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
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
 *	@(#)chpass.h	8.4 (Berkeley) 4/2/94
 */

struct passwd;

typedef struct _entry {
	const char *prompt;
	int (*func)(const char *, struct passwd *, struct _entry *), restricted, len;
	const char *except, *save;
} ENTRY;

extern	int use_yp;

/* Field numbers. */
#define	E_BPHONE	8
#define	E_HPHONE	9
#define	E_LOCATE	10
#define	E_NAME		7
#define	E_SHELL		12

extern ENTRY list[];
extern uid_t uid;

int	 atot(const char *, time_t *);
void	 display(char *, int, struct passwd *);
void	 edit(char *, struct passwd *);
const char *
	 ok_shell(const char *);
int	 p_change(const char *, struct passwd *, ENTRY *);
int	 p_class(const char *, struct passwd *, ENTRY *);
int	 p_expire(const char *, struct passwd *, ENTRY *);
int	 p_gecos(const char *, struct passwd *, ENTRY *);
int	 p_gid(const char *, struct passwd *, ENTRY *);
int	 p_hdir(const char *, struct passwd *, ENTRY *);
int	 p_login(const char *, struct passwd *, ENTRY *);
int	 p_passwd(const char *, struct passwd *, ENTRY *);
int	 p_shell(const char *, struct passwd *, ENTRY *);
int	 p_uid(const char *, struct passwd *, ENTRY *);
char    *ttoa(char *, size_t, time_t);
int	 verify(char *, struct passwd *);

#ifdef YP
int	check_yppasswdd(void);
int	pw_yp(struct passwd *, uid_t);
void	yppw_error(const char *name, int, int);
void	yppw_prompt(void);
struct passwd *ypgetpwnam(const char *);
struct passwd *ypgetpwuid(uid_t);
#endif

extern	void (*Pw_error)(const char *name, int, int);
