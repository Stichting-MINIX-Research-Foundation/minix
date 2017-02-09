/* $NetBSD: wsemul_vt100var.h,v 1.14 2010/02/10 19:39:39 drochner Exp $ */

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

#include <dev/wscons/vt100_base.h>

struct wsemul_vt100_emuldata {
	struct vt100base_data bd;

	long kernattr;			/* attribute for kernel output */
	int console;			/* used for DIAGNOSTIC */

	u_int state;			/* processing state */

	int chartab0, chartab1;
	u_int *chartab_G[4];
	u_int *isolatin1tab, *decgraphtab, *dectechtab;
	u_int *nrctab;
	int sschartab; /* single shift */

	int designating;	/* substate in VT100_EMUL_STATE_SCS* */

	u_int savedcursor_row, savedcursor_col;
	long savedattr, savedbkgdattr;
	int savedattrflags, savedfgcol, savedbgcol;
	int savedchartab0, savedchartab1;
	u_int *savedchartab_G[4];
};

void wsemul_vt100_reset(struct wsemul_vt100_emuldata *);

int wsemul_vt100_translate(void *, keysym_t, const char **);

void vt100_initchartables(struct wsemul_vt100_emuldata *);
void vt100_setnrc(struct wsemul_vt100_emuldata *, int);
