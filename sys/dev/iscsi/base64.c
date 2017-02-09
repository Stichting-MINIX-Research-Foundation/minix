/*	$NetBSD: base64.c,v 1.1 2011/10/23 21:15:02 agc Exp $	*/

/*-
 * Copyright (c) 2005,2006,2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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
/*
 * Adapted for kernel mode use with minimal changes from
 * /usr/src/crypto/dist/heimdal/lib/roken/base64.c
 *
 * Original:
 * Copyright (c) 1995-2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "iscsi_globals.h"
#include "base64.h"

static char base64_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int
pos(char c)
{
	char *p;

	for (p = base64_chars; *p; p++) {
		if (*p == c) {
			return (int)(p - base64_chars);
		}
	}
	return -1;
}

int
base64_encode(const void *data, int size, uint8_t * buffer)
{
	uint8_t *p;
	int i;
	int c;
	const uint8_t *q;

	p = buffer;
	q = (const uint8_t *) data;
	*p++ = '0';
	*p++ = 'b';

	i = 0;
	for (i = 0; i < size;) {
		c = q[i++];
		c *= 256;
		if (i < size) {
			c += q[i];
		}
		i++;
		c *= 256;
		if (i < size) {
			c += q[i];
		}
		i++;
		p[0] = base64_chars[(c & 0x00fc0000) >> 18];
		p[1] = base64_chars[(c & 0x0003f000) >> 12];
		p[2] = base64_chars[(c & 0x00000fc0) >> 6];
		p[3] = base64_chars[(c & 0x0000003f) >> 0];
		if (i > size) {
			p[3] = '=';
		}
		if (i > size + 1) {
			p[2] = '=';
		}
		p += 4;
	}
	*p = 0;
	return strlen(buffer);
}

#define DECODE_ERROR 0xffffffff

static uint32_t
token_decode(uint8_t * token)
{
	int i;
	uint32_t val = 0;
	int marker = 0;

	if (strlen(token) < 4) {
		return DECODE_ERROR;
	}
	for (i = 0; i < 4; i++) {
		val *= 64;
		if (token[i] == '=') {
			marker++;
		} else if (marker > 0) {
			return DECODE_ERROR;
		} else {
			val += pos(token[i]);
		}
	}
	if (marker > 2) {
		return DECODE_ERROR;
	}
	return (marker << 24) | val;
}


uint8_t *
base64_decode(uint8_t * str, void *data, int *datalen)
{
	uint8_t *p, *q;
	uint32_t marker = 0;

	q = data;
	for (p = str; *p; p += 4) {
		uint32_t val = token_decode(p);
		marker = (val >> 24) & 0xff;
		if (val == DECODE_ERROR) {
			return NULL;
		}
		*q++ = (val >> 16) & 0xff;
		if (marker < 2) {
			*q++ = (val >> 8) & 0xff;
		}
		if (marker < 1) {
			*q++ = val & 0xff;
		}
	}
	*datalen = (int)(q - (uint8_t *) data);

	return p - marker + 1;
}
