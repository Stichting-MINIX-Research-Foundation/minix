/*	$NetBSD: __cmsg_alignbytes.c,v 1.3 2009/03/16 05:59:21 cegger Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jun-ichiro Hagino.
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

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: __cmsg_alignbytes.c,v 1.3 2009/03/16 05:59:21 cegger Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#else
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#endif

int
__cmsg_alignbytes(void)
{
	static int alignbytes = -1;
#ifdef HW_ALIGNBYTES
	int mib[2];
	size_t len;
	int ret;
#endif

	if (alignbytes > 0)
		return alignbytes;

#ifdef HW_ALIGNBYTES
	mib[0] = CTL_HW;
	mib[1] = HW_ALIGNBYTES;
	len = sizeof(alignbytes);
	ret = sysctl(mib, (u_int) (sizeof(mib) / sizeof(mib[0])),
	    (void *)&alignbytes, &len, NULL, (size_t)0);
	if (ret >= 0 && alignbytes >= 0)
		return alignbytes;
#endif
	/* last resort */
	alignbytes = ALIGNBYTES;
	return alignbytes;
}
