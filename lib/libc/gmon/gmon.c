/*	$NetBSD: gmon.c,v 1.34 2012/03/13 21:13:37 christos Exp $	*/

/*
 * Copyright (c) 2003, 2004 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Nathan J. Williams for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1983, 1992, 1993
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
 */

#include <sys/cdefs.h>
#if !defined(lint) && defined(LIBC_SCCS)
#if 0
static char sccsid[] = "@(#)gmon.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: gmon.c,v 1.34 2012/03/13 21:13:37 christos Exp $");
#endif
#endif

#include "namespace.h"
#include <sys/param.h>
#include <sys/time.h>
#include <sys/gmon.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <err.h>
#include "extern.h"
#include "reentrant.h"

struct gmonparam _gmonparam = { .state = GMON_PROF_OFF };

#ifdef _REENTRANT
struct gmonparam *_gmonfree;
struct gmonparam *_gmoninuse;
mutex_t _gmonlock = MUTEX_INITIALIZER;
thread_key_t _gmonkey;
struct gmonparam _gmondummy;
#endif

static u_int	s_scale;
/* see profil(2) where this is describe (incorrectly) */
#define		SCALE_1_TO_1	0x10000L

void	moncontrol(int);
void	monstartup(u_long, u_long);
void	_mcleanup(void);
static int hertz(void);

#ifdef _REENTRANT
static void _m_gmon_destructor(void *);
struct gmonparam *_m_gmon_alloc(void)
    __attribute__((__no_instrument_function__));
static void _m_gmon_merge(void);
static void _m_gmon_merge_two(struct gmonparam *, struct gmonparam *);
#endif

void
monstartup(u_long lowpc, u_long highpc)
{
	u_long o;
	char *cp;
	struct gmonparam *p = &_gmonparam;

	/*
	 * round lowpc and highpc to multiples of the density we're using
	 * so the rest of the scaling (here and in gprof) stays in ints.
	 */
	p->lowpc = rounddown(lowpc, HISTFRACTION * sizeof(HISTCOUNTER));
	p->highpc = roundup(highpc, HISTFRACTION * sizeof(HISTCOUNTER));
	p->textsize = p->highpc - p->lowpc;
	p->kcountsize = p->textsize / HISTFRACTION;
	p->hashfraction = HASHFRACTION;
	p->fromssize = p->textsize / p->hashfraction;
	p->tolimit = p->textsize * ARCDENSITY / 100;
	if (p->tolimit < MINARCS)
		p->tolimit = MINARCS;
	else if (p->tolimit > MAXARCS)
		p->tolimit = MAXARCS;
	p->tossize = p->tolimit * sizeof(struct tostruct);

	cp = sbrk((intptr_t)(p->kcountsize + p->fromssize + p->tossize));
	if (cp == (char *)-1) {
		warnx("%s: out of memory", __func__);
		return;
	}
#ifdef notdef
	(void)memset(cp, 0, p->kcountsize + p->fromssize + p->tossize);
#endif
	p->tos = (struct tostruct *)(void *)cp;
	cp += (size_t)p->tossize;
	p->kcount = (u_short *)(void *)cp;
	cp += (size_t)p->kcountsize;
	p->froms = (u_short *)(void *)cp;

	__minbrk = sbrk((intptr_t)0);
	p->tos[0].link = 0;

	o = p->highpc - p->lowpc;
	if (p->kcountsize < o) {
#ifndef notdef
		s_scale = ((float)p->kcountsize / o ) * SCALE_1_TO_1;
#else /* avoid floating point */
		u_long quot = o / p->kcountsize;
		
		if (quot >= 0x10000)
			s_scale = 1;
		else if (quot >= 0x100)
			s_scale = 0x10000 / quot;
		else if (o >= 0x800000)
			s_scale = 0x1000000 / (o / (p->kcountsize >> 8));
		else
			s_scale = 0x1000000 / ((o << 8) / p->kcountsize);
#endif
	} else
		s_scale = SCALE_1_TO_1;

#ifdef _REENTRANT
	_gmondummy.state = GMON_PROF_BUSY;
	thr_keycreate(&_gmonkey, _m_gmon_destructor);
#endif
	moncontrol(1);
}

#ifdef _REENTRANT
static void
_m_gmon_destructor(void *arg)
{
	struct gmonparam *p = arg, *q, **prev;

	if (p == &_gmondummy)
		return;

	thr_setspecific(_gmonkey, &_gmondummy);

	mutex_lock(&_gmonlock);
	/* XXX eww, linear list traversal. */
	for (q = _gmoninuse, prev = &_gmoninuse;
	     q != NULL;
	     prev = (struct gmonparam **)(void *)&q->kcount,	/* XXX */
		 q = (struct gmonparam *)(void *)q->kcount) {
		if (q == p)
			*prev = (struct gmonparam *)(void *)q->kcount;
	}
	p->kcount = (u_short *)(void *)_gmonfree;
	_gmonfree = p;
	mutex_unlock(&_gmonlock);

	thr_setspecific(_gmonkey, NULL);
}

struct gmonparam *
_m_gmon_alloc(void)
{
	struct gmonparam *p;
	char *cp;

	mutex_lock(&_gmonlock);
	if (_gmonfree != NULL) {
		p = _gmonfree;
		_gmonfree = (struct gmonparam *)(void *)p->kcount;
		p->kcount = (u_short *)(void *)_gmoninuse;
		_gmoninuse = p;
	} else {
		mutex_unlock(&_gmonlock);
		cp = mmap(NULL,
		    (size_t)(sizeof (struct gmonparam) + 
			_gmonparam.fromssize + _gmonparam.tossize),
		    PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, (off_t)0);
		p = (void *)cp;
		*p = _gmonparam;
		p->kcount = NULL;
		cp += sizeof (struct gmonparam);
		memset(cp, 0, (size_t)(p->fromssize + p->tossize));
		p->froms = (u_short *)(void *)cp;
		p->tos = (struct tostruct *)(void *)(cp + p->fromssize);
		mutex_lock(&_gmonlock);
		p->kcount = (u_short *)(void *)_gmoninuse;
		_gmoninuse = p;
	}
	mutex_unlock(&_gmonlock);
	thr_setspecific(_gmonkey, p);

	return p;
}

static void
_m_gmon_merge_two(struct gmonparam *p, struct gmonparam *q)
{
	u_long fromindex;
	u_short *frompcindex, qtoindex, toindex;
	u_long selfpc;
	u_long endfrom;
	long count;
	struct tostruct *top;
	
	endfrom = (q->fromssize / sizeof(*q->froms));
	for (fromindex = 0; fromindex < endfrom; fromindex++) {
		if (q->froms[fromindex] == 0)
			continue;
		for (qtoindex = q->froms[fromindex]; qtoindex != 0;
		     qtoindex = q->tos[qtoindex].link) {
			selfpc = q->tos[qtoindex].selfpc;
			count = q->tos[qtoindex].count;
			/* cribbed from mcount */
			frompcindex = &p->froms[fromindex];
			toindex = *frompcindex;
			if (toindex == 0) {
				/*
				 *	first time traversing this arc
				 */
				toindex = ++p->tos[0].link;
				if (toindex >= p->tolimit)
					/* halt further profiling */
					goto overflow;
				
				*frompcindex = (u_short)toindex;
				top = &p->tos[(size_t)toindex];
				top->selfpc = selfpc;
				top->count = count;
				top->link = 0;
				goto done;
			}
			top = &p->tos[(size_t)toindex];
			if (top->selfpc == selfpc) {
				/*
				 * arc at front of chain; usual case.
				 */
				top->count+= count;
				goto done;
			}
			/*
			 * have to go looking down chain for it.
			 * top points to what we are looking at,
			 * we know it is not at the head of the chain.
			 */
			for (; /* goto done */; ) {
				if (top->link == 0) {
					/*
					 * top is end of the chain and
					 * none of the chain had
					 * top->selfpc == selfpc.  so
					 * we allocate a new tostruct
					 * and link it to the head of
					 * the chain.
					 */
					toindex = ++p->tos[0].link;
					if (toindex >= p->tolimit)
						goto overflow;
					
					top = &p->tos[(size_t)toindex];
					top->selfpc = selfpc;
					top->count = count;
					top->link = *frompcindex;
					*frompcindex = (u_short)toindex;
					goto done;
				}
				/*
				 * otherwise, check the next arc on the chain.
				 */
				top = &p->tos[top->link];
				if (top->selfpc == selfpc) {
					/*
					 * there it is.
					 * add to its count.
					 */
					top->count += count;
					goto done;
				}
				
			}

		done: ;
		}

	}
 overflow: ;
 
}

static void
_m_gmon_merge(void)
{
	struct gmonparam *q;

	mutex_lock(&_gmonlock);

	for (q = _gmonfree; q != NULL;
	    q = (struct gmonparam *)(void *)q->kcount)
		_m_gmon_merge_two(&_gmonparam, q);

	for (q = _gmoninuse; q != NULL;
	    q = (struct gmonparam *)(void *)q->kcount) {
		q->state = GMON_PROF_OFF;
		_m_gmon_merge_two(&_gmonparam, q);
	}

	mutex_unlock(&_gmonlock);
}
#endif

void
_mcleanup(void)
{
	int fd;
	int fromindex;
	int endfrom;
	u_long frompc;
	int toindex;
	struct rawarc rawarc;
	struct gmonparam *p = &_gmonparam;
	struct gmonhdr gmonhdr, *hdr;
	struct clockinfo clockinfo;
#if !defined(__minix)
	int mib[2];
	size_t size;
#endif /* !defined(__minix) */
	char *profdir;
	const char *proffile;
	char  buf[PATH_MAX];
#ifdef DEBUG
	int logfd, len;
	char buf2[200];
#endif

	/*
	 * We disallow writing to the profiling file, if we are a
	 * set{u,g}id program and our effective {u,g}id does not match
	 * our real one.
	 */
	if (issetugid() && (geteuid() != getuid() || getegid() != getgid())) {
		warnx("%s: Profiling of set{u,g}id binaries is not"
		    " allowed", __func__);
		return;
	}

	if (p->state == GMON_PROF_ERROR)
		warnx("%s: tos overflow", __func__);

#if defined(__minix)
	clockinfo.profhz = sysconf(_SC_CLK_TCK);
#else
	size = sizeof(clockinfo);
	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	if (sysctl(mib, 2, &clockinfo, &size, NULL, 0) < 0) {
		/*
		 * Best guess
		 */
		clockinfo.profhz = hertz();
	} else if (clockinfo.profhz == 0) {
		if (clockinfo.hz != 0)
			clockinfo.profhz = clockinfo.hz;
		else
			clockinfo.profhz = hertz();
	}
#endif /* defined(__minix) */

	moncontrol(0);

	if ((profdir = getenv("PROFDIR")) != NULL) {
		/* If PROFDIR contains a null value, no profiling 
		   output is produced */
		if (*profdir == '\0')
			return;

		if (snprintf(buf, sizeof buf, "%s/%d.%s",
		    profdir, getpid(), getprogname()) >= (int)(sizeof buf)) {
			warnx("%s: internal buffer overflow, PROFDIR too long",
			    __func__);
			return;
		}
		
		proffile = buf;
	} else {
		proffile = "gmon.out";
	}

	fd = open(proffile , O_CREAT|O_TRUNC|O_WRONLY, 0666);
	if (fd < 0) {
		warn("%s: Cannot open `%s'", __func__, proffile);
		return;
	}
#ifdef DEBUG
	logfd = open("gmon.log", O_CREAT|O_TRUNC|O_WRONLY, 0664);
	if (logfd < 0) {
		warn("%s: Cannot open `%s'", __func__, "gmon.log");
		(void)close(fd);
		return;
	}
	len = snprintf(buf2, sizeof buf2, "[mcleanup1] kcount %p ssiz %lu\n",
	    p->kcount, p->kcountsize);
	(void)write(logfd, buf2, (size_t)len);
#endif
#ifdef _REENTRANT
	_m_gmon_merge();
#endif
	hdr = (struct gmonhdr *)&gmonhdr;
	hdr->lpc = p->lowpc;
	hdr->hpc = p->highpc;
	hdr->ncnt = (int)(p->kcountsize + sizeof(gmonhdr));
	hdr->version = GMONVERSION;
	hdr->profrate = clockinfo.profhz;
	(void)write(fd, hdr, sizeof *hdr);
	(void)write(fd, p->kcount, (size_t)p->kcountsize);
	endfrom = (int)(p->fromssize / sizeof(*p->froms));
	for (fromindex = 0; fromindex < endfrom; fromindex++) {
		if (p->froms[fromindex] == 0)
			continue;

		frompc = p->lowpc;
		frompc += fromindex * p->hashfraction * sizeof(*p->froms);
		for (toindex = p->froms[fromindex]; toindex != 0;
		     toindex = p->tos[toindex].link) {
#ifdef DEBUG
			len = snprintf(buf2, sizeof buf2,
			"[mcleanup2] frompc 0x%lx selfpc 0x%lx count %lu\n" ,
				(u_long)frompc, (u_long)p->tos[toindex].selfpc,
				(u_long)p->tos[toindex].count);
			(void)write(logfd, buf2, (size_t)len);
#endif
			rawarc.raw_frompc = frompc;
			rawarc.raw_selfpc = p->tos[toindex].selfpc;
			rawarc.raw_count = p->tos[toindex].count;
			(void)write(fd, &rawarc, sizeof rawarc);
		}
	}
	(void)close(fd);
#ifdef DEBUG
	(void)close(logfd);
#endif
}

/*
 * Control profiling
 *	profiling is what mcount checks to see if
 *	all the data structures are ready.
 */
void
moncontrol(int mode)
{
	struct gmonparam *p = &_gmonparam;

	if (mode) {
		/* start */
		profil((char *)(void *)p->kcount, (size_t)p->kcountsize,
		    p->lowpc, s_scale);
		p->state = GMON_PROF_ON;
	} else {
		/* stop */
		profil(NULL, 0, (u_long)0, 0);
		p->state = GMON_PROF_OFF;
	}
}

#if !defined(__minix)
/*
 * discover the tick frequency of the machine
 * if something goes wrong, we return 0, an impossible hertz.
 */
static int
hertz(void)
{
        struct itimerspec tim;
	timer_t t;
	int rv = 0;

        tim.it_interval.tv_sec = 0;
        tim.it_interval.tv_nsec = 1;
        tim.it_value.tv_sec = 0;
        tim.it_value.tv_nsec = 0;

	if (timer_create(CLOCK_REALTIME, NULL, &t) == -1)
		return 0;

	if (timer_settime(t, 0, &tim, NULL) == -1)
		goto out;

	if (timer_gettime(t, &tim) == -1)
		goto out;

        if (tim.it_interval.tv_nsec < 2)
		goto out;

	rv = (int)(1000000000LL / tim.it_interval.tv_nsec);
out:
	(void)timer_delete(t);
	return rv;
}
#endif /* !defined(__minix) */
