/*	$NetBSD: rcons.h,v 1.16 2005/12/11 12:23:44 christos Exp $ */

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
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)fbvar.h	8.1 (Berkeley) 6/11/93
 */
#ifndef _RCONS_H_
#define _RCONS_H_ 1

#include "opt_rcons.h"

#include <sys/callout.h>

/* Avoid dragging in dev/wscons/wsdisplayvar.h */
struct wsdisplay_emulops;

struct rconsole {
	/* This section must be filled in by the framebugger device */
	u_int	rc_maxrow;		/* emulator height of screen */
	u_int	rc_maxcol;		/* emulator width of screen */
	void	(*rc_bell)(int);	/* ring the bell */
	struct	wsdisplay_emulops *rc_ops;/* output ops */
	void	*rc_cookie;		/* cookie thereof */
	u_int	rc_width;		/* width in pixels */
	u_int	rc_height;		/* height in pixels */
	u_int	rc_row;			/* emulator row */
	u_int	rc_col;			/* emulator column */

	struct callout rc_belltmr_ch;

	/* These may be overridden in the kernel config file. */
	int	rc_deffgcolor;		/* default fg color */
	int	rc_defbgcolor;		/* default bg color */

	/* Bits maintained by the raster routines */
	u_int	rc_bits;		/* see defines below */
	int	rc_ringing;		/* bell currently ringing */
	int	rc_belldepth;		/* audible bell depth */
	int	rc_scroll;		/* stupid sun scroll mode */
	int	rc_p0;			/* escape sequence parameter 0 */
	int	rc_p1;			/* escape sequence parameter 1 */
	int	rc_fgcolor;		/* current fg color */
	int	rc_bgcolor;		/* current bg color */
	long	rc_attr;		/* current wscons text attribute */
	long	rc_defattr;		/* default text attribute */
	long	rc_kern_attr;		/* kernel output attribute */
	u_int	rc_wsflg;		/* wscons attribute flags */
	u_int	rc_supwsflg;		/* supported attribute flags */
	u_int   rc_charmap[256];	/* ASCII->emulator char map */
};

#define FB_INESC	0x001		/* processing an escape sequence */
#define FB_VISBELL	0x002		/* visual bell */
#define FB_CURSOR	0x004		/* cursor is visible */
#define FB_INVERT	0x008		/* framebuffer inverted */
#define FB_NO_CURSOR	0x010		/* cursor is disabled */

#define FB_P0_DEFAULT	0x100		/* param 0 is defaulted */
#define FB_P1_DEFAULT	0x200		/* param 1 is defaulted */
#define FB_P0		0x400		/* working on param 0 */
#define FB_P1		0x800		/* working on param 1 */

/* rcons_kern.c */
void rcons_cnputc(int);
void rcons_bell(struct rconsole *);
void rcons_init(struct rconsole *, int);
void rcons_ttyinit(struct tty *);

/* rcons_subr.c */
void rcons_init_ops(struct rconsole *rc);
void rcons_puts(struct rconsole *, const unsigned char *, int);
void rcons_pctrl(struct rconsole *, int);
void rcons_esc(struct rconsole *, int);
void rcons_doesc(struct rconsole *, int);
void rcons_sgresc(struct rconsole *, int);
void rcons_text(struct rconsole *, const unsigned char *, int);
void rcons_cursor(struct rconsole *);
void rcons_invert(struct rconsole *, int);
void rcons_clear2eop(struct rconsole *);
void rcons_clear2eol(struct rconsole *);
void rcons_scroll(struct rconsole *, int);
void rcons_delchar(struct rconsole *, int);
void rcons_delline(struct rconsole *, int);
void rcons_insertchar(struct rconsole *, int);
void rcons_insertline(struct rconsole *, int);
void rcons_setcolor(struct rconsole *, int, int);

#endif /* !defined _RCONS_H_ */
