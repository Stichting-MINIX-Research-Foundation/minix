/*	$NetBSD: rcons_subr.c,v 1.18 2009/03/14 21:04:22 dsl Exp $ */

/*
 * Copyright (c) 1991, 1993
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
 *	@(#)rcons_subr.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rcons_subr.c,v 1.18 2009/03/14 21:04:22 dsl Exp $");

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/device.h>
#include <sys/systm.h>
#else
#include "myfbdevice.h"
#endif

#include <dev/rcons/rcons.h>
#include <dev/wscons/wsdisplayvar.h>

extern void rcons_bell(struct rconsole *);

#if 0
#define RCONS_ISPRINT(c) ((((c) >= ' ') && ((c) <= '~')) || ((c) > 160))
#else
#define RCONS_ISPRINT(c) (((((c) >= ' ') && ((c) <= '~'))) || ((c) > 127))
#endif
#define RCONS_ISDIGIT(c) ((c) >= '0' && (c) <= '9')

/* Initialize our operations set */
void
rcons_init_ops(struct rconsole *rc)
{
	long tmp;
	int i, m;

	m = sizeof(rc->rc_charmap) / sizeof(rc->rc_charmap[0]);

	for (i = 0; i < m; i++)
		rc->rc_ops->mapchar(rc->rc_cookie, i, rc->rc_charmap + i);

	/* Determine which attributes the device supports. */
#ifdef RASTERCONSOLE_FGCOL
	rc->rc_deffgcolor = RASTERCONSOLE_FGCOL;
#endif
#ifdef RASTERCONSOLE_BGCOL
	rc->rc_defbgcolor = RASTERCONSOLE_BGCOL;
#endif
	rc->rc_fgcolor = rc->rc_deffgcolor;
	rc->rc_bgcolor = rc->rc_defbgcolor;
	rc->rc_supwsflg = 0;

	for (i = 1; i < 256; i <<= 1)
		if (rc->rc_ops->allocattr(rc->rc_cookie, 0, 0, i, &tmp) == 0)
			rc->rc_supwsflg |= i;

	/* Allocate kernel output attribute */
	rc->rc_wsflg = WSATTR_HILIT;
	rcons_setcolor(rc, rc->rc_deffgcolor, rc->rc_defbgcolor);
	rc->rc_kern_attr = rc->rc_attr;

	rc->rc_wsflg = 0;
	rcons_setcolor(rc, rc->rc_deffgcolor, rc->rc_defbgcolor);
	rc->rc_defattr = rc->rc_attr;
}

/* Output (or at least handle) a string sent to the console */
void
rcons_puts(struct rconsole *rc, const unsigned char *str, int n)
{
	int c, i, j;
	const unsigned char *cp;

	/* Jump scroll */
	/* XXX maybe this should be an option? */
	if ((rc->rc_bits & FB_INESC) == 0) {
		/* Count newlines up to an escape sequence */
		i = 0;
		j = 0;
		for (cp = str; j++ < n && *cp != '\033'; ++cp) {
			if (*cp == '\n')
				++i;
			else if (*cp == '\013')
				--i;
		}

		/* Only jump scroll two or more rows */
		if (rc->rc_row + i > rc->rc_maxrow + 1) {
			/* Erase the cursor (if necessary) */
			if (rc->rc_bits & FB_CURSOR)
				rcons_cursor(rc);

			rcons_scroll(rc, i);
		}
	}

	/* Process characters */
	while (--n >= 0) {
		c = *str;
		if (c == '\033') {
			/* Start an escape (perhaps aborting one in progress) */
			rc->rc_bits |= FB_INESC | FB_P0_DEFAULT | FB_P1_DEFAULT;
			rc->rc_bits &= ~(FB_P0 | FB_P1);

			/* Most parameters default to 1 */
			rc->rc_p0 = rc->rc_p1 = 1;
		} else if (rc->rc_bits & FB_INESC) {
			rcons_esc(rc, c);
		} else {
			/* Erase the cursor (if necessary) */
			if (rc->rc_bits & FB_CURSOR)
				rcons_cursor(rc);

			/* Display the character */
			if (RCONS_ISPRINT(c)) {
				/* Try to output as much as possible */
				j = rc->rc_maxcol - rc->rc_col;
				if (j > n)
					j = n;
				for (i = 1; i < j && RCONS_ISPRINT(str[i]); ++i)
					continue;
				rcons_text(rc, str, i);
				--i;
				str += i;
				n -= i;
			} else
				rcons_pctrl(rc, c);
		}
		++str;
	}
	/* Redraw the cursor (if necessary) */
	if ((rc->rc_bits & FB_CURSOR) == 0)
		rcons_cursor(rc);
}


/* Handle a control character sent to the console */
void
rcons_pctrl(struct rconsole *rc, int c)
{

	switch (c) {
	case '\r':	/* Carriage return */
		rc->rc_col = 0;
		break;

	case '\b':	/* Backspace */
		if (rc->rc_col > 0)
			(rc->rc_col)--;
		break;

	case '\v':	/* Vertical tab */
		if (rc->rc_row > 0)
			(rc->rc_row)--;
		break;

	case '\f':	/* Formfeed */
		rc->rc_row = rc->rc_col = 0;
		rcons_clear2eop(rc);
		break;

	case '\n':	/* Linefeed */
		(rc->rc_row)++;
		if (rc->rc_row >= rc->rc_maxrow)
			rcons_scroll(rc, 1);
		break;

	case '\a':	/* Bell */
		rcons_bell(rc);
		break;

	case '\t':	/* Horizontal tab */
		rc->rc_col = (rc->rc_col + 8) & ~7;
		if (rc->rc_col >= rc->rc_maxcol)
			rc->rc_col = rc->rc_maxcol;
		break;
	}
}

/* Handle the next character in an escape sequence */
void
rcons_esc(struct rconsole *rc, int c)
{

	if (c == '[') {
		/* Parameter 0 */
		rc->rc_bits &= ~FB_P1;
		rc->rc_bits |= FB_P0;
	} else if (c == ';') {
		/* Parameter 1 */
		rc->rc_bits &= ~FB_P0;
		rc->rc_bits |= FB_P1;
	} else if (RCONS_ISDIGIT(c)) {
		/* Add a digit to a parameter */
		if (rc->rc_bits & FB_P0) {
			/* Parameter 0 */
			if (rc->rc_bits & FB_P0_DEFAULT) {
				rc->rc_bits &= ~FB_P0_DEFAULT;
				rc->rc_p0 = 0;
			}
			rc->rc_p0 *= 10;
			rc->rc_p0 += c - '0';
		} else if (rc->rc_bits & FB_P1) {
			/* Parameter 1 */
			if (rc->rc_bits & FB_P1_DEFAULT) {
				rc->rc_bits &= ~FB_P1_DEFAULT;
				rc->rc_p1 = 0;
			}
			rc->rc_p1 *= 10;
			rc->rc_p1 += c - '0';
		}
	} else {
		/* Erase the cursor (if necessary) */
		if (rc->rc_bits & FB_CURSOR)
			rcons_cursor(rc);

		/* Process the completed escape sequence */
		rcons_doesc(rc, c);
		rc->rc_bits &= ~FB_INESC;
	}
}


/* Handle an SGR (Select Graphic Rendition) escape */
void
rcons_sgresc(struct rconsole *rc, int c)
{

	switch (c) {
	/* Clear all attributes || End underline */
	case 0:
		rc->rc_wsflg = 0;
		rc->rc_fgcolor = rc->rc_deffgcolor;
		rc->rc_bgcolor = rc->rc_defbgcolor;
		rc->rc_attr = rc->rc_defattr;
		break;

	/* ANSI foreground color */
	case 30: case 31: case 32: case 33:
	case 34: case 35: case 36: case 37:
		rcons_setcolor(rc, c - 30, rc->rc_bgcolor);
		break;

	/* ANSI background color */
	case 40: case 41: case 42: case 43:
	case 44: case 45: case 46: case 47:
		rcons_setcolor(rc, rc->rc_fgcolor, c - 40);
		break;

	/* Begin reverse */
	case 7:
		rc->rc_wsflg |= WSATTR_REVERSE;
		rcons_setcolor(rc, rc->rc_fgcolor, rc->rc_bgcolor);
		break;

	/* Begin bold */
	case 1:
		rc->rc_wsflg |= WSATTR_HILIT;
		rcons_setcolor(rc, rc->rc_fgcolor, rc->rc_bgcolor);
		break;

	/* Begin underline */
	case 4:
		rc->rc_wsflg |= WSATTR_UNDERLINE;
		rcons_setcolor(rc, rc->rc_fgcolor, rc->rc_bgcolor);
		break;
	}
}


/* Process a complete escape sequence */
void
rcons_doesc(struct rconsole *rc, int c)
{

#ifdef notdef
	/* XXX add escape sequence to enable visual (and audible) bell */
	rc->rc_bits = FB_VISBELL;
#endif

	switch (c) {

	case '@':
		/* Insert Character (ICH) */
		rcons_insertchar(rc, rc->rc_p0);
		break;

	case 'A':
		/* Cursor Up (CUU) */
		if (rc->rc_row >= rc->rc_p0)
			rc->rc_row -= rc->rc_p0;
		else
			rc->rc_row = 0;
		break;

	case 'B':
		/* Cursor Down (CUD) */
		rc->rc_row += rc->rc_p0;
		if (rc->rc_row >= rc->rc_maxrow)
			rc->rc_row = rc->rc_maxrow - 1;
		break;

	case 'C':
		/* Cursor Forward (CUF) */
		rc->rc_col += rc->rc_p0;
		if (rc->rc_col >= rc->rc_maxcol)
			rc->rc_col = rc->rc_maxcol - 1;
		break;

	case 'D':
		/* Cursor Backward (CUB) */
		if (rc->rc_col >= rc->rc_p0)
			rc->rc_col -= rc->rc_p0;
		else
			rc->rc_col = 0;
		break;

	case 'E':
		/* Cursor Next Line (CNL) */
		rc->rc_col = 0;
		rc->rc_row += rc->rc_p0;
		if (rc->rc_row >= rc->rc_maxrow)
			rc->rc_row = rc->rc_maxrow - 1;
		break;

	case 'f':
		/* Horizontal And Vertical Position (HVP) */
	case 'H':
		/* Cursor Position (CUP) */
		rc->rc_col = MIN(MAX(rc->rc_p1, 1), rc->rc_maxcol) - 1;
		rc->rc_row = MIN(MAX(rc->rc_p0, 1), rc->rc_maxrow) - 1;
		break;

	case 'J':
		/* Erase in Display (ED) */
		rcons_clear2eop(rc);
		break;

	case 'K':
		/* Erase in Line (EL) */
		rcons_clear2eol(rc);
		break;

	case 'L':
		/* Insert Line (IL) */
		rcons_insertline(rc, rc->rc_p0);
		break;

	case 'M':
		/* Delete Line (DL) */
		rcons_delline(rc, rc->rc_p0);
		break;

	case 'P':
		/* Delete Character (DCH) */
		rcons_delchar(rc, rc->rc_p0);
		break;

	case 'm':
		/* Select Graphic Rendition (SGR) */
		/* (defaults to zero) */
		if (rc->rc_bits & FB_P0_DEFAULT)
			rc->rc_p0 = 0;

		if (rc->rc_bits & FB_P1_DEFAULT)
			rc->rc_p1 = 0;

		rcons_sgresc(rc, rc->rc_p0);

		if (rc->rc_bits & FB_P1)
			rcons_sgresc(rc, rc->rc_p1);

		break;

	/*
	 * XXX: setting SUNBOW and SUNWOB should probably affect
	 * deffgcolor, defbgcolor and defattr too.
	 */
	case 'p':
		/* Black On White (SUNBOW) */
		rcons_setcolor(rc, WSCOL_BLACK, WSCOL_WHITE);
		break;

	case 'q':
		/* White On Black (SUNWOB) */
		rcons_setcolor(rc, WSCOL_WHITE, WSCOL_BLACK);
		break;

	case 'r':
		/* Set scrolling (SUNSCRL) */
		/* (defaults to zero) */
		if (rc->rc_bits & FB_P0_DEFAULT)
			rc->rc_p0 = 0;
		/* XXX not implemented yet */
		rc->rc_scroll = rc->rc_p0;
		break;

	case 's':
		/* Reset terminal emulator (SUNRESET) */
		rc->rc_wsflg = 0;
		rc->rc_scroll = 0;
		rc->rc_bits &= ~FB_NO_CURSOR;
		rc->rc_fgcolor = rc->rc_deffgcolor;
		rc->rc_bgcolor = rc->rc_defbgcolor;
		rc->rc_attr = rc->rc_defattr;

		if (rc->rc_bits & FB_INVERT)
			rcons_invert(rc, 0);
		break;
#ifdef notyet
	/*
	 * XXX following two read \E[?25h and \E[?25l. rcons
	 * can't currently handle the '?'.
	 */
	case 'h':
		/* Normal/very visible cursor */
		if (rc->rc_p0 == 25) {
			rc->rc_bits &= ~FB_NO_CURSOR;

			if (rc->rc_bits & FB_CURSOR) {
				rc->rc_bits ^= FB_CURSOR;
				rcons_cursor(rc);
			}
		}
		break;

	case 'l':
		/* Invisible cursor */
		if (rc->rc_p0 == 25 && (rc->rc_bits & FB_NO_CURSOR) == 0) {
			if (rc->rc_bits & FB_CURSOR)
				rcons_cursor(rc);

			rc->rc_bits |= FB_NO_CURSOR;
		}
		break;
#endif
	}
}

/* Set ANSI colors */
void
rcons_setcolor(struct rconsole *rc, int fg, int bg)
{
	int flg;

	if (fg > WSCOL_WHITE || fg < 0)
		return;

	if (bg > WSCOL_WHITE || bg < 0)
		return;

#ifdef RASTERCONS_WONB
	flg = bg;
	bg = fg;
	fg = flg;
#endif

	/* Emulate WSATTR_REVERSE attribute if it's not supported */
	if ((rc->rc_wsflg & WSATTR_REVERSE) &&
	    !(rc->rc_supwsflg & WSATTR_REVERSE)) {
		flg = bg;
		bg = fg;
		fg = flg;
	}

	/*
	 * Mask out unsupported flags and get attribute
	 * XXX - always ask for WSCOLORS if supported (why shouldn't we?)
	 */
	flg = (rc->rc_wsflg | WSATTR_WSCOLORS) & rc->rc_supwsflg;
	rc->rc_bgcolor = bg;
	rc->rc_fgcolor = fg;
	rc->rc_ops->allocattr(rc->rc_cookie, fg, bg, flg, &rc->rc_attr);
}


/* Actually write a string to the frame buffer */
void
rcons_text(struct rconsole *rc, const unsigned char *str, int n)
{
	u_int uc;

	while (n--) {
		uc = rc->rc_charmap[*str++ & 255];
		rc->rc_ops->putchar(rc->rc_cookie, rc->rc_row, rc->rc_col++,
		    uc, rc->rc_attr);
	}

	if (rc->rc_col >= rc->rc_maxcol) {
		rc->rc_col = 0;
		rc->rc_row++;
	}

	if (rc->rc_row >= rc->rc_maxrow)
		rcons_scroll(rc, 1);
}

/* Paint (or unpaint) the cursor */
void
rcons_cursor(struct rconsole *rc)
{
	rc->rc_bits ^= FB_CURSOR;

	if (rc->rc_bits & FB_NO_CURSOR)
		return;

	rc->rc_ops->cursor(rc->rc_cookie, rc->rc_bits & FB_CURSOR,
	    rc->rc_row, rc->rc_col);
}

/* Possibly change to SUNWOB or SUNBOW mode */
void
rcons_invert(struct rconsole *rc, int wob)
{

	rc->rc_bits ^= FB_INVERT;
	/* XXX how do we do we invert the framebuffer?? */
}

/* Clear to the end of the page */
void
rcons_clear2eop(struct rconsole *rc)
{
	if (rc->rc_col || rc->rc_row) {
		rcons_clear2eol(rc);

		if (rc->rc_row < (rc->rc_maxrow - 1))
			rc->rc_ops->eraserows(rc->rc_cookie, rc->rc_row + 1,
			    rc->rc_maxrow, rc->rc_attr);
	} else
		rc->rc_ops->eraserows(rc->rc_cookie, 0, rc->rc_maxrow,
		    rc->rc_attr);
}

/* Clear to the end of the line */
void
rcons_clear2eol(struct rconsole *rc)
{
	rc->rc_ops->erasecols(rc->rc_cookie, rc->rc_row, rc->rc_col,
	    rc->rc_maxcol - rc->rc_col, rc->rc_attr);
}


/* Scroll up */
void
rcons_scroll(struct rconsole *rc, int n)
{
	/* Can't scroll more than the whole screen */
	if (n > rc->rc_maxrow)
		n = rc->rc_maxrow;

	/* Calculate new row */
	if (rc->rc_row >= n)
		rc->rc_row -= n;
	else 
		rc->rc_row = 0;

	rc->rc_ops->copyrows(rc->rc_cookie, n, 0, rc->rc_maxrow - n);
	rc->rc_ops->eraserows(rc->rc_cookie, rc->rc_maxrow - n, n,  rc->rc_attr);
}

/* Delete characters */
void
rcons_delchar(struct rconsole *rc, int n)
{
	/* Can't delete more chars than there are */
	if (n > rc->rc_maxcol - rc->rc_col)
		n = rc->rc_maxcol - rc->rc_col;

	rc->rc_ops->copycols(rc->rc_cookie, rc->rc_row, rc->rc_col + n,
	    rc->rc_col, rc->rc_maxcol - rc->rc_col - n);

	rc->rc_ops->erasecols(rc->rc_cookie, rc->rc_row,
	    rc->rc_maxcol - n, n, rc->rc_attr);
}

/* Delete a number of lines */
void
rcons_delline(struct rconsole *rc, int n)
{
	/* Can't delete more lines than there are */
	if (n > rc->rc_maxrow - rc->rc_row)
		n = rc->rc_maxrow - rc->rc_row;

	rc->rc_ops->copyrows(rc->rc_cookie, rc->rc_row + n, rc->rc_row,
	    rc->rc_maxrow - rc->rc_row - n);

	rc->rc_ops->eraserows(rc->rc_cookie, rc->rc_maxrow - n, n,
	    rc->rc_attr);
}

/* Insert some characters */
void
rcons_insertchar(struct rconsole *rc, int n)
{
	/* Can't insert more chars than can fit */
	if (n > rc->rc_maxcol - rc->rc_col)
		n = rc->rc_maxcol - rc->rc_col - 1;

	rc->rc_ops->copycols(rc->rc_cookie, rc->rc_row, rc->rc_col,
	    rc->rc_col + n, rc->rc_maxcol - rc->rc_col - n - 1);

	rc->rc_ops->erasecols(rc->rc_cookie, rc->rc_row, rc->rc_col,
	    n, rc->rc_attr);
}

/* Insert some lines */
void
rcons_insertline(struct rconsole *rc, int n)
{
	/* Can't insert more lines than can fit */
	if (n > rc->rc_maxrow - rc->rc_row)
		n = rc->rc_maxrow - rc->rc_row;

	rc->rc_ops->copyrows(rc->rc_cookie, rc->rc_row, rc->rc_row + n,
	    rc->rc_maxrow - rc->rc_row - n);

	rc->rc_ops->eraserows(rc->rc_cookie, rc->rc_row, n,
	    rc->rc_attr);
}

/* end of rcons_subr.c */
