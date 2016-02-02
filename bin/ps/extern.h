/*	$NetBSD: extern.h,v 1.35 2014/04/20 22:48:59 dholland Exp $	*/

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

void	 command(void *, VARENT *, enum mode);
void	 cpuid(void *, VARENT *, enum mode);
void	 cputime(void *, VARENT *, enum mode);
int	 donlist(void);
int	 donlist_sysctl(void);
void	 fmt_puts(char *, int *);
void	 fmt_putc(int, int *);
void	 elapsed(void *, VARENT *, enum mode);
double	 getpcpu(const struct kinfo_proc2 *);
double	 getpmem(const struct kinfo_proc2 *);
void	 gname(void *, VARENT *, enum mode);
void	 groups(void *, VARENT *, enum mode);
void	 groupnames(void *, VARENT *, enum mode);
void	 lcputime(void *, VARENT *, enum mode);
void	 logname(void *, VARENT *, enum mode);
void	 longtname(void *, VARENT *, enum mode);
void	 lname(void *, VARENT *, enum mode);
void	 lstarted(void *, VARENT *, enum mode);
void	 lstate(void *, VARENT *, enum mode);
void	 maxrss(void *, VARENT *, enum mode);
void	 nlisterr(struct nlist *);
void	 p_rssize(void *, VARENT *, enum mode);
void	 pagein(void *, VARENT *, enum mode);
void	 parsefmt(const char *);
void	 parsefmt_insert(const char *, VARENT **);
void	 parsesort(const char *);
VARENT * varlist_find(VARLIST *, const char *);
void	 emul(void *, VARENT *, enum mode);
void	 pcpu(void *, VARENT *, enum mode);
void	 pmem(void *, VARENT *, enum mode);
void	 pnice(void *, VARENT *, enum mode);
void	 pri(void *, VARENT *, enum mode);
void	 printheader(void);
void	 putimeval(void *, VARENT *, enum mode);
void	 pvar(void *, VARENT *, enum mode);
void	 rgname(void *, VARENT *, enum mode);
void	 rssize(void *, VARENT *, enum mode);
void	 runame(void *, VARENT *, enum mode);
void	 showkey(void);
void	 started(void *, VARENT *, enum mode);
void	 state(void *, VARENT *, enum mode);
void	 svgname(void *, VARENT *, enum mode);
void	 svuname(void *, VARENT *, enum mode);
void	 tdev(void *, VARENT *, enum mode);
void	 tname(void *, VARENT *, enum mode);
void	 tsize(void *, VARENT *, enum mode);
void	 ucomm(void *, VARENT *, enum mode);
void	 uname(void *, VARENT *, enum mode);
void	 uvar(void *, VARENT *, enum mode);
void	 vsize(void *, VARENT *, enum mode);
void	 wchan(void *, VARENT *, enum mode);
