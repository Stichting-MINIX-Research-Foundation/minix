/*	$NetBSD: in_print.c,v 1.1 2014/12/02 19:35:27 christos Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
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

#include <sys/types.h>
#ifdef _KERNEL
__KERNEL_RCSID(0, "$NetBSD: in_print.c,v 1.1 2014/12/02 19:35:27 christos Exp $");
#include <sys/systm.h>
#else
__RCSID("$NetBSD: in_print.c,v 1.1 2014/12/02 19:35:27 christos Exp $");
#include <stdio.h>
#endif
#include <netinet/in.h>

int
in_print(char *buf, size_t len, const struct in_addr *ia)
{
	const in_addr_t a = ntohl(ia->s_addr);
	return snprintf(buf, len, "%d.%d.%d.%d", 
	    (a >> 24) & 0xff, (a >> 16) & 0xff,
	    (a >>  8) & 0xff, (a >>  0) & 0xff);
}

int
sin_print(char *buf, size_t len, const void *v)
{
	const struct sockaddr_in *sin = v;
	const struct in_addr *ia = &sin->sin_addr;
	char abuf[INET_ADDRSTRLEN];

	if (!sin->sin_port)
		return in_print(buf, len, ia);

	in_print(abuf, sizeof(abuf), ia);
	return snprintf(buf, len, "%s:%hu", abuf, ntohs(sin->sin_port));
}
