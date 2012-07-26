#ifndef lint
static char *rcsid = "$Id: filemapper.c,v 1.1.1.1 2003-06-04 00:25:53 marka Exp $";
#endif

/*
 * Copyright (c) 2001,2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/log.h>
#include <idn/logmacro.h>
#include <idn/debug.h>
#include <idn/ucs4.h>
#include <idn/ucsmap.h>
#include <idn/filemapper.h>

#define SUPPORT_VERSIONING

#define UCSBUF_LOCAL_SIZE	20

typedef struct ucsbuf {
	unsigned long *ucs;
	size_t size;
	size_t len;
	unsigned long local[UCSBUF_LOCAL_SIZE];
} ucsbuf_t;

struct idn__filemapper {
	idn_ucsmap_t map;
};

static void		ucsbuf_init(ucsbuf_t *b);
static idn_result_t	ucsbuf_grow(ucsbuf_t *b);
static idn_result_t	ucsbuf_append(ucsbuf_t *b, unsigned long v);
static void		ucsbuf_free(ucsbuf_t *b);
static idn_result_t	read_file(const char *file, FILE *fp,
				  idn_ucsmap_t map);
static idn_result_t	get_map(char *p, ucsbuf_t *b);
static char 		*get_ucs(char *p, unsigned long *vp);


idn_result_t
idn__filemapper_create(const char *file, idn__filemapper_t *ctxp) {
	FILE *fp;
	idn__filemapper_t ctx;
	idn_result_t r;

	assert(file != NULL && ctxp != NULL);

	TRACE(("idn__filemapper_create(file=\"%-.100s\")\n", file));

	if ((fp = fopen(file, "r")) == NULL) {
		WARNING(("idn__filemapper_create: cannot open %-.100s\n",
			 file));
		return (idn_nofile);
	}
	if ((ctx = malloc(sizeof(struct idn__filemapper))) == NULL)
		return (idn_nomemory);

	if ((r = idn_ucsmap_create(&ctx->map)) != idn_success) {
		free(ctx);
		return (r);
	}

	r = read_file(file, fp, ctx->map);
	fclose(fp);

	if (r == idn_success) {
		idn_ucsmap_fix(ctx->map);
		*ctxp = ctx;
	} else {
		idn_ucsmap_destroy(ctx->map);
		free(ctx);
	}
	return (r);
}

void
idn__filemapper_destroy(idn__filemapper_t ctx) {

	assert(ctx != NULL);

	TRACE(("idn__filemapper_destroy()\n"));

	idn_ucsmap_destroy(ctx->map);
	free(ctx);
}

idn_result_t
idn__filemapper_map(idn__filemapper_t ctx, const unsigned long *from,
		    unsigned long *to, size_t tolen)
{
	idn_result_t r = idn_success;
	ucsbuf_t ub;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn__filemapper_map(from=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50)));

	/* Initialize temporary buffer. */
	ucsbuf_init(&ub);

	while (*from != '\0') {
		/* Try mapping. */
		r = idn_ucsmap_map(ctx->map, *from, ub.ucs, ub.size, &ub.len);
		switch (r) {
		case idn_buffer_overflow:
			/* Temporary buffer too small.  Enlarge and retry. */
			if ((r = ucsbuf_grow(&ub)) != idn_success)
				break;
			continue;
		case idn_nomapping:
			/* There is no mapping. */
			r = idn_success;
			/* fallthrough */
		case idn_success:
			if (tolen < ub.len) {
				r = idn_buffer_overflow;
				goto ret;
			}
			memcpy(to, ub.ucs, sizeof(*to) * ub.len);
			to += ub.len;
			tolen -= ub.len;
			break;
		default:
			goto ret;
		}
		from++;
	}

 ret:
	ucsbuf_free(&ub);

	if (r == idn_success) {
		/* Terminate with NUL. */
		if (tolen == 0)
			return (idn_buffer_overflow);
		*to = '\0';
	}

	return (r);
}

static void
ucsbuf_init(ucsbuf_t *b) {
	b->ucs = b->local;
	b->size = UCSBUF_LOCAL_SIZE;
	b->len = 0;
}

static idn_result_t
ucsbuf_grow(ucsbuf_t *b) {
	unsigned long *newbuf;

	b->size *= 2;
	if (b->ucs == b->local) {
		b->ucs = malloc(sizeof(unsigned long) * b->size);
		if (b->ucs == NULL)
			return (idn_nomemory);
		memcpy(b->ucs, b->local, sizeof(b->local));
	} else {
		newbuf = realloc(b->ucs, sizeof(unsigned long) * b->size);
		if (newbuf == NULL)
			return (idn_nomemory);
		b->ucs = newbuf;
	}
	return (idn_success);
}

static idn_result_t
ucsbuf_append(ucsbuf_t *b, unsigned long v) {
	idn_result_t r;

	if (b->len + 1 > b->size) {
		r = ucsbuf_grow(b);
		if (r != idn_success)
			return (r);
	}
	b->ucs[b->len++] = v;
	return (idn_success);
}

static void
ucsbuf_free(ucsbuf_t *b) {
	if (b->ucs != b->local && b->ucs != NULL)
		free(b->ucs);
}

static idn_result_t
read_file(const char *file, FILE *fp, idn_ucsmap_t map) {
	char line[1024];
	ucsbuf_t ub;
	idn_result_t r = idn_success;
	int lineno = 0;

	ucsbuf_init(&ub);

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *p = line;

		lineno++;
		while (isspace((unsigned char)*p))
			p++;
		if (*p == '\0' || *p == '#')
			continue;
#ifdef SUPPORT_VERSIONING
		/* Skip version tag. */
		if (lineno == 1 && strncmp("version=", line, 8) == 0)
			continue;
#endif
	again:
		ub.len = 0;
		r = get_map(p, &ub);
		switch (r) {
		case idn_success:
			r = idn_ucsmap_add(map, ub.ucs[0],
					   &ub.ucs[1], ub.len - 1);
			break;
		case idn_buffer_overflow:
			if ((r = ucsbuf_grow(&ub)) != idn_success)
				break;
			goto again;
		case idn_invalid_syntax:
			WARNING(("syntax error in file \"%-.100s\" line %d: "
				 "%-.100s", file, lineno, line));
			/* fall through */
		default:
			ucsbuf_free(&ub);
			return (r);
		}
	}
	ucsbuf_free(&ub);
	return (r);
}

static idn_result_t
get_map(char *p, ucsbuf_t *b) {
	unsigned long v;
	idn_result_t r = idn_success;

	for (;;) {
		if ((p = get_ucs(p, &v)) == NULL)
			return (idn_invalid_syntax);
		if ((r = ucsbuf_append(b, v)) != idn_success)
			return (r);
		if (b->len == 1) {
			if (*p != ';')
				return (idn_invalid_syntax);
			p++;
			while (isspace((unsigned char)*p))
				p++;
		}

		if (*p == ';' || *p == '#' || *p == '\0')
			return (r);
	}
	return (r);
}

static char *
get_ucs(char *p, unsigned long *vp) {
	char *end;

	/* Skip leading space */
	while (isspace((unsigned char)*p))
		p++;

	/* Skip optional 'U+' */
	if (strncmp(p, "U+", 2) == 0)
		p += 2;

	*vp = strtoul(p, &end, 16);
	if (end == p) {
		INFO(("idn__filemapper_create: UCS code point expected\n"));
		return (NULL);
	}
	p = end;

	/* Skip trailing space */
	while (isspace((unsigned char)*p))
		p++;
	return p;
}

idn_result_t
idn__filemapper_createproc(const char *parameter, void **ctxp) {
	return idn__filemapper_create(parameter, (idn__filemapper_t *)ctxp);
}

void
idn__filemapper_destroyproc(void *ctxp) {
	idn__filemapper_destroy((idn__filemapper_t)ctxp);
}

idn_result_t
idn__filemapper_mapproc(void *ctx, const unsigned long *from,
			unsigned long *to, size_t tolen) {
	return idn__filemapper_map((idn__filemapper_t)ctx, from, to, tolen);
}
