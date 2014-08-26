/*	$NetBSD: extern.h,v 1.33 2010/05/31 03:18:33 rmind Exp $	*/

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
 *	@(#)extern.h	8.3 (Berkeley) 4/2/94
 */

/* 
 * We expect to be included by ps.h, which will already have
 * defined the types we use.
 */

extern double ccpu;
extern int eval, fscale, mempages, nlistread, rawcpu, maxslp, uspace;
extern int sumrusage, termwidth, totwidth;
extern int needenv, needcomm, commandonly;
extern uid_t myuid;
extern kvm_t *kd;
extern VAR var[];
extern VARLIST displaylist;
extern VARLIST sortlist;

void	 command(void *, VARENT *, int);
void	 cpuid(void *, VARENT *, int);
void	 cputime(void *, VARENT *, int);
int	 donlist(void);
int	 donlist_sysctl(void);
void	 fmt_puts(char *, int *);
void	 fmt_putc(int, int *);
void	 elapsed(void *, VARENT *, int);
double	 getpcpu(const struct kinfo_proc2 *);
double	 getpmem(const struct kinfo_proc2 *);
void	 gname(void *, VARENT *, int);
void	 groups(void *, VARENT *, int);
void	 groupnames(void *, VARENT *, int);
void	 logname(void *, VARENT *, int);
void	 longtname(void *, VARENT *, int);
void	 lname(void *, VARENT *, int);
void	 lstarted(void *, VARENT *, int);
void	 lstate(void *, VARENT *, int);
void	 maxrss(void *, VARENT *, int);
void	 nlisterr(struct nlist *);
void	 p_rssize(void *, VARENT *, int);
void	 pagein(void *, VARENT *, int);
void	 parsefmt(const char *);
void	 parsefmt_insert(const char *, VARENT **);
void	 parsesort(const char *);
VARENT * varlist_find(VARLIST *, const char *);
void	 emul(void *, VARENT *, int);
void	 pcpu(void *, VARENT *, int);
void	 pmem(void *, VARENT *, int);
void	 pnice(void *, VARENT *, int);
void	 pri(void *, VARENT *, int);
void	 printheader(void);
void	 putimeval(void *, VARENT *, int);
void	 pvar(void *, VARENT *, int);
void	 rgname(void *, VARENT *, int);
void	 rssize(void *, VARENT *, int);
void	 runame(void *, VARENT *, int);
void	 showkey(void);
void	 started(void *, VARENT *, int);
void	 state(void *, VARENT *, int);
void	 svgname(void *, VARENT *, int);
void	 svuname(void *, VARENT *, int);
void	 tdev(void *, VARENT *, int);
void	 tname(void *, VARENT *, int);
void	 tsize(void *, VARENT *, int);
void	 ucomm(void *, VARENT *, int);
void	 uname(void *, VARENT *, int);
void	 uvar(void *, VARENT *, int);
void	 vsize(void *, VARENT *, int);
void	 wchan(void *, VARENT *, int);
