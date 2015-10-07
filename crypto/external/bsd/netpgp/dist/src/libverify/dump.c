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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef isprint
#define isprint(x)	((x) >= ' ' && (x) <= '~')
#endif

#define HEXDUMP_LINELEN	16

#ifndef PRIsize
#define PRIsize	"z"
#endif

/* show hexadecimal/ascii dump */
static ssize_t 
hexdump(const char *in, const size_t len, void *outvp, size_t size)
{
	size_t	 i;
	char	 line[HEXDUMP_LINELEN + 1];
	char	*out = (char *)outvp;
	int	 o;

	for (i = 0, o = 0 ; i < len ; i++) {
		if (i % HEXDUMP_LINELEN == 0) {
			o += snprintf(&out[o], size - o,
					"%.5" PRIsize "u |  ", i);
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
			line[i % HEXDUMP_LINELEN] = ' ';
		}
		line[HEXDUMP_LINELEN] = 0x0;
		o += snprintf(&out[o], size - o, " | %s\n", line);
	}
	return (ssize_t)o;
}

void dumpmem(void */*p*/, size_t /*size*/);

/* just dump an area of memory to stdout */
void
dumpmem(void *vp, size_t size)
{
	ssize_t	cc;
	uint8_t	*p = (uint8_t *)vp;
	char	*buf;

	buf = calloc(1, size * 5);
	cc = hexdump((const char *)p, size, buf, size * 5);
	fprintf(stdout, "%.*s\n", (int)cc, buf);
	free(buf);
}
