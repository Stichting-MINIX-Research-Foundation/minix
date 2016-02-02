/*-
 * Copyright (c) 2012 Alistair Crooks <agc@NetBSD.org>
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
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syslog.h>

#ifdef _KERNEL
# include <sys/kmem.h>
#else
# include <ctype.h>
# include <inttypes.h>
# include <stdarg.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <unistd.h>
#endif

#include "misc.h"

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

void *
netpgp_allocate(size_t n, size_t nels)
{
#ifdef _KERNEL
	return kmem_zalloc(n * nels, KM_SLEEP);
#else
	return calloc(n, nels);
#endif
}

void
netpgp_deallocate(void *ptr, size_t size)
{
#ifdef _KERNEL
	kmem_free(ptr, size);
#else
	USE_ARG(size);
	free(ptr);
#endif
}

#define HEXDUMP_LINELEN	16

#ifndef PRIsize
#define PRIsize	"z"
#endif

/* show hexadecimal/ascii dump */
ssize_t 
netpgp_hexdump(const void *vin, const size_t len, void *outvp, size_t size)
{
	const char	*in = (const char *)vin;
	size_t		 i;
	char		 line[HEXDUMP_LINELEN + 1];
	char		*out = (char *)outvp;
	int		 o;

	for (i = 0, o = 0 ; i < len ; i++) {
		if (i % HEXDUMP_LINELEN == 0) {
			o += snprintf(&out[o], size - o,
					"%.5" PRIsize "u |  ", i);
		} else if (i % (HEXDUMP_LINELEN / 2) == 0) {
			o += snprintf(&out[o], size - o, " ");
		}
		o += snprintf(&out[o], size - o, "%.02x ", (uint8_t)in[i]);
		line[i % HEXDUMP_LINELEN] =
			(isprint((uint8_t)in[i])) ? in[i] : '.';
		if (i % HEXDUMP_LINELEN == HEXDUMP_LINELEN - 1) {
			line[HEXDUMP_LINELEN] = 0x0;
			o += snprintf(&out[o], size - o, " | %s\n", line);
		}
	}
	if (i % HEXDUMP_LINELEN != 0) {
		for ( ; i % HEXDUMP_LINELEN != 0 ; i++) {
			o += snprintf(&out[o], size - o, "   ");
			if (i % (HEXDUMP_LINELEN / 2) == 0) {
				o += snprintf(&out[o], size - o, " ");
			}
			line[i % HEXDUMP_LINELEN] = ' ';
		}
		line[HEXDUMP_LINELEN] = 0x0;
		o += snprintf(&out[o], size - o, " | %s\n", line);
	}
	return (ssize_t)o;
}
