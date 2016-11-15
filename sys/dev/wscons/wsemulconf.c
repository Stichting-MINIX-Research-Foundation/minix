/* $NetBSD: wsemulconf.c,v 1.8 2010/02/02 16:18:29 drochner Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: wsemulconf.c,v 1.8 2010/02/02 16:18:29 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/wscons/wsemulvar.h>		/* pulls in opt_wsemul.h */
#include <dev/wscons/wscons_callbacks.h>

struct wsemulentry {
	const struct wsemul_ops *ops;
	int usecnt;
	LIST_ENTRY(wsemulentry) next;
};
static LIST_HEAD(, wsemulentry) wsemuls = LIST_HEAD_INITIALIZER(&wsemuls);

static const struct wsemul_ops *wsemul_conf[] = {
#ifdef WSEMUL_SUN
	&wsemul_sun_ops,
#endif
#ifdef WSEMUL_VT100
	&wsemul_vt100_ops,
#endif
#ifndef WSEMUL_NO_DUMB
	&wsemul_dumb_ops,
#endif
	NULL
};

const struct wsemul_ops *
wsemul_pick(const char *name)
{
	const struct wsemul_ops **ops;
	struct wsemulentry *wep;

	if (name == NULL) {
		/* default */
#ifdef WSEMUL_DEFAULT
		name = WSEMUL_DEFAULT;
#else
		return (wsemul_conf[0]);
#endif
	}

	LIST_FOREACH(wep, &wsemuls, next)
		if (!strcmp(name, wep->ops->name)) {
			wep->usecnt++;
			return wep->ops;
		}

	for (ops = &wsemul_conf[0]; *ops != NULL; ops++)
		if (strcmp(name, (*ops)->name) == 0)
			break;

	return (*ops);
}

void
wsemul_drop(const struct wsemul_ops *ops)
{
	struct wsemulentry *wep;

	LIST_FOREACH(wep, &wsemuls, next)
		if (ops == wep->ops) {
			wep->usecnt--;
			return;
		}
}

int
wsemul_add(const struct wsemul_ops *ops)
{
	struct wsemulentry *wep;

	wep = malloc(sizeof (struct wsemulentry), M_DEVBUF, M_WAITOK);
	wep->ops = ops;
	wep->usecnt = 0;
	LIST_INSERT_HEAD(&wsemuls, wep, next);
	return 0;
}

int
wsemul_remove(const struct wsemul_ops *ops)
{
	struct wsemulentry *wep;

	LIST_FOREACH(wep, &wsemuls, next) {
		if (ops == wep->ops) {
			if (wep->usecnt)
				return EBUSY;
			LIST_REMOVE(wep, next);
			return 0;
		}
	}
	return ENOENT;
}
