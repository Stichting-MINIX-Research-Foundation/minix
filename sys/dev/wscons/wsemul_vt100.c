/* $NetBSD: wsemul_vt100.c,v 1.37 2015/08/24 22:50:33 pooka Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wsemul_vt100.c,v 1.37 2015/08/24 22:50:33 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_wsmsgattrs.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsemulvar.h>
#include <dev/wscons/wsemul_vt100var.h>
#include <dev/wscons/ascii.h>

void	*wsemul_vt100_cnattach(const struct wsscreen_descr *, void *,
			       int, int, long);
void	*wsemul_vt100_attach(int console, const struct wsscreen_descr *,
			     void *, int, int, void *, long);
void	wsemul_vt100_output(void *cookie, const u_char *data, u_int count, int);
void	wsemul_vt100_detach(void *cookie, u_int *crowp, u_int *ccolp);
void	wsemul_vt100_resetop(void *, enum wsemul_resetops);
#ifdef WSDISPLAY_CUSTOM_OUTPUT
static void wsemul_vt100_getmsgattrs(void *, struct wsdisplay_msgattrs *);
static void wsemul_vt100_setmsgattrs(void *, const struct wsscreen_descr *,
                                     const struct wsdisplay_msgattrs *);
#endif /* WSDISPLAY_CUSTOM_OUTPUT */

const struct wsemul_ops wsemul_vt100_ops = {
	"vt100",
	wsemul_vt100_cnattach,
	wsemul_vt100_attach,
	wsemul_vt100_output,
	wsemul_vt100_translate,
	wsemul_vt100_detach,
	wsemul_vt100_resetop,
#ifdef WSDISPLAY_CUSTOM_OUTPUT
	wsemul_vt100_getmsgattrs,
	wsemul_vt100_setmsgattrs,
#else
	NULL,
	NULL,
#endif
};

struct wsemul_vt100_emuldata wsemul_vt100_console_emuldata;

static void wsemul_vt100_init(struct wsemul_vt100_emuldata *,
			      const struct wsscreen_descr *,
			      void *, int, int, long);

static void wsemul_vt100_output_normal(struct wsemul_vt100_emuldata *,
				       u_char, int);
static void wsemul_vt100_output_c0c1(struct wsemul_vt100_emuldata *,
				     u_char, int);
static void wsemul_vt100_nextline(struct wsemul_vt100_emuldata *);
typedef u_int vt100_handler(struct wsemul_vt100_emuldata *, u_char);

static vt100_handler
wsemul_vt100_output_esc,
wsemul_vt100_output_csi,
wsemul_vt100_output_scs94,
wsemul_vt100_output_scs94_percent,
wsemul_vt100_output_scs96,
wsemul_vt100_output_scs96_percent,
wsemul_vt100_output_esc_hash,
wsemul_vt100_output_esc_spc,
wsemul_vt100_output_string,
wsemul_vt100_output_string_esc,
wsemul_vt100_output_dcs,
wsemul_vt100_output_dcs_dollar;

#define	VT100_EMUL_STATE_NORMAL		0	/* normal processing */
#define	VT100_EMUL_STATE_ESC		1	/* got ESC */
#define	VT100_EMUL_STATE_CSI		2	/* got CSI (ESC[) */
#define	VT100_EMUL_STATE_SCS94		3	/* got ESC{()*+} */
#define	VT100_EMUL_STATE_SCS94_PERCENT	4	/* got ESC{()*+}% */
#define	VT100_EMUL_STATE_SCS96		5	/* got ESC{-./} */
#define	VT100_EMUL_STATE_SCS96_PERCENT	6	/* got ESC{-./}% */
#define	VT100_EMUL_STATE_ESC_HASH	7	/* got ESC# */
#define	VT100_EMUL_STATE_ESC_SPC	8	/* got ESC<SPC> */
#define	VT100_EMUL_STATE_STRING		9	/* waiting for ST (ESC\) */
#define	VT100_EMUL_STATE_STRING_ESC	10	/* waiting for ST, got ESC */
#define	VT100_EMUL_STATE_DCS		11	/* got DCS (ESC P) */
#define	VT100_EMUL_STATE_DCS_DOLLAR	12	/* got DCS<p>$ */

vt100_handler *vt100_output[] = {
	wsemul_vt100_output_esc,
	wsemul_vt100_output_csi,
	wsemul_vt100_output_scs94,
	wsemul_vt100_output_scs94_percent,
	wsemul_vt100_output_scs96,
	wsemul_vt100_output_scs96_percent,
	wsemul_vt100_output_esc_hash,
	wsemul_vt100_output_esc_spc,
	wsemul_vt100_output_string,
	wsemul_vt100_output_string_esc,
	wsemul_vt100_output_dcs,
	wsemul_vt100_output_dcs_dollar,
};

static void
wsemul_vt100_init(struct wsemul_vt100_emuldata *edp,
	const struct wsscreen_descr *type, void *cookie, int ccol, int crow,
	long defattr)
{
	struct vt100base_data *vd = &edp->bd;
	int error;

	vd->emulops = type->textops;
	vd->emulcookie = cookie;
	vd->scrcapabilities = type->capabilities;
	vd->nrows = type->nrows;
	vd->ncols = type->ncols;
	vd->crow = crow;
	vd->ccol = ccol;

	/* The underlying driver has already allocated a default and simple
	 * attribute for us, which is stored in defattr.  We try to set the
	 * values specified by the kernel options below, but in case of
	 * failure we fallback to the value given by the driver. */

	if (type->capabilities & WSSCREEN_WSCOLORS) {
		vd->msgattrs.default_attrs = WS_DEFAULT_COLATTR |
		    WSATTR_WSCOLORS;
		vd->msgattrs.default_bg = WS_DEFAULT_BG;
		vd->msgattrs.default_fg = WS_DEFAULT_FG;

		vd->msgattrs.kernel_attrs = WS_KERNEL_COLATTR |
		    WSATTR_WSCOLORS;
		vd->msgattrs.kernel_bg = WS_KERNEL_BG;
		vd->msgattrs.kernel_fg = WS_KERNEL_FG;
	} else {
		vd->msgattrs.default_attrs = WS_DEFAULT_MONOATTR;
		vd->msgattrs.default_bg = vd->msgattrs.default_fg = 0;

		vd->msgattrs.kernel_attrs = WS_KERNEL_MONOATTR;
		vd->msgattrs.kernel_bg = vd->msgattrs.kernel_fg = 0;
	}

	error = (*vd->emulops->allocattr)(cookie,
					   vd->msgattrs.default_fg,
					   vd->msgattrs.default_bg,
					   vd->msgattrs.default_attrs,
					   &vd->defattr);
	if (error) {
		vd->defattr = defattr;
		/* XXX This assumes the driver has allocated white on black
		 * XXX as the default attribute, which is not always true.
		 * XXX Maybe we need an emulop that, given an attribute,
		 * XXX (defattr) returns its flags and colors? */
		vd->msgattrs.default_attrs = 0;
		vd->msgattrs.default_bg = WSCOL_BLACK;
		vd->msgattrs.default_fg = WSCOL_WHITE;
	} else {
		if (vd->emulops->replaceattr != NULL)
			(*vd->emulops->replaceattr)(cookie, defattr,
			                             vd->defattr);
	}

#if defined(WS_KERNEL_CUSTOMIZED)
	/* Set up kernel colors, in case they were customized by the user;
	 * otherwise default to the colors specified for the console.
	 * In case of failure, we use console colors too; we can assume
	 * they are good as they have been previously allocated and
	 * verified. */
	error = (*vd->emulops->allocattr)(cookie,
					   vd->msgattrs.kernel_fg,
					   vd->msgattrs.kernel_bg,
					   vd->msgattrs.kernel_attrs,
					   &edp->kernattr);
	if (error)
#endif
	edp->kernattr = vd->defattr;
}

void *
wsemul_vt100_cnattach(const struct wsscreen_descr *type, void *cookie,
	int ccol, int crow, long defattr)
{
	struct wsemul_vt100_emuldata *edp;
	struct vt100base_data *vd;

	edp = &wsemul_vt100_console_emuldata;
	vd = &edp->bd;
	wsemul_vt100_init(edp, type, cookie, ccol, crow, defattr);
#ifdef DIAGNOSTIC
	edp->console = 1;
#endif
	vd->cbcookie = NULL;

	vd->tabs = 0;
	vd->dblwid = 0;
	vd->dw = 0;
	vd->dcsarg = 0;
	edp->isolatin1tab = edp->decgraphtab = edp->dectechtab = 0;
	edp->nrctab = 0;
	wsemul_vt100_reset(edp);
	return (edp);
}

void *
wsemul_vt100_attach(int console, const struct wsscreen_descr *type,
	void *cookie, int ccol, int crow, void *cbcookie, long defattr)
{
	struct wsemul_vt100_emuldata *edp;
	struct vt100base_data *vd;

	if (console) {
		edp = &wsemul_vt100_console_emuldata;
#ifdef DIAGNOSTIC
		KASSERT(edp->console == 1);
#endif
	} else {
		edp = malloc(sizeof *edp, M_DEVBUF, M_WAITOK);
		wsemul_vt100_init(edp, type, cookie, ccol, crow, defattr);
#ifdef DIAGNOSTIC
		edp->console = 0;
#endif
	}
	vd = &edp->bd;
	vd->cbcookie = cbcookie;

	vd->tabs = malloc(vd->ncols, M_DEVBUF, M_NOWAIT);
	vd->dblwid = malloc(vd->nrows, M_DEVBUF, M_NOWAIT|M_ZERO);
	vd->dw = 0;
	vd->dcsarg = malloc(DCS_MAXLEN, M_DEVBUF, M_NOWAIT);
	edp->isolatin1tab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	edp->decgraphtab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	edp->dectechtab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	edp->nrctab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	vt100_initchartables(edp);
	wsemul_vt100_reset(edp);
	return (edp);
}

void
wsemul_vt100_detach(void *cookie, u_int *crowp, u_int *ccolp)
{
	struct wsemul_vt100_emuldata *edp = cookie;
	struct vt100base_data *vd = &edp->bd;

	*crowp = vd->crow;
	*ccolp = vd->ccol;
#define f(ptr) if (ptr) {free(ptr, M_DEVBUF); ptr = 0;}
	f(vd->tabs)
	f(vd->dblwid)
	f(vd->dcsarg)
	f(edp->isolatin1tab)
	f(edp->decgraphtab)
	f(edp->dectechtab)
	f(edp->nrctab)
#undef f
	if (edp != &wsemul_vt100_console_emuldata)
		free(edp, M_DEVBUF);
}

void
wsemul_vt100_resetop(void *cookie, enum wsemul_resetops op)
{
	struct wsemul_vt100_emuldata *edp = cookie;
	struct vt100base_data *vd = &edp->bd;

	switch (op) {
	case WSEMUL_RESET:
		wsemul_vt100_reset(edp);
		break;
	case WSEMUL_SYNCFONT:
		vt100_initchartables(edp);
		break;
	case WSEMUL_CLEARSCREEN:
		wsemul_vt100_ed(vd, 2);
		vd->ccol = vd->crow = 0;
		(*vd->emulops->cursor)(vd->emulcookie,
					vd->flags & VTFL_CURSORON, 0, 0);
		break;
	default:
		break;
	}
}

void
wsemul_vt100_reset(struct wsemul_vt100_emuldata *edp)
{
	struct vt100base_data *vd = &edp->bd;
	int i;

	edp->state = VT100_EMUL_STATE_NORMAL;
	vd->flags = VTFL_DECAWM | VTFL_CURSORON;
	vd->bkgdattr = vd->curattr = vd->defattr;
	vd->attrflags = vd->msgattrs.default_attrs;
	vd->fgcol = vd->msgattrs.default_fg;
	vd->bgcol = vd->msgattrs.default_bg;
	vd->scrreg_startrow = 0;
	vd->scrreg_nrows = vd->nrows;
	if (vd->tabs) {
		memset(vd->tabs, 0, vd->ncols);
		for (i = 8; i < vd->ncols; i += 8)
			vd->tabs[i] = 1;
	}
	vd->dcspos = 0;
	vd->dcstype = 0;
	edp->chartab_G[0] = 0;
	edp->chartab_G[1] = edp->nrctab; /* ??? */
	edp->chartab_G[2] = edp->isolatin1tab;
	edp->chartab_G[3] = edp->isolatin1tab;
	edp->chartab0 = 0;
	edp->chartab1 = 2;
	edp->sschartab = 0;
}

/*
 * now all the state machine bits
 */

/*
 * Move the cursor to the next line if possible. If the cursor is at
 * the bottom of the scroll area, then scroll it up. If the cursor is
 * at the bottom of the screen then don't move it down.
 */
static void
wsemul_vt100_nextline(struct wsemul_vt100_emuldata *edp)
{
	struct vt100base_data *vd = &edp->bd;

	if (ROWS_BELOW(vd) == 0) {
		/* Bottom of the scroll region. */
	  	wsemul_vt100_scrollup(vd, 1);
	} else {
		if ((vd->crow+1) < vd->nrows)
			/* Cursor not at the bottom of the screen. */
			vd->crow++;
		CHECK_DW(vd);
	}
}

static void
wsemul_vt100_output_normal(struct wsemul_vt100_emuldata *edp, u_char c,
	int kernel)
{
	struct vt100base_data *vd = &edp->bd;
	u_int *ct, dc;

	if ((vd->flags & (VTFL_LASTCHAR | VTFL_DECAWM)) ==
	    (VTFL_LASTCHAR | VTFL_DECAWM)) {
		wsemul_vt100_nextline(edp);
		vd->ccol = 0;
		vd->flags &= ~VTFL_LASTCHAR;
	}

	if (c & 0x80) {
		c &= 0x7f;
		ct = edp->chartab_G[edp->chartab1];
	} else {
		if (edp->sschartab) {
			ct = edp->chartab_G[edp->sschartab];
			edp->sschartab = 0;
		} else
			ct = edp->chartab_G[edp->chartab0];
	}
	dc = (ct ? ct[c] : c);

	if ((vd->flags & VTFL_INSERTMODE) && COLS_LEFT(vd))
		COPYCOLS(vd, vd->ccol, vd->ccol + 1, COLS_LEFT(vd));

	(*vd->emulops->putchar)(vd->emulcookie, vd->crow,
				 vd->ccol << vd->dw, dc,
				 kernel ? edp->kernattr : vd->curattr);

	if (COLS_LEFT(vd))
		vd->ccol++;
	else
		vd->flags |= VTFL_LASTCHAR;
}

static void
wsemul_vt100_output_c0c1(struct wsemul_vt100_emuldata *edp, u_char c,
	int kernel)
{
	struct vt100base_data *vd = &edp->bd;
	u_int n;

	switch (c) {
	case ASCII_NUL:
	default:
		/* ignore */
		break;
	case ASCII_BEL:
		wsdisplay_emulbell(vd->cbcookie);
		break;
	case ASCII_BS:
		if (vd->ccol > 0) {
			vd->ccol--;
			vd->flags &= ~VTFL_LASTCHAR;
		}
		break;
	case ASCII_CR:
		vd->ccol = 0;
		vd->flags &= ~VTFL_LASTCHAR;
		break;
	case ASCII_HT:
		if (vd->tabs) {
			if (!COLS_LEFT(vd))
				break;
			for (n = vd->ccol + 1; n < NCOLS(vd) - 1; n++)
				if (vd->tabs[n])
					break;
		} else {
			n = vd->ccol + min(8 - (vd->ccol & 7), COLS_LEFT(vd));
		}
		vd->ccol = n;
		break;
	case ASCII_SO: /* LS1 */
		edp->chartab0 = 1;
		break;
	case ASCII_SI: /* LS0 */
		edp->chartab0 = 0;
		break;
	case ASCII_ESC:
		if (kernel) {
			printf("wsemul_vt100_output_c0c1: ESC in kernel output ignored\n");
			break;	/* ignore the ESC */
		}

		if (edp->state == VT100_EMUL_STATE_STRING) {
			/* might be a string end */
			edp->state = VT100_EMUL_STATE_STRING_ESC;
		} else {
			/* XXX cancel current escape sequence */
			edp->state = VT100_EMUL_STATE_ESC;
		}
		break;
#if 0
	case CSI: /* 8-bit */
		/* XXX cancel current escape sequence */
		edp->nargs = 0;
		memset(edp->args, 0, sizeof (edp->args));
		edp->modif1 = edp->modif2 = '\0';
		edp->state = VT100_EMUL_STATE_CSI;
		break;
	case DCS: /* 8-bit */
		/* XXX cancel current escape sequence */
		edp->nargs = 0;
		memset(edp->args, 0, sizeof (edp->args));
		edp->state = VT100_EMUL_STATE_DCS;
		break;
	case ST: /* string end 8-bit */
		/* XXX only in VT100_EMUL_STATE_STRING */
		wsemul_vt100_handle_dcs(edp);
		return (VT100_EMUL_STATE_NORMAL);
#endif
	case ASCII_LF:
	case ASCII_VT:
	case ASCII_FF:
		wsemul_vt100_nextline(edp);
		break;
	}
}

static u_int
wsemul_vt100_output_esc(struct wsemul_vt100_emuldata *edp, u_char c)
{
	struct vt100base_data *vd = &edp->bd;
	u_int newstate = VT100_EMUL_STATE_NORMAL;
	int i;

	switch (c) {
	case '[': /* CSI */
		vd->nargs = 0;
		memset(vd->args, 0, sizeof (vd->args));
		vd->modif1 = vd->modif2 = '\0';
		newstate = VT100_EMUL_STATE_CSI;
		break;
	case '7': /* DECSC */
		vd->flags |= VTFL_SAVEDCURS;
		edp->savedcursor_row = vd->crow;
		edp->savedcursor_col = vd->ccol;
		edp->savedattr = vd->curattr;
		edp->savedbkgdattr = vd->bkgdattr;
		edp->savedattrflags = vd->attrflags;
		edp->savedfgcol = vd->fgcol;
		edp->savedbgcol = vd->bgcol;
		for (i = 0; i < 4; i++)
			edp->savedchartab_G[i] = edp->chartab_G[i];
		edp->savedchartab0 = edp->chartab0;
		edp->savedchartab1 = edp->chartab1;
		break;
	case '8': /* DECRC */
		if ((vd->flags & VTFL_SAVEDCURS) == 0)
			break;
		vd->crow = edp->savedcursor_row;
		vd->ccol = edp->savedcursor_col;
		vd->curattr = edp->savedattr;
		vd->bkgdattr = edp->savedbkgdattr;
		vd->attrflags = edp->savedattrflags;
		vd->fgcol = edp->savedfgcol;
		vd->bgcol = edp->savedbgcol;
		for (i = 0; i < 4; i++)
			edp->chartab_G[i] = edp->savedchartab_G[i];
		edp->chartab0 = edp->savedchartab0;
		edp->chartab1 = edp->savedchartab1;
		break;
	case '=': /* DECKPAM application mode */
		vd->flags |= VTFL_APPLKEYPAD;
		break;
	case '>': /* DECKPNM numeric mode */
		vd->flags &= ~VTFL_APPLKEYPAD;
		break;
	case 'E': /* NEL */
		vd->ccol = 0;
		/* FALLTHRU */
	case 'D': /* IND */
		wsemul_vt100_nextline(edp);
		break;
	case 'H': /* HTS */
		KASSERT(vd->tabs != 0);
		vd->tabs[vd->ccol] = 1;
		break;
	case '~': /* LS1R */
		edp->chartab1 = 1;
		break;
	case 'n': /* LS2 */
		edp->chartab0 = 2;
		break;
	case '}': /* LS2R */
		edp->chartab1 = 2;
		break;
	case 'o': /* LS3 */
		edp->chartab0 = 3;
		break;
	case '|': /* LS3R */
		edp->chartab1 = 3;
		break;
	case 'N': /* SS2 */
		edp->sschartab = 2;
		break;
	case 'O': /* SS3 */
		edp->sschartab = 3;
		break;
	case 'M': /* RI */
		if (ROWS_ABOVE(vd) > 0) {
			vd->crow--;
			CHECK_DW(vd);
			break;
		}
		wsemul_vt100_scrolldown(vd, 1);
		break;
	case 'P': /* DCS */
		vd->nargs = 0;
		memset(vd->args, 0, sizeof (vd->args));
		newstate = VT100_EMUL_STATE_DCS;
		break;
	case 'c': /* RIS */
		wsemul_vt100_reset(edp);
		wsemul_vt100_ed(vd, 2);
		vd->ccol = vd->crow = 0;
		break;
	case '(': case ')': case '*': case '+': /* SCS */
		edp->designating = c - '(';
		newstate = VT100_EMUL_STATE_SCS94;
		break;
	case '-': case '.': case '/': /* SCS */
		edp->designating = c - '-' + 1;
		newstate = VT100_EMUL_STATE_SCS96;
		break;
	case '#':
		newstate = VT100_EMUL_STATE_ESC_HASH;
		break;
	case ' ': /* 7/8 bit */
		newstate = VT100_EMUL_STATE_ESC_SPC;
		break;
	case ']': /* OSC operating system command */
	case '^': /* PM privacy message */
	case '_': /* APC application program command */
		/* ignored */
		newstate = VT100_EMUL_STATE_STRING;
		break;
	case '<': /* exit VT52 mode - ignored */
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c unknown\n", c);
#endif
		break;
	}

	return (newstate);
}

static u_int
wsemul_vt100_output_scs94(struct wsemul_vt100_emuldata *edp, u_char c)
{
	u_int newstate = VT100_EMUL_STATE_NORMAL;

	switch (c) {
	case '%': /* probably DEC supplemental graphic */
		newstate = VT100_EMUL_STATE_SCS94_PERCENT;
		break;
	case 'A': /* british / national */
		edp->chartab_G[edp->designating] = edp->nrctab;
		break;
	case 'B': /* ASCII */
		edp->chartab_G[edp->designating] = 0;
		break;
	case '<': /* user preferred supplemental */
		/* XXX not really "user" preferred */
		edp->chartab_G[edp->designating] = edp->isolatin1tab;
		break;
	case '0': /* DEC special graphic */
		edp->chartab_G[edp->designating] = edp->decgraphtab;
		break;
	case '>': /* DEC tech */
		edp->chartab_G[edp->designating] = edp->dectechtab;
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%c unknown\n", edp->designating + '(', c);
#endif
		break;
	}
	return (newstate);
}

static u_int
wsemul_vt100_output_scs94_percent(struct wsemul_vt100_emuldata *edp, u_char c)
{
	switch (c) {
	case '5': /* DEC supplemental graphic */
		/* XXX there are differences */
		edp->chartab_G[edp->designating] = edp->isolatin1tab;
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%%%c unknown\n", edp->designating + '(', c);
#endif
		break;
	}
	return (VT100_EMUL_STATE_NORMAL);
}

static u_int
wsemul_vt100_output_scs96(struct wsemul_vt100_emuldata *edp, u_char c)
{
	u_int newstate = VT100_EMUL_STATE_NORMAL;
	int nrc;

	switch (c) {
	case '%': /* probably portuguese */
		newstate = VT100_EMUL_STATE_SCS96_PERCENT;
		break;
	case 'A': /* ISO-latin-1 supplemental */
		edp->chartab_G[edp->designating] = edp->isolatin1tab;
		break;
	case '4': /* dutch */
		nrc = 1;
		goto setnrc;
	case '5': case 'C': /* finnish */
		nrc = 2;
		goto setnrc;
	case 'R': /* french */
		nrc = 3;
		goto setnrc;
	case 'Q': /* french canadian */
		nrc = 4;
		goto setnrc;
	case 'K': /* german */
		nrc = 5;
		goto setnrc;
	case 'Y': /* italian */
		nrc = 6;
		goto setnrc;
	case 'E': case '6': /* norwegian / danish */
		nrc = 7;
		goto setnrc;
	case 'Z': /* spanish */
		nrc = 9;
		goto setnrc;
	case '7': case 'H': /* swedish */
		nrc = 10;
		goto setnrc;
	case '=': /* swiss */
		nrc = 11;
setnrc:
		vt100_setnrc(edp, nrc); /* what table ??? */
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%c unknown\n", edp->designating + '-' - 1, c);
#endif
		break;
	}
	return (newstate);
}

static u_int
wsemul_vt100_output_scs96_percent(struct wsemul_vt100_emuldata *edp, u_char c)
{
	switch (c) {
	case '6': /* portuguese */
		vt100_setnrc(edp, 8);
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%%%c unknown\n", edp->designating + '-', c);
#endif
		break;
	}
	return (VT100_EMUL_STATE_NORMAL);
}

static u_int
wsemul_vt100_output_esc_spc(struct wsemul_vt100_emuldata *edp,
    u_char c)
{
	switch (c) {
	case 'F': /* 7-bit controls */
	case 'G': /* 8-bit controls */
#ifdef VT100_PRINTNOTIMPL
		printf("ESC<SPC>%c ignored\n", c);
#endif
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC<SPC>%c unknown\n", c);
#endif
		break;
	}
	return (VT100_EMUL_STATE_NORMAL);
}

static u_int
wsemul_vt100_output_string(struct wsemul_vt100_emuldata *edp, u_char c)
{
	struct vt100base_data *vd = &edp->bd;

	if (vd->dcstype && vd->dcspos < DCS_MAXLEN)
		vd->dcsarg[vd->dcspos++] = c;
	return (VT100_EMUL_STATE_STRING);
}

static u_int
wsemul_vt100_output_string_esc(struct wsemul_vt100_emuldata *edp, u_char c)
{
	struct vt100base_data *vd = &edp->bd;

	if (c == '\\') { /* ST complete */
		wsemul_vt100_handle_dcs(vd);
		return (VT100_EMUL_STATE_NORMAL);
	} else
		return (VT100_EMUL_STATE_STRING);
}

static u_int
wsemul_vt100_output_dcs(struct wsemul_vt100_emuldata *edp, u_char c)
{
	struct vt100base_data *vd = &edp->bd;
	u_int newstate = VT100_EMUL_STATE_DCS;

	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		/* argument digit */
		if (vd->nargs > VT100_EMUL_NARGS - 1)
			break;
		vd->args[vd->nargs] = (vd->args[vd->nargs] * 10) +
		    (c - '0');
		break;
	case ';': /* argument terminator */
		vd->nargs++;
		break;
	default:
		vd->nargs++;
		if (vd->nargs > VT100_EMUL_NARGS) {
#ifdef VT100_DEBUG
			printf("vt100: too many arguments\n");
#endif
			vd->nargs = VT100_EMUL_NARGS;
		}
		newstate = VT100_EMUL_STATE_STRING;
		switch (c) {
		case '$':
			newstate = VT100_EMUL_STATE_DCS_DOLLAR;
			break;
		case '{': /* DECDLD soft charset */
		case '!': /* DECRQUPSS user preferred supplemental set */
			/* 'u' must follow - need another state */
		case '|': /* DECUDK program F6..F20 */
#ifdef VT100_PRINTNOTIMPL
			printf("DCS%c ignored\n", c);
#endif
			break;
		default:
#ifdef VT100_PRINTUNKNOWN
			printf("DCS%c (%d, %d) unknown\n", c, ARG(vd, 0), ARG(vd, 1));
#endif
			break;
		}
	}

	return (newstate);
}

static u_int
wsemul_vt100_output_dcs_dollar(struct wsemul_vt100_emuldata *edp, u_char c)
{
	struct vt100base_data *vd = &edp->bd;

	switch (c) {
	case 'p': /* DECRSTS terminal state restore */
	case 'q': /* DECRQSS control function request */
#ifdef VT100_PRINTNOTIMPL
		printf("DCS$%c ignored\n", c);
#endif
		break;
	case 't': /* DECRSPS restore presentation state */
		switch (ARG(vd, 0)) {
		case 0: /* error */
			break;
		case 1: /* cursor information restore */
#ifdef VT100_PRINTNOTIMPL
			printf("DCS1$t ignored\n");
#endif
			break;
		case 2: /* tab stop restore */
			vd->dcspos = 0;
			vd->dcstype = DCSTYPE_TABRESTORE;
			break;
		default:
#ifdef VT100_PRINTUNKNOWN
			printf("DCS%d$t unknown\n", ARG(vd, 0));
#endif
			break;
		}
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("DCS$%c (%d, %d) unknown\n", c, ARG(vd, 0), ARG(vd, 1));
#endif
		break;
	}
	return (VT100_EMUL_STATE_STRING);
}

static u_int
wsemul_vt100_output_esc_hash(struct wsemul_vt100_emuldata *edp, u_char c)
{
	struct vt100base_data *vd = &edp->bd;
	int i, j;

	switch (c) {
	case '5': /*  DECSWL single width, single height */
		if (vd->dw) {
			for (i = 0; i < vd->ncols / 2; i++)
				(*vd->emulops->copycols)(vd->emulcookie,
							  vd->crow,
							  2 * i, i, 1);
			(*vd->emulops->erasecols)(vd->emulcookie, vd->crow,
						   i, vd->ncols - i,
						   vd->bkgdattr);
			vd->dblwid[vd->crow] = 0;
			vd->dw = 0;
		}
		break;
	case '6': /*  DECDWL double width, single height */
	case '3': /*  DECDHL double width, double height, top half */
	case '4': /*  DECDHL double width, double height, bottom half */
		if (!vd->dw) {
			for (i = vd->ncols / 2 - 1; i >= 0; i--)
				(*vd->emulops->copycols)(vd->emulcookie,
							  vd->crow,
							  i, 2 * i, 1);
			for (i = 0; i < vd->ncols / 2; i++)
				(*vd->emulops->erasecols)(vd->emulcookie,
							   vd->crow,
							   2 * i + 1, 1,
							   vd->bkgdattr);
			vd->dblwid[vd->crow] = 1;
			vd->dw = 1;
			if (vd->ccol > (vd->ncols >> 1) - 1)
				vd->ccol = (vd->ncols >> 1) - 1;
		}
		break;
	case '8': /* DECALN */
		for (i = 0; i < vd->nrows; i++)
			for (j = 0; j < vd->ncols; j++)
				(*vd->emulops->putchar)(vd->emulcookie, i, j,
							 'E', vd->curattr);
		vd->ccol = 0;
		vd->crow = 0;
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC#%c unknown\n", c);
#endif
		break;
	}
	return (VT100_EMUL_STATE_NORMAL);
}

static u_int
wsemul_vt100_output_csi(struct wsemul_vt100_emuldata *edp, u_char c)
{
	struct vt100base_data *vd = &edp->bd;
	u_int newstate = VT100_EMUL_STATE_CSI;

	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		/* argument digit */
		if (vd->nargs > VT100_EMUL_NARGS - 1)
			break;
		vd->args[vd->nargs] = (vd->args[vd->nargs] * 10) +
		    (c - '0');
		break;
	case ';': /* argument terminator */
		vd->nargs++;
		break;
	case '?': /* DEC specific */
	case '>': /* DA query */
		vd->modif1 = c;
		break;
	case '!':
	case '"':
	case '$':
	case '&':
		vd->modif2 = c;
		break;
	default: /* end of escape sequence */
		vd->nargs++;
		if (vd->nargs > VT100_EMUL_NARGS) {
#ifdef VT100_DEBUG
			printf("vt100: too many arguments\n");
#endif
			vd->nargs = VT100_EMUL_NARGS;
		}
		wsemul_vt100_handle_csi(vd, c);
		newstate = VT100_EMUL_STATE_NORMAL;
		break;
	}
	return (newstate);
}

void
wsemul_vt100_output(void *cookie, const u_char *data, u_int count, int kernel)
{
	struct wsemul_vt100_emuldata *edp = cookie;
	struct vt100base_data *vd = &edp->bd;

#ifdef DIAGNOSTIC
	if (kernel && !edp->console)
		panic("wsemul_vt100_output: kernel output, not console");
#endif

	if (vd->flags & VTFL_CURSORON)
		(*vd->emulops->cursor)(vd->emulcookie, 0,
					vd->crow, vd->ccol << vd->dw);
	for (; count > 0; data++, count--) {
		if ((*data & 0x7f) < 0x20) {
			wsemul_vt100_output_c0c1(edp, *data, kernel);
			continue;
		}
		if (edp->state == VT100_EMUL_STATE_NORMAL || kernel) {
			wsemul_vt100_output_normal(edp, *data, kernel);
			continue;
		}
#ifdef DIAGNOSTIC
		if (edp->state > sizeof(vt100_output) / sizeof(vt100_output[0]))
			panic("wsemul_vt100: invalid state %d", edp->state);
#endif
		edp->state = vt100_output[edp->state - 1](edp, *data);
	}
	if (vd->flags & VTFL_CURSORON)
		(*vd->emulops->cursor)(vd->emulcookie, 1,
					vd->crow, vd->ccol << vd->dw);
}

#ifdef WSDISPLAY_CUSTOM_OUTPUT
static void
wsemul_vt100_getmsgattrs(void *cookie, struct wsdisplay_msgattrs *ma)
{
	struct wsemul_vt100_emuldata *edp = cookie;
	struct vt100base_data *vd = &edp->bd;

	*ma = vd->msgattrs;
}

static void
wsemul_vt100_setmsgattrs(void *cookie, const struct wsscreen_descr *type,
                         const struct wsdisplay_msgattrs *ma)
{
	int error;
	long tmp;
	struct wsemul_vt100_emuldata *edp = cookie;
	struct vt100base_data *vd = &edp->bd;

	vd->msgattrs = *ma;
	if (type->capabilities & WSSCREEN_WSCOLORS) {
		vd->msgattrs.default_attrs |= WSATTR_WSCOLORS;
		vd->msgattrs.kernel_attrs |= WSATTR_WSCOLORS;
	} else {
		vd->msgattrs.default_bg = vd->msgattrs.kernel_bg = 0;
		vd->msgattrs.default_fg = vd->msgattrs.kernel_fg = 0;
	}

	error = (*vd->emulops->allocattr)(vd->emulcookie,
	                                   vd->msgattrs.default_fg,
					   vd->msgattrs.default_bg,
	                                   vd->msgattrs.default_attrs,
	                                   &tmp);
#ifndef VT100_DEBUG
	__USE(error);
#else
	if (error)
		printf("vt100: failed to allocate attribute for default "
		       "messages\n");
	else
#endif
	{
		if (vd->curattr == vd->defattr) {
			vd->bkgdattr = vd->curattr = tmp;
			vd->attrflags = vd->msgattrs.default_attrs;
			vd->bgcol = vd->msgattrs.default_bg;
			vd->fgcol = vd->msgattrs.default_fg;
		} else {
			edp->savedbkgdattr = edp->savedattr = tmp;
			edp->savedattrflags = vd->msgattrs.default_attrs;
			edp->savedbgcol = vd->msgattrs.default_bg;
			edp->savedfgcol = vd->msgattrs.default_fg;
		}
		if (vd->emulops->replaceattr != NULL)
			(*vd->emulops->replaceattr)(vd->emulcookie,
			                             vd->defattr, tmp);
		vd->defattr = tmp;
	}

	error = (*vd->emulops->allocattr)(vd->emulcookie,
	                                   vd->msgattrs.kernel_fg,
					   vd->msgattrs.kernel_bg,
	                                   vd->msgattrs.kernel_attrs,
	                                   &tmp);
#ifdef VT100_DEBUG
	if (error)
		printf("vt100: failed to allocate attribute for kernel "
		       "messages\n");
	else
#endif
	{
		if (vd->emulops->replaceattr != NULL)
			(*vd->emulops->replaceattr)(vd->emulcookie,
			                             edp->kernattr, tmp);
		edp->kernattr = tmp;
	}
}
#endif /* WSDISPLAY_CUSTOM_OUTPUT */
