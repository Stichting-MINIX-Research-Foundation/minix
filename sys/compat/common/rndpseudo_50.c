/*	$NetBSD: rndpseudo_50.c,v 1.2 2012/08/03 07:51:21 matt Exp $	*/

/*-
 * Copyright (c) 1997-2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Graff <explorer@flame.org> and Thor Lancelot Simon.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rndpseudo_50.c,v 1.2 2012/08/03 07:51:21 matt Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#include "opt_compat_netbsd32.h"
#endif

#include <sys/param.h>
#include <sys/file.h>

#include <sys/rnd.h>
#include <compat/sys/rnd.h>

/*
 * Convert from rndsource_t to rndsource50_t, for the results from
 * RNDGETNUM50 and RNDGETNAME50.
 */
static void
rndsource_to_rndsource50(rndsource_t *r, rndsource50_t *r50)
{
	memset(r50, 0, sizeof(*r50));
	strlcpy(r50->name, r->name, sizeof(r50->name));
	r50->total = r->total;
	r50->type = r->type;
	r50->flags = r->flags;
}

#if defined(COMPAT_NETBSD32) && defined(_LP64)
/*
 * Convert from rndsource_t to rndsource50_32_t, for the results from
 * RNDGETNUM50_32 and RNDGETNAME50_32.
 */
static void
rndsource_to_rndsource50_32(rndsource_t *r, rndsource50_32_t *r50_32)
{
	memset(r50_32, 0, sizeof(*r50_32));
	strlcpy(r50_32->name, r->name, sizeof(r50_32->name));
	r50_32->total = r->total;
	r50_32->type = r->type;
	r50_32->flags = r->flags;
}
#endif /* COMPAT_NETBSD32 */

/*
 * COMPAT_50 handling for rnd_ioctl.  This is called from rnd_ioctl.
 *
 * It also handles the case of (COMPAT_50 && COMPAT_NETBSD32).
 */
int
compat_50_rnd_ioctl(struct file *fp, u_long cmd, void *addr)
{
	int ret = 0;

	switch (cmd) {

	case RNDGETSRCNUM50:
	{
		rndstat_t rstbuf = {.start = 0};
		rndstat50_t *rst50 = (rndstat50_t *)addr;
		int count;

		if (rst50->count > RND_MAXSTATCOUNT50)
			return EINVAL;

		rstbuf.start = rst50->start;
		rstbuf.count = rst50->count;

		ret = (fp->f_ops->fo_ioctl)(fp, RNDGETSRCNUM, &rstbuf);
		if (ret != 0)
			return ret;

		for (count = 0; count < rst50->count; count++) {
			rndsource_to_rndsource50(&rstbuf.source[count],
			    &rst50->source[count]);
		}
		rst50->count = rstbuf.count;

		break;
	}

#if defined(COMPAT_NETBSD32) && defined(_LP64)
	case RNDGETSRCNUM50_32:
	{
		rndstat_t rstbuf = {.start = 0};
		rndstat50_32_t *rst50_32 = (rndstat50_32_t *)addr;
		int count;

		if (rst50_32->count > RND_MAXSTATCOUNT50)
			return (EINVAL);

		rstbuf.start = rst50_32->start;
		rstbuf.count = rst50_32->count;

		ret = (fp->f_ops->fo_ioctl)(fp, RNDGETSRCNUM, &rstbuf);
		if (ret != 0)
			return ret;

		for (count = 0; count < rst50_32->count; count++) {
			rndsource_to_rndsource50_32(&rstbuf.source[count],
			    &rst50_32->source[count]);
		}
		rst50_32->count = rstbuf.count;

		break;
	}
#endif /* COMPAT_NETBSD32 */

	case RNDGETSRCNAME50:
	{
		rndstat_name_t rstnmbuf = {.name[0] = 0};
		rndstat_name50_t *rstnm50;
		rstnm50 = (rndstat_name50_t *)addr;

		strlcpy(rstnmbuf.name, rstnm50->name, sizeof(rstnmbuf.name));

		ret = (fp->f_ops->fo_ioctl)(fp, RNDGETSRCNAME, &rstnmbuf);
		if (ret != 0)
			return ret;

		rndsource_to_rndsource50(&rstnmbuf.source, &rstnm50->source);

		break;
	}

#if defined(COMPAT_NETBSD32) && defined(_LP64)
	case RNDGETSRCNAME50_32:
	{
		rndstat_name_t rstnmbuf = {.name[0] = 0};
		rndstat_name50_32_t *rstnm50_32;
		rstnm50_32 = (rndstat_name50_32_t *)addr;

		strlcpy(rstnmbuf.name, rstnm50_32->name, sizeof(rstnmbuf.name));

		ret = (fp->f_ops->fo_ioctl)(fp, RNDGETSRCNAME, &rstnmbuf);
		if (ret != 0)
			return ret;

		rndsource_to_rndsource50_32(&rstnmbuf.source,
		    &rstnm50_32->source);

		break;
	}
#endif

	default:
		return ENOTTY;
	}

	return ret;
}
