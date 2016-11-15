/* $NetBSD: vt100_base.h,v 1.1 2010/02/10 19:39:39 drochner Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define	VT100_EMUL_NARGS	10	/* max # of args to a command */

struct vt100base_data {
	const struct wsdisplay_emulops *emulops;
	void *emulcookie;
	int scrcapabilities;
	u_int nrows, ncols, crow, ccol;
	struct wsdisplay_msgattrs msgattrs;
	long defattr;			/* default attribute */

	void *cbcookie;

	int flags;
#define VTFL_LASTCHAR	0x001	/* printed last char on line (below cursor) */
#define VTFL_INSERTMODE	0x002
#define VTFL_APPLKEYPAD	0x004
#define VTFL_APPLCURSOR	0x008
#define VTFL_DECOM	0x010	/* origin mode */
#define VTFL_DECAWM	0x020	/* auto wrap */
#define VTFL_CURSORON	0x040
#define VTFL_NATCHARSET	0x080	/* national replacement charset mode */
#define VTFL_SAVEDCURS	0x100	/* we have a saved cursor state */
	long curattr, bkgdattr;		/* currently used attribute */
	int attrflags, fgcol, bgcol;	/* properties of curattr */
	u_int scrreg_startrow;
	u_int scrreg_nrows;
	char *tabs;
	char *dblwid;
	int dw;

	int nargs;
	u_int args[VT100_EMUL_NARGS]; /* numeric command args (CSI/DCS) */

	char modif1;	/* {>?} in VT100_EMUL_STATE_CSI */
	char modif2;	/* {!"$&} in VT100_EMUL_STATE_CSI */

	int dcstype;		/* substate in VT100_EMUL_STATE_STRING */
	char *dcsarg;
	int dcspos;
#define DCS_MAXLEN 256 /* ??? */
#define DCSTYPE_TABRESTORE 1 /* DCS2$t */
};

/* some useful utility macros */
#define	ARG(d, n)			((d)->args[(n)])
#define	DEF1_ARG(d, n)		(ARG(d, n) ? ARG(d, n) : 1)
#define	DEFx_ARG(d, n, x)		(ARG(d, n) ? ARG(d, n) : (x))
/* the following two can be negative if we are outside the scrolling region */
#define ROWS_ABOVE(d)	((int)(d)->crow - (int)(d)->scrreg_startrow)
#define ROWS_BELOW(d)	((int)((d)->scrreg_startrow + (d)->scrreg_nrows) \
					- (int)(d)->crow - 1)
#define CHECK_DW(d) do { \
	if ((d)->dblwid && (d)->dblwid[(d)->crow]) { \
		(d)->dw = 1; \
		if ((d)->ccol > ((d)->ncols >> 1) - 1) \
			(d)->ccol = ((d)->ncols >> 1) - 1; \
	} else \
		(d)->dw = 0; \
} while (0)
#define NCOLS(d)	((d)->ncols >> (d)->dw)
#define	COLS_LEFT(d)	(NCOLS(d) - (d)->ccol - 1)
#define COPYCOLS(d, f, t, n) (*(d)->emulops->copycols)((d)->emulcookie, \
	(d)->crow, (f) << (d)->dw, (t) << (d)->dw, (n) << (d)->dw)
#define ERASECOLS(d, f, n, a) (*(d)->emulops->erasecols)((d)->emulcookie, \
	(d)->crow, (f) << (d)->dw, (n) << (d)->dw, a)

/*
 * response to primary DA request
 * operating level: 61 = VT100, 62 = VT200, 63 = VT300
 * extensions: 1 = 132 cols, 2 = printer port, 6 = selective erase,
 *	7 = soft charset, 8 = UDKs, 9 = NRC sets
 * VT100 = "033[?1;2c"
 */
#define WSEMUL_VT_ID1 "\033[?62;6c"
/*
 * response to secondary DA request
 * ident code: 24 = VT320
 * firmware version
 * hardware options: 0 = no options
 */
#define WSEMUL_VT_ID2 "\033[>24;20;0c"

void wsemul_vt100_scrollup(struct vt100base_data *, int);
void wsemul_vt100_scrolldown(struct vt100base_data *, int);
void wsemul_vt100_ed(struct vt100base_data *, int);
void wsemul_vt100_el(struct vt100base_data *, int);
void wsemul_vt100_handle_csi(struct vt100base_data *, u_char);
void wsemul_vt100_handle_dcs(struct vt100base_data *);
