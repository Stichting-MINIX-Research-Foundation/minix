/* $NetBSD: test_hash.c,v 1.2 2011/02/16 02:14:23 christos Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <saslc.h>

__RCSID("$NetBSD: test_hash.c,v 1.2 2011/02/16 02:14:23 christos Exp $");

#define MAX_HASH_SIZE	256

/*
 * List of all property keys.
 * NB: I did not list those used for debugging as collisions in that
 * case don't really hurt anything.
 */
static const char *keys[] = {
	SASLC_PROP_AUTHCID,
	SASLC_PROP_AUTHZID,
	SASLC_PROP_BASE64IO,
	SASLC_PROP_CIPHERMASK,
	SASLC_PROP_DEBUG,
	SASLC_PROP_HOSTNAME,
	SASLC_PROP_MAXBUF,
	SASLC_PROP_PASSWD,
	SASLC_PROP_QOPMASK,
	SASLC_PROP_REALM,
	SASLC_PROP_SECURITY,
	SASLC_PROP_SERVICE,
	SASLC_PROP_SERVNAME
};

/*
 * This must match the dict.c::saslc__dict_hashval() routine.
 */
static size_t
hash(const char *cp, size_t hsize, size_t hinit, size_t shift)
{
	size_t hval;

	hval = hinit;
	for (/*EMPTY*/; *cp != '\0'; cp++) {
		hval <<= shift;
		hval += (size_t)*cp;
	}
	return hval % hsize;
}

static int
no_collision(size_t hsize, size_t hinit, size_t shift)
{
	const char **used;
	size_t hval, i;

	used = calloc(sizeof(*used), hsize);
	if (used == NULL)
		err(EXIT_FAILURE, "calloc");
	for (i = 0; i < __arraycount(keys); i++) {
		hval = hash(keys[i], hsize, hinit, shift);
		if (used[hval] != 0) {
			free(used);
			return 0;
		}
		used[hval] = keys[i];
	}
#if 0
	for (i = 0; i < hsize; i++) {
		if (used[i] != NULL)
			printf("%02zd: %s\n", i, used[i]);
	}
#endif
	free(used);
	return 1;
}

static int __dead
usage(int rval)
{

	printf("%s [<min hash size> [<max hash size>]]\n", getprogname());
	printf("min and max hash size must be >= %zu\n", __arraycount(keys));
	exit(rval);
}

static void
show_result(int brief, size_t h, size_t i, size_t s)
{
	if (brief)
	    printf("no collisions: hsize=%zu  hinit=%zu  shift=%zu\n", h, i, s);
	else {
		printf("#define HASH_SIZE\t%zu\n", h);
		printf("#define HASH_INIT\t%zu\n", i);
		printf("#define HASH_SHIFT\t%zu\n", s);
	}
}

int
main(int argc, char **argv)
{
	char *p;
	size_t i, h, s;
	size_t h_min, h_max;
	int brief;

	brief = argc != 1;
	h_min = 0;
	h_max = 0;
	switch (argc) {
	case 1:
		h_min = __arraycount(keys);
		h_max = MAX_HASH_SIZE;
		break;
	case 3:
		h = strtol(argv[2], &p, 0);
		if (*p != '\0' || h == 0)
			usage(-1);
		h_max = h;
		/*FALLTHROUGH*/
	case 2:
		h = strtol(argv[1], &p, 0);
		if (*p != '\0' || h == 0)
			usage(-1);
		h_min = h;
		if (argc == 2)
			h_max = h;
		break;
	default:
		usage(0);
	}
	if (h_max < __arraycount(keys) ||
	    h_min < __arraycount(keys) ||
	    h_max < h_min)
		usage(-1);

	for (h = h_min; h <= h_max; h++) {
		for (s = 0; s < 32; s++) {
			for (i = 0; i < h; i++) {
				if (no_collision(h, i, s)) {
					show_result(brief, h, i, s);
					if (argc == 1)
						return 0;
				}
			}
		}
	}
	return 0;
}
