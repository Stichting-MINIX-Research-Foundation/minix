/*	$NetBSD: sem.h,v 1.19 2014/11/21 20:46:56 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	from: @(#)sem.h	8.1 (Berkeley) 6/6/93
 */

void		enddefs(void);

void		setversion(int);
void		setdefmaxusers(int, int, int);
void		setmaxusers(int);
void		setident(const char *);
int		defattr0(const char *, struct loclist *, struct attrlist *, int);
int		defattr(const char *, struct loclist *, struct attrlist *, int);
int		defiattr(const char *, struct loclist *, struct attrlist *, int);
int		defdevclass(const char *, struct loclist *, struct attrlist *, int);
void		defdev(struct devbase *, struct loclist *, struct attrlist *, int);
void		defdevattach(struct deva *, struct devbase *, struct nvlist *,
			     struct attrlist *);
struct devbase *getdevbase(const char *);
struct deva    *getdevattach(const char *);
struct attr	*mkattr(const char *);
struct attr    *getattr(const char *);
struct attr    *refattr(const char *);
int		getrefattr(const char *, struct attr **);
void		expandattr(struct attr *, void (*)(struct attr *));
void		addattr(const char *);
void		delattr(const char *);
void		selectattr(struct attr *);
void		deselectattr(struct attr *);
void		dependattrs(void);
void		setmajor(struct devbase *, int);
void		addconf(struct config *);
void		setconf(struct nvlist **, const char *, struct nvlist *);
void		delconf(const char *);
void		setfstype(const char **, const char *);
void		adddev(const char *, const char *, struct loclist *, int);
void		deldevi(const char *, const char *);
void		deldeva(const char *);
void		deldev(const char *);
void		addpseudo(const char *, int);
void		delpseudo(const char *);
void		addpseudoroot(const char *);
void		adddevm(const char *, devmajor_t, devmajor_t,
			struct condexpr *, struct nvlist *);
int		fixdevis(void);
const char     *ref(const char *);
const char     *starref(const char *);
const char     *wildref(const char *);
int		has_attr(struct attrlist *, const char *);

extern const char *s_qmark;
extern const char *s_none;
extern const char *s_ifnet;
extern size_t nattrs;
