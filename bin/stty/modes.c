/* $NetBSD: modes.c,v 1.18 2015/05/01 17:01:08 christos Exp $ */

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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)modes.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: modes.c,v 1.18 2015/05/01 17:01:08 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "stty.h"
#include "extern.h"

struct modes {
	const char *name;
	tcflag_t flag;
};

struct specialmodes {
	const char *name;
	tcflag_t set;
	tcflag_t unset;
};

/*
 * The code in optlist() depends on minus options following regular
 * options, i.e. "foo" must immediately precede "-foo".
 */
const struct modes cmodes[] = {
	{ "cstopb",	CSTOPB },
	{ "cread",	CREAD },
	{ "parenb",	PARENB },
	{ "parodd",	PARODD },
	{ "hupcl",	HUPCL },
	{ "hup",	HUPCL },
	{ "clocal",	CLOCAL },
	{ "crtscts",	CRTSCTS },
	{ "mdmbuf",	MDMBUF },
	{ "cdtrcts",	CDTRCTS },
	{ .name = NULL },
};

const struct specialmodes cspecialmodes[] = {
	{ "cs5",	CS5, CSIZE },
	{ "cs6",	CS6, CSIZE },
	{ "cs7",	CS7, CSIZE },
	{ "cs8",	CS8, CSIZE },
	{ "parity",	PARENB | CS7, PARODD | CSIZE },
	{ "-parity",	CS8, PARODD | PARENB | CSIZE },
	{ "evenp",	PARENB | CS7, PARODD | CSIZE },
	{ "-evenp",	CS8, PARODD | PARENB | CSIZE },
	{ "oddp",	PARENB | CS7 | PARODD, CSIZE },
	{ "-oddp",	CS8, PARODD | PARENB | CSIZE },
	{ "pass8",	CS8, PARODD | PARENB | CSIZE },
	{ "-pass8",	PARENB | CS7, PARODD | CSIZE },
	{ .name = NULL },
};

const struct modes imodes[] = {
	{ "ignbrk",	IGNBRK },
	{ "brkint",	BRKINT },
	{ "ignpar",	IGNPAR },
	{ "parmrk",	PARMRK },
	{ "inpck",	INPCK },
	{ "istrip",	ISTRIP },
	{ "inlcr",	INLCR },
	{ "igncr",	IGNCR },
	{ "icrnl",	ICRNL },
	{ "ixon",	IXON },
	{ "flow",	IXON },
	{ "ixoff",	IXOFF },
	{ "tandem",	IXOFF },
	{ "ixany",	IXANY },
	{ "imaxbel",	IMAXBEL },
	{ .name = NULL },
};

const struct specialmodes ispecialmodes[] = {
	{ "decctlq",	0, IXANY },
	{ "-decctlq",	IXANY, 0 },
	{ .name = NULL },
};

const struct modes lmodes[] = {
	{ "echo",	ECHO },
	{ "echoe",	ECHOE },
	{ "crterase",	ECHOE },
	{ "crtbs",	ECHOE },	/* crtbs not supported, close enough */
	{ "echok",	ECHOK },
	{ "echoke",	ECHOKE },
	{ "crtkill",	ECHOKE },
	{ "altwerase",	ALTWERASE },
	{ "iexten",	IEXTEN },
	{ "echonl",	ECHONL },
	{ "echoctl",	ECHOCTL },
	{ "ctlecho",	ECHOCTL },
	{ "echoprt",	ECHOPRT },
	{ "prterase",	ECHOPRT },
	{ "isig",	ISIG },
	{ "icanon",	ICANON },
	{ "noflsh",	NOFLSH },
	{ "tostop",	TOSTOP },
	{ "flusho",	FLUSHO },
	{ "pendin",	PENDIN },
	{ "nokerninfo",	NOKERNINFO },
	{ .name = NULL },
};

const struct specialmodes lspecialmodes[] = {
	{ "crt",	ECHOE|ECHOKE|ECHOCTL, ECHOK|ECHOPRT },
	{ "-crt",	ECHOK, ECHOE|ECHOKE|ECHOCTL },
	{ "newcrt",	ECHOE|ECHOKE|ECHOCTL, ECHOK|ECHOPRT },
	{ "-newcrt",	ECHOK, ECHOE|ECHOKE|ECHOCTL },
	{ "kerninfo",	0, NOKERNINFO },
	{ "-kerninfo",	NOKERNINFO, 0 },
	{ .name = NULL },
};

const struct modes omodes[] = {
	{ "opost",	OPOST },
	{ "onlcr",	ONLCR },
	{ "ocrnl",	OCRNL },
	{ "oxtabs",	OXTABS },
	{ "onocr",	ONOCR },
	{ "onlret",	ONLRET },
	{ .name = NULL },
};

const struct specialmodes ospecialmodes[] = {
	{ "litout",	0, OPOST },
	{ "-litout",	OPOST, 0 },
	{ "tabs",	0, OXTABS },		/* "preserve" tabs */
	{ "-tabs",	OXTABS, 0 },
	{ .name = NULL },
};

#define	CHK(s)	(!strcmp(name, s))

static int
modeset(const char *name, const struct modes *mp,
    const struct specialmodes *smp, tcflag_t *f)
{
	bool neg;

	for (; smp->name; ++smp)
		if (CHK(smp->name)) {
			*f &= ~smp->unset;
			*f |= smp->set;
			return 1;
		}

	if ((neg = (*name == '-')))
		name++;

	for (; mp->name; ++mp)
		if (CHK(mp->name)) {
			if (neg)
				*f &= ~mp->flag;
			else
				*f |= mp->flag;
			return 1;
		}

	return 0;
}

int
msearch(char ***argvp, struct info *ip)
{
	const char *name = **argvp;

	if (modeset(name, cmodes, cspecialmodes, &ip->t.c_cflag))
		goto out;
		
	if (modeset(name, imodes, ispecialmodes, &ip->t.c_iflag))
		goto out;

	if (modeset(name, lmodes, lspecialmodes, &ip->t.c_lflag))
		goto out;

	if (modeset(name, omodes, ospecialmodes, &ip->t.c_oflag))
		goto out;

	return 0;
out:
	ip->set = 1;
	return 1;
}
