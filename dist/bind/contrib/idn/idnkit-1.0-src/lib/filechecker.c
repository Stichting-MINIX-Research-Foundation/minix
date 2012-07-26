#ifndef lint
static char *rcsid = "$Id: filechecker.c,v 1.1.1.1 2003-06-04 00:25:52 marka Exp $";
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
#include <idn/ucsset.h>
#include <idn/filechecker.h>
#include <idn/debug.h>

#define SUPPORT_VERSIONING

struct idn__filechecker {
	idn_ucsset_t set;
};

static idn_result_t	read_file(const char *file, FILE *fp,
				  idn_ucsset_t set);
static int		get_range(char *s, unsigned long *ucs1,
				  unsigned long *ucs2);
static char		*get_ucs(char *p, unsigned long *vp);


idn_result_t
idn__filechecker_create(const char *file, idn__filechecker_t *ctxp) {
	FILE *fp;
	idn__filechecker_t ctx;
	idn_result_t r;

	assert(file != NULL && ctxp != NULL);

	TRACE(("idn__filechecker_create(file=\"%-.100s\")\n", file));

	if ((fp = fopen(file, "r")) == NULL) {
		WARNING(("idn__filechecker_create: cannot open %-.100s\n",
			 file));
		return (idn_nofile);
	}

	if ((ctx = malloc(sizeof(struct idn__filechecker))) == NULL)
		return (idn_nomemory);

	if ((r = idn_ucsset_create(&ctx->set)) != idn_success) {
		free(ctx);
		return (r);
	}

	r = read_file(file, fp, ctx->set);
	fclose(fp);

	if (r == idn_success) {
		idn_ucsset_fix(ctx->set);
		*ctxp = ctx;
	} else {
		idn_ucsset_destroy(ctx->set);
		free(ctx);
	}
	return (r);
}

void
idn__filechecker_destroy(idn__filechecker_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn__filechecker_destroy()\n"));

	idn_ucsset_destroy(ctx->set);
	free(ctx);
}

idn_result_t
idn__filechecker_lookup(idn__filechecker_t ctx, const unsigned long *str,
			const unsigned long **found) {
	idn_result_t r = idn_success;

	assert(ctx != NULL && str != NULL);

	TRACE(("idn__filechecker_lookup(str=\"%s\")\n",
	       idn__debug_ucs4xstring(str, 50)));

	while (*str != '\0') {
		int exists;

		r = idn_ucsset_lookup(ctx->set, *str, &exists);

		if (r != idn_success) {
			return (r);
		} else if (exists) {
			/* Found. */
			*found = str;
			return (idn_success);
		}
		str++;
	}
	*found = NULL;
	return (idn_success);
}

static idn_result_t
read_file(const char *file, FILE *fp, idn_ucsset_t set) {
	char line[256];
	idn_result_t r;
	int lineno = 0;

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *p = line;
		unsigned long ucs1, ucs2;

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
		if (!get_range(p, &ucs1, &ucs2)) {
			WARNING(("syntax error in file \"%-.100s\" line %d: "
				 "%-.100s", file, lineno, line));
			return (idn_invalid_syntax);
		}
		if ((r = idn_ucsset_addrange(set, ucs1, ucs2)) != idn_success)
			return (r);
	}
	return (idn_success);
}

static int
get_range(char *s, unsigned long *ucs1, unsigned long *ucs2) {
	if ((s = get_ucs(s, ucs1)) == NULL)
		return (0);
	*ucs2 = *ucs1;

	switch (s[0]) {
	case '\0':
	case '\n':
	case '#':
	case ';':
		return (1);
	case '-':
		break;
	default:
		return (0);
	}

	if ((s = get_ucs(s + 1, ucs2)) == NULL)
		return (0);

	if (*ucs1 > *ucs2) {
		INFO(("idn__filechecker_create: invalid range spec "
		      "U+%X-U+%X\n", *ucs1, *ucs2));
		return (0);
	}

	switch (s[0]) {
	case '\0':
	case '\n':
	case '#':
	case ';':
		return (1);
	default:
		return (0);
	}
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
		INFO(("idn__filechecker_create: UCS code point expected\n"));
		return (NULL);
	}
	p = end;

	/* Skip trailing space */
	while (isspace((unsigned char)*p))
		p++;
	return p;
}

idn_result_t
idn__filechecker_createproc(const char *parameter, void **ctxp) {
	return idn__filechecker_create(parameter, (idn__filechecker_t *)ctxp);
}

void
idn__filechecker_destroyproc(void *ctxp) {
	idn__filechecker_destroy((idn__filechecker_t)ctxp);
}

idn_result_t
idn__filechecker_lookupproc(void *ctx, const unsigned long *str,
			    const unsigned long **found) {
	return idn__filechecker_lookup((idn__filechecker_t)ctx, str, found);
}
