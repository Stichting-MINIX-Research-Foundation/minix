/* $NetBSD: extern.h,v 1.29 2013/07/16 17:47:43 christos Exp $ */

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)extern.h	8.1 (Berkeley) 5/31/93
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

#include <sys/cdefs.h>

/*
 * csh.c
 */
int gethdir(Char *);
void dosource(Char **, struct command *);
__dead void exitstat(void);
__dead void goodbye(void);
void importpath(Char *);
void initdesc(void);
__dead void pintr(int);
__dead void pintr1(int);
void printprompt(void);
#ifdef EDIT
char *printpromptstr(EditLine *);
#endif
void process(int);
void rechist(void);
void untty(void);
int vis_fputc(int, FILE *);

#ifdef PROF
__dead void done(int);
#else
__dead void xexit(int);
#endif

/*
 * dir.c
 */
void dinit(Char *);
void dodirs(Char **, struct command *);
Char *dcanon(Char *, Char *);
void dtildepr(Char *, Char *);
void dtilde(void);
void dochngd(Char **, struct command *);
Char *dnormalize(Char *);
void dopushd(Char **, struct command *);
void dopopd(Char **, struct command *);
struct directory;
void dfree(struct directory *);

/*
 * dol.c
 */
void Dfix(struct command *);
Char *Dfix1(Char *);
void heredoc(Char *);

/*
 * err.c
 */
void seterror(int, ...);
__dead void stderror(int, ...);

/*
 * exec.c
 */
__dead void doexec(Char **, struct command *);
void dohash(Char **, struct command *);
void dounhash(Char **, struct command *);
void dowhich(Char **, struct command *);
void execash(Char **, struct command *);
void hashstat(Char **, struct command *);
void xechoit(Char **);

/*
 * exp.c
 */
int expr(Char ***);
int exp0(Char ***, int);

/*
 * file.c
 */
#ifdef FILEC
ssize_t tenex(Char *, size_t);
#endif

/*
 * func.c
 */
void Setenv(Char *, Char *);
void doalias(Char **, struct command *);
void dobreak(Char **, struct command *);
void docontin(Char **, struct command *);
void doecho(Char **, struct command *);
void doelse(Char **, struct command *);
void doend(Char **, struct command *);
void doeval(Char **, struct command *);
void doexit(Char **, struct command *);
void doforeach(Char **, struct command *);
void doglob(Char **, struct command *);
void dogoto(Char **, struct command *);
void doif(Char **, struct command *);
void dolimit(Char **, struct command *);
__dead void dologin(Char **, struct command *);
__dead void dologout(Char **, struct command *);
void donohup(Char **, struct command *);
void doonintr(Char **, struct command *);
void doprintf(Char **, struct command *);
void dorepeat(Char **, struct command *);
void dosetenv(Char **, struct command *);
void dosuspend(Char **, struct command *);
void doswbrk(Char **, struct command *);
void doswitch(Char **, struct command *);
void doumask(Char **, struct command *);
void dounlimit(Char **, struct command *);
void dounsetenv(Char **, struct command *);
void dowhile(Char **, struct command *);
void dozip(Char **, struct command *);
void func(struct command *, struct biltins *);
struct biltins *isbfunc(struct command *);
void prvars(void);
void gotolab(Char *);
int srchx(Char *);
void unalias(Char **, struct command *);
void wfree(void);

/*
 * glob.c
 */
Char **dobackp(Char *, int);
void Gcat(Char *, Char *);
Char *globone(Char *, int);
int  Gmatch(Char *, Char *);
void ginit(void);
Char **globall(Char **);
void rscan(Char **, void (*)(int));
void tglob(Char **);
void trim(Char **);
#ifdef FILEC
int sortscmp(const ptr_t, const ptr_t);
#endif /* FILEC */

/*
 * hist.c
 */
void dohist(Char **, struct command *);
struct Hist *enthist(int, struct wordent *, int);
#ifdef EDIT
void loadhist(struct Hist *);
#endif
void savehist(struct wordent *);

/*
 * lex.c
 */
void addla(Char *);
void bseek(struct Ain *);
void btell(struct Ain *);
void btoeof(void);
void copylex(struct wordent *, struct wordent *);
Char *domod(Char *, int);
void freelex(struct wordent *);
int lex(struct wordent *);
void prlex(FILE *, struct wordent *);
#ifdef EDIT
int sprlex(char **, struct wordent *);
#endif
int readc(int);
void settell(void);
void unreadc(int);

/*
 * misc.c
 */
int any(const char *, int);
Char **blkcat(Char **, Char **);
Char **blkcpy(Char **, Char **);
Char **blkend(Char **);
void blkfree(Char **);
int blklen(Char **);
void blkpr(FILE *, Char **);
Char **blkspl(Char **, Char **);
void closem(void);
Char **copyblk(Char **);
int dcopy(int, int);
int dmove(int, int);
void donefds(void);
Char lastchr(Char *);
void lshift(Char **, size_t);
int number(Char *);
int prefix(Char *, Char *);
Char **saveblk(Char **);
Char *strip(Char *);
Char *quote(Char *);
char *strsave(const char *);
char *strspl(char *, char *);
__dead void udvar(Char *);

#ifndef	SHORT_STRINGS
# ifdef NOTUSED
char *strstr(const char *, const char *);
# endif /* NOTUSED */
char *strend(char *);
#endif

/*
 * parse.c
 */
void alias(struct wordent *);
void freesyn(struct command *);
struct command *syntax(struct wordent *, struct wordent *, int);


/*
 * proc.c
 */
void dobg(Char **, struct command *);
void dobg1(Char **, struct command *);
void dofg(Char **, struct command *);
void dofg1(Char **, struct command *);
void dojobs(Char **, struct command *);
void dokill(Char **, struct command *);
void donotify(Char **, struct command *);
void dostop(Char **, struct command *);
void dowait(Char **, struct command *);
void palloc(int, struct command *);
void panystop(int);
void pchild(int);
void pendjob(void);
struct process *pfind(Char *);
int pfork(struct command *, int);
void pgetty(int, int);
void pjwait(struct process *);
void pnote(void);
void prestjob(void);
void psavejob(void);
void pstart(struct process *, int);
void pwait(void);

/*
 * sem.c
 */
void execute(struct command *, int, int *, int *);
void mypipe(int *);

/*
 * set.c
 */
struct varent*adrof1(Char *, struct varent *);
void doset(Char **, struct command *);
void dolet(Char **, struct command *);
Char *putn(int);
int getn(Char *);
Char *value1(Char *, struct varent *);
void set(Char *, Char *);
void set1(Char *, Char **, struct varent *);
void setq(Char *, Char **, struct varent *);
void unset(Char **, struct command *);
void unset1(Char *[], struct varent *);
void unsetv(Char *);
void setNS(Char *);
void shift(Char **, struct command *);
void plist(struct varent *);

/*
 * time.c
 */
void donice(Char **, struct command *);
void dotime(Char **, struct command *);
void prusage(FILE *, struct rusage *, struct rusage *, struct timespec *,
             struct timespec *);
void ruadd(struct rusage *, struct rusage *);
void settimes(void);
void psecs(long);

/*
 * alloc.c
 */
void Free(ptr_t);
ptr_t Malloc(size_t);
ptr_t Realloc(ptr_t, size_t);
ptr_t Calloc(size_t, size_t);

/*
 * str.c:
 */
#ifdef SHORT_STRINGS
Char *s_strchr(const Char *, int);
Char *s_strrchr(const Char *, int);
Char *s_strcat(Char *, const Char *);
#ifdef NOTUSED
Char *s_strncat(Char *, const Char *, size_t);
#endif
Char *s_strcpy(Char *, const Char *);
Char *s_strncpy(Char *, const Char *, size_t);
Char *s_strspl(const Char *, const Char *);
size_t s_strlen(const Char *);
int s_strcmp(const Char *, const Char *);
int s_strncmp(const Char *, const Char *, size_t);
Char *s_strsave(const Char *);
Char *s_strend(const Char *);
Char *s_strstr(const Char *, const Char *);
Char *str2short(const char *);
Char **blk2short(char **);
char *short2str(const Char *);
char **short2blk(Char * const *);
#endif
char *short2qstr(const Char *);
char *vis_str(const Char *);

#endif /* !_EXTERN_H_ */
