/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@netbsd.org)
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
#ifndef NETPGPDEFS_H_
#define NETPGPDEFS_H_	1

#define PRItime		"ll"

#ifdef WIN32
#define PRIsize		"I"
#else
#define PRIsize		"z"
#endif

/* for silencing unused parameter warnings */
#define __PGP_USED(x)	/*LINTED*/(void)&(x)

#ifndef __UNCONST
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))
#endif

/* number of elements in an array */
#define PGP_ARRAY_SIZE(a)       (sizeof(a)/sizeof(*(a)))

void            hexdump(FILE *, const char *, const uint8_t *, size_t);

const char     *pgp_str_from_map(int, pgp_map_t *);

int             pgp_set_debug_level(const char *);
int             pgp_get_debug_level(const char *);

void		*pgp_new(size_t);

#define NETPGP_BUFSIZ	8192

#define CALLBACK(t, cbinfo, pkt)	do {				\
	(pkt)->tag = (t);						\
	if (pgp_callback(pkt, cbinfo) == PGP_RELEASE_MEMORY) {	\
		pgp_parser_content_free(pkt);				\
	}								\
} while(/* CONSTCOND */0)

#endif /* !NETPGPDEFS_H_ */
