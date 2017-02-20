/*	$NetBSD: extern.h,v 1.33 2014/12/16 19:30:24 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)extern.h	8.2 (Berkeley) 4/20/95
 *	$NetBSD: extern.h,v 1.33 2014/12/16 19:30:24 christos Exp $
 */

#ifndef __EXTERN_H__
#define __EXTERN_H__

/*
 * from cmd1.c
 */
int	More(void *);
int	Type(void *);
int	folders(void *);
int	from(void *);
int	headers(void *);
int	inc(void *);
int	mboxit(void *);
int	more(void *);
int	pcmdlist(void *);
int	pdot(void *);
int	pipecmd(void *);
int	scroll(void *);
int	stouch(void *);
int	top(void *);
int	type(void *);
#ifdef MIME_SUPPORT
int	page(void *);
int	Page(void *);
int	print(void *);
int	Print(void *);
int	view(void *);
int	View(void *);
#endif
/* XXX - should these be elsewhere? */
void	printhead(int);
char *	sget_msgnum(struct message *, struct message *);
void	show_msgnum(FILE *, struct message *, struct message *);

/*
 * from cmd2.c
 */
int	Detach(void *);
int	Save(void *);
int	clobber(void *);
int	copycmd(void *);
int	core(void *);
int	delete(void *);
int	deltype(void *);
int	detach(void *);
int	igfield(void *);
int	next(void *);
int	retfield(void *);
int	save(void *);
int	saveigfield(void *);
int	saveretfield(void *);
int	swrite(void *);
int	undeletecmd(void *);

/*
 * from cmd3.c
 */
int	Respond(void *);
int	alternates(void *);
int	bounce(void *);
int	dosh(void *);
int	echo(void *);
int	elsecmd(void *);
int	endifcmd(void *);
int	file(void *);
int	bounce(void *);
int	forward(void *);
int	group(void *);
int	help(void *);
int	ifcmd(void *);
int	ifdefcmd(void *v);
int	ifndefcmd(void *v);
int	markread(void *);
int	messize(void *);
int	null(void *);
int	preserve(void *);
int	respond(void *);
int	rexit(void *);
int	schdir(void *);
int	set(void *);
int	shell(void *);
int	show(void *);
int	unalias(void *);
int	unread(void *);
int	unset(void *);
/* XXX - Should this be elsewhere? */
void	sort(const char **);

/*
 * from cmd4.c
 */
struct smopts_s *findsmopts(const char *, int);
int	smoptscmd(void *);
int	unsmoptscmd(void *);
int	Header(void *);

/*
 * from cmdtab.c
 */
extern const struct cmd cmdtab[];

/*
 * from collect.c
 */
FILE *	collect(struct header *, int);
void	savedeadletter(FILE *);

/*
 * from dotlock.c
 */
int	dot_lock(const char *, int, FILE *, const char *);
void	dot_unlock(const char *);

/*
 * from edit.c
 */
int	editor(void *);
int	visual(void *);
FILE *	run_editor(FILE *, off_t, int, int);

/*
 * from fio.c
 */
const char *expand(const char *);
off_t	fsize(FILE *);
const char *getdeadletter(void);
int	getfold(char *, size_t);
#ifdef USE_EDITLINE
#define readline xreadline	/* readline() is defined in libedit */
#endif
int	readline(FILE *, char *, int, int);
int	putline(FILE *, const char *, int);
int	rm(char *);
FILE *	setinput(const struct message *);
void	setptr(FILE *, off_t);

/*
 * from getname.c
 */
const char *getname(uid_t);
int	getuserid(char []);

/*
 * from head.c
 */
int	ishead(const char []);
void	parse(const char [], struct headline *, char []);

/*
 * from lex.c
 */
void	announce(void);
void	commands(void);
enum execute_contxt_e { ec_normal, ec_composing, ec_autoprint };
int	execute(char [], enum execute_contxt_e);
int	incfile(void);
const struct cmd *lex(char []);
void	load(const char *);
int	newfileinfo(int);
int	pversion(void *);
int	setfile(const char *);
char *	shellpr(char *);
char *	get_cmdname(char *);

/*
 * from list.c
 */
int	first(int, int);
int	get_Hflag(char **);
int	getmsglist(char *, int *, int);
int	getrawlist(const char [], char **, int);
int	show_headers_and_exit(int) __dead;

/*
 * from main.c
 */
struct name *lexpand(char *, int);
void	setscreensize(void);
int	main(int, char **);

/*
 * from names.c
 */
struct name *cat(struct name *, struct name *);
int	count(struct name *);
struct name *delname(struct name *, char []);
char *	detract(struct name *, int);
struct name * elide(struct name *);
struct name * extract(char [], int);
struct name * gexpand(struct name *, struct grouphead *, int, int);
struct name * nalloc(char [], int);
struct name * outof(struct name *, FILE *, struct header *);
const char ** unpack(struct name *, struct name *);
struct name * usermap(struct name *);
#if 0
void	prettyprint(struct name *);	/* commented out? */
#endif

/*
 * from popen.c
 */
int	Fclose(FILE *);
FILE *	Fdopen(int, const char *);
FILE *	Fopen(const char *, const char *);
int	Pclose(FILE *);
FILE *	Popen(const char *, const char *);
void	close_all_files(void);
void	close_top_files(FILE *);
void	free_child(int);
void	prepare_child(sigset_t *, int, int);
FILE *	last_registered_file(int);
void	register_file(FILE *, int, int);
int	run_command(const char *, sigset_t *, int, int, ...);
void	sigchild(int);
int	start_command(const char *, sigset_t *, int, int, ...);
int	wait_child(int);
#ifdef MIME_SUPPORT
void	flush_files(FILE *, int);
#endif

/*
 * from quit.c
 */
void	quit(jmp_buf);
int	quitcmd(void *);

/*
 * from send.c
 */
#ifndef MIME_SUPPORT
# define sendmessage(a,b,c,d,e)	legacy_sendmessage(a,b,c,d)
# define mail(a,b,c,d,e,f)	legacy_mail(a,b,c,d,e)
#endif
int	sendmessage(struct message *, FILE *, struct ignoretab *, const char *, struct mime_info *);
int	mail(struct name *, struct name *, struct name *, struct name *, char *, struct attachment *);
void	mail1(struct header *, int);
void	mail2(FILE *, const char **);
int	puthead(struct header *, FILE *, int);
int	sendmail(void *);

/*
 * from strings.c
 */
void *	csalloc(size_t, size_t);
void *	salloc(size_t);
void	sreset(void);
void	spreserve(void);

/*
 * from support.c
 */
void	add_ignore(const char *, struct ignoretab *);
void	alter(char *);
int	argcount(char **);
int	blankline(char []);
char *	copy(char *, char *);
char *	hfield(const char [], const struct message *);
int	isdir(const char []);
int	isign(const char *, struct ignoretab []);
void	istrcpy(char *, const char *);
int	member(char *, struct ignoretab *);
char *	nameof(struct message *, int);
int	sasprintf(char **ret, const char *format, ...) __printflike(2, 3);
char *	savestr(const char *);
struct message *set_m_flag(int, int, int);
char *	skin(char *);
int	source(void *);
void	touch(struct message *);
int	unstack(void);
int	upcase(int);
void	cathelp(const char *);

/*
 * from temp.c
 */
void	tinit(void);

/*
 * from tty.c
 */
int	grabh(struct header *, int);

/*
 * from vars.c
 */
void	assign(const char [], const char []);
struct grouphead * findgroup(const char []);
int	hash(const char *);
struct var * lookup(const char []);
void	printgroup(const char []);
void	v_free(char *);
char *	value(const char []);
char *	vcopy(const char []);

/*
 * from v7.local.c
 */
void	demail(void);
void	findmail(const char *, char *, size_t);
const char *username(void);

/*
 * from version.c
 */
extern const char *version;


#ifndef	THREAD_SUPPORT
/*
 * Specials from fio.c (if THREAD_SUPPORT is not defined).
 * With THREAD_SUPPORT, they live in thread.c.
 */
struct message *next_message(struct message *);
struct message *prev_message(struct message *);
struct message *get_message(int);
int	get_msgnum(struct message *);
int	get_msgCount(void);

/* we remap these commands */
# define get_abs_msgCount	get_msgCount
# define get_abs_message(a)	get_message(a)
# define next_abs_message(a)	next_message(a)

/* we trash these commands */
# define do_recursion()			0
# define thread_recursion(mp,fn,args)	fn(mp,args)
# define thread_fix_old_links(nmessage,message,omsgCount)
# define thread_fix_new_links(message,omsgCount,msgCount)
#endif /* THREAD_SUPPORT */

#endif /* __EXTERN_H__ */
