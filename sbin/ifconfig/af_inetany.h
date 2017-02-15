/*	$NetBSD: af_inetany.h,v 1.4 2008/07/02 07:44:14 dyoung Exp $	*/

/*-
 * Copyright (c) 2008 David Young.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _IFCONFIG_AF_INETANY_H
#define _IFCONFIG_AF_INETANY_H

#include <sys/types.h>
#include <prop/proplib.h>

#define	IFADDR_PARAM(__arg)	{.cmd = (__arg), .desc = #__arg}
#define	BUFPARAM(__arg) 	{.buf = &(__arg), .buflen = sizeof(__arg)}

struct apbuf {
	void *buf;
	size_t buflen;
};

struct afparam {
	struct {
		char *buf;
		size_t buflen;
	} name[2];
	struct apbuf dgaddr, addr, brd, dst, mask, req, dgreq, defmask,
	    pre_aifaddr_arg;
	struct {
		unsigned long cmd;
		const char *desc;
	} aifaddr, difaddr, gifaddr;
	int (*pre_aifaddr)(prop_dictionary_t, const struct afparam *);
};

void	commit_address(prop_dictionary_t, prop_dictionary_t,
    const struct afparam *);

#endif /* _IFCONFIG_AF_INETANY_H */
