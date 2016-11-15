/*	$NetBSD: at_print.c,v 1.1 2014/12/02 19:33:44 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: at_print.c,v 1.1 2014/12/02 19:33:44 christos Exp $");
#include <sys/systm.h>
#else
__RCSID("$NetBSD: at_print.c,v 1.1 2014/12/02 19:33:44 christos Exp $");
#include <stdio.h>
#endif
#include <netatalk/at.h>

int
at_print(char *buf, size_t len, const struct at_addr *aa)
{
	return snprintf(buf, len, "%hu.%hhu", ntohs(aa->s_net), aa->s_node);
}

// XXX: netrange
int
sat_print(char *buf, size_t len, const void *v)
{
	const struct sockaddr_at *sat = v;
	const struct at_addr *aa = &sat->sat_addr;
	char abuf[ATALK_ADDRSTRLEN];

	if (!sat->sat_port)
		return at_print(buf, len, aa);

	at_print(abuf, sizeof(abuf), aa);
	return snprintf(buf, len, "%s:%hhu", abuf, sat->sat_port);
}
