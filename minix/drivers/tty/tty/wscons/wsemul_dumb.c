/* $NetBSD: wsemul_dumb.c,v 1.18 2018/09/03 16:29:34 riastradh Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wsemul_dumb.c,v 1.18 2018/09/03 16:29:34 riastradh Exp $");

#include <sys/param.h>
//#include <sys/systm.h>
#include <sys/time.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <minix/sysutil.h>

#include "wsconsio.h"
#include "wsdisplayvar.h"
#include "wsemulvar.h"
#include "wsemul_vt100var.h"
#include "ascii.h"

void	*wsemul_dumb_cnattach(const struct wsscreen_descr *, void *,
				   int, int, long);
void	*wsemul_dumb_attach(int console, const struct wsscreen_descr *,
				 void *, int, int, void *, long);
void	wsemul_dumb_output(void *cookie, const u_char *data, u_int count, int);
//int	wsemul_dumb_translate(void *cookie, keysym_t, const char **);
void	wsemul_dumb_detach(void *cookie, u_int *crowp, u_int *ccolp);
void	wsemul_dumb_resetop(void *, enum wsemul_resetops);

const struct wsemul_ops wsemul_dumb_ops = {
	.name = "dumb",
	.cnattach = wsemul_dumb_cnattach,
	.attach = wsemul_dumb_attach,
	.output = wsemul_dumb_output,
//	.translate = wsemul_dumb_translate,
	.detach = wsemul_dumb_detach,
	.reset = wsemul_dumb_resetop,
//	.getmsgattrs = NULL,
//	.setmsgattrs = NULL,
	.resize = NULL,
};

struct wsemul_dumb_emuldata {
	const struct wsdisplay_emulops *emulops;
	void *emulcookie;
	void *cbcookie;
	u_int nrows, ncols, crow, ccol;
	long defattr;
};

struct wsemul_dumb_emuldata wsemul_dumb_console_emuldata;

void *
wsemul_dumb_cnattach(const struct wsscreen_descr *type, void *cookie,
	int ccol, int crow, long defattr)
{
	struct wsemul_dumb_emuldata *edp;

	edp = &wsemul_dumb_console_emuldata;

	edp->emulops = type->textops;
	edp->emulcookie = cookie;
	edp->nrows = type->nrows;
	edp->ncols = type->ncols;
	edp->crow = crow;
	edp->ccol = ccol;
	edp->defattr = defattr;
	edp->cbcookie = NULL;

	return (edp);
}

void *
wsemul_dumb_attach(int console, const struct wsscreen_descr *type,
	void *cookie, int ccol, int crow, void *cbcookie, long defattr)
{
	struct wsemul_dumb_emuldata *edp;

	if (console)
		edp = &wsemul_dumb_console_emuldata;
	else {
		edp = malloc(sizeof *edp);

		edp->emulops = type->textops;
		edp->emulcookie = cookie;
		edp->nrows = type->nrows;
		edp->ncols = type->ncols;
		edp->crow = crow;
		edp->ccol = ccol;
		edp->defattr = defattr;
	}

	edp->cbcookie = cbcookie;

	return (edp);
}

void wsemul_dumb_output(void *cookie, const u_char *data, u_int count,
    int kernel)
{
	struct wsemul_dumb_emuldata *edp = cookie;
	u_char c;
	int n;

	ser_puts("wsemul_dumb_output:enter\n");
	/* XXX */
	(*edp->emulops->cursor)(edp->emulcookie, 0, edp->crow, edp->ccol);
	while (count-- > 0) {
		c = *data++;
		switch (c) {
		case ASCII_BEL:
			wsdisplay_emulbell(edp->cbcookie);
			break;

		case ASCII_BS:
			if (edp->ccol > 0)
				edp->ccol--;
			break;

		case ASCII_CR:
			edp->ccol = 0;
			break;

		case ASCII_HT:
			n = uimin(8 - (edp->ccol & 7),
			    edp->ncols - edp->ccol - 1);
			(*edp->emulops->erasecols)(edp->emulcookie,
			    edp->crow, edp->ccol, n, edp->defattr);
			edp->ccol += n;
			break;

		case ASCII_FF:
			(*edp->emulops->eraserows)(edp->emulcookie, 0,
			    edp->nrows, edp->defattr);
			edp->ccol = 0;
			edp->crow = 0;
			break;

		case ASCII_VT:
			if (edp->crow > 0)
				edp->crow--;
			break;

		default:
			(*edp->emulops->putchar)(edp->emulcookie, edp->crow,
			    edp->ccol, c, edp->defattr);
			edp->ccol++;

			/* if cur col is still on cur line, done. */
			if (edp->ccol < edp->ncols)
				break;

			/* wrap the column around. */
			edp->ccol = 0;

                	/* FALLTHRU */

		case ASCII_LF:
	                /* if the cur line isn't the last, incr and leave. */
			if (edp->crow < edp->nrows - 1) {
				edp->crow++;
				break;
			}
			n = 1;		/* number of lines to scroll */
			(*edp->emulops->copyrows)(edp->emulcookie, n, 0,
			    edp->nrows - n);
			(*edp->emulops->eraserows)(edp->emulcookie,
			    edp->nrows - n, n, edp->defattr);
			edp->crow -= n - 1;
			break;
		}
	}
	/* XXX */
	(*edp->emulops->cursor)(edp->emulcookie, 1, edp->crow, edp->ccol);
}

void wsemul_dumb_detach(void *cookie, u_int *crowp, u_int *ccolp)
{
	struct wsemul_dumb_emuldata *edp = cookie;

	*crowp = edp->crow;
	*ccolp = edp->ccol;
	if (edp != &wsemul_dumb_console_emuldata)
		free(edp);
}

void wsemul_dumb_resetop(void *cookie, enum wsemul_resetops op)
{
	struct wsemul_dumb_emuldata *edp = cookie;

	switch (op) {
	case WSEMUL_CLEARSCREEN:
		(*edp->emulops->eraserows)(edp->emulcookie, 0, edp->nrows,
					   edp->defattr);
		edp->ccol = edp->crow = 0;
		(*edp->emulops->cursor)(edp->emulcookie, 1, 0, 0);
		break;
	default:
		break;
	}
}
