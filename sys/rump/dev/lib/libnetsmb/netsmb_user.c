/*	$NetBSD: netsmb_user.c,v 1.3 2014/11/16 15:31:12 nakayama Exp $	*/

/*
 * Copyright (c) 2014 Takeshi Nakayama.
 * All rights reserved.
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
 */
#ifndef _KERNEL
#include <stddef.h>
#include <errno.h>
#ifdef __NetBSD__
#include <iconv.h>
#endif

#include <rump/rumpuser_component.h>

#include "netsmb_user.h"

int
rumpcomp_netsmb_iconv_open(const char *to, const char *from, void **handle)
{
#ifdef __NetBSD__
	iconv_t cd;
	int rv;

	cd = iconv_open(to, from);
	if (cd == (iconv_t)-1)
		rv = errno;
	else {
		if (handle != NULL)
			*handle = (void *)cd;
		rv = 0;
	}

	return rumpuser_component_errtrans(rv);
#else
	/* fallback to use dumb copy function */
	return 0;
#endif
}

int
rumpcomp_netsmb_iconv_close(void *handle)
{
#ifdef __NetBSD__
	int rv;

	if (iconv_close((iconv_t)handle) == -1)
		rv = errno;
	else
		rv = 0;

	return rumpuser_component_errtrans(rv);
#else
	/* do nothing */
	return 0;
#endif
}

int
rumpcomp_netsmb_iconv_conv(void *handle, const char **inbuf,
    size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
#ifdef __NetBSD__
	int rv;

	if (iconv((iconv_t)handle, inbuf, inbytesleft, outbuf, outbytesleft)
	    == (size_t)-1)
		rv = errno;
	else
		rv = 0;

	return rumpuser_component_errtrans(rv);
#else
	/* do nothing */
	return 0;
#endif
}
#endif
