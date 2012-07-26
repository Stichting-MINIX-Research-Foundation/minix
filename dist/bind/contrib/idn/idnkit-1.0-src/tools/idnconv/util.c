#ifndef lint
static char *rcsid = "$Id: util.c,v 1.1.1.1 2003-06-04 00:27:08 marka Exp $";
#endif

/*
 * Copyright (c) 2000,2002 Japan Network Information Center.
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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <idn/resconf.h>
#include <idn/converter.h>
#include <idn/res.h>
#include <idn/utf8.h>

#include "util.h"
#include "selectiveencode.h"

extern int		line_number;

idn_result_t
selective_encode(idn_resconf_t conf, idn_action_t actions,
		 char *from, char *to, int tolen)
{
	for (;;) {
		int len;
		char *region_start, *region_end;
		idn_result_t r;
		char save;

		/*
		 * Find the region that needs conversion.
		 */
		r = idn_selectiveencode_findregion(from, &region_start,
						   &region_end);
		if (r == idn_notfound) {
			/*
			 * Not found.  Just copy the whole thing.
			 */
			if (tolen <= strlen(from))
				return (idn_buffer_overflow);
			(void)strcpy(to, from);
			return (idn_success);
		} else if (r != idn_success) {
			/* This should not happen.. */
			errormsg("internal error at line %d: %s\n",
				 line_number, idn_result_tostring(r));
			return (r);
		}

		/*
		 * We have found a region to convert.
		 * First, copy the prefix part verbatim.
		 */
		len = region_start - from;
		if (tolen < len) {
			errormsg("internal buffer overflow at line %d\n",
				 line_number);
			return (idn_buffer_overflow);
		}
		(void)memcpy(to, from, len);
		to += len;
		tolen -= len;

		/*
		 * Terminate the region with NUL.
		 */
		save = *region_end;
		*region_end = '\0';

		/*
		 * Encode the region.
		 */
		r = idn_res_encodename(conf, actions, region_start, to, tolen);

		/*
		 * Restore character.
		 */
		*region_end = save;

		if (r != idn_success)
			return (r);

		len = strlen(to);
		to += len;
		tolen -= len;

		from = region_end;
	}
}

idn_result_t
selective_decode(idn_resconf_t conf, idn_action_t actions,
		 char *from, char *to, int tolen)
{
	char *domain_name;
	char *ignored_chunk;
	char save;
	int len;
	idn_result_t r;

	/*
	 * While `*from' points to a character in a string which may be
	 * a domain name, `domain_name' refers to the beginning of the
	 * domain name.
	 */
	domain_name = NULL;

	/*
	 * We ignore chunks matching to the regular expression:
	 *    [\-\.][0-9A-Za-z\-\.]*
	 *
	 * While `*from' points to a character in such a chunk,
	 * `ignored_chunk' refers to the beginning of the chunk.
	 */
	ignored_chunk = NULL;

	for (;;) {
		if (*from == '-') {
			/*
			 * We don't recognize `.-' as a part of domain name.
			 */
			if (domain_name != NULL) {
				if (*(from - 1) == '.') {
					ignored_chunk = domain_name;
					domain_name = NULL;
				}
			} else if (ignored_chunk == NULL) {
				ignored_chunk = from;
			}

		} else if (*from == '.') {
			/*
			 * We don't recognize `-.' nor `..' as a part of
			 * domain name.
			 */
			if (domain_name != NULL) {
				if (*(from - 1) == '-' || *(from - 1) == '.') {
					ignored_chunk = domain_name;
					domain_name = NULL;
				}
			} else if (ignored_chunk == NULL) {
				ignored_chunk = from;
			}

		} else if (('a' <= *from && *from <= 'z') ||
			   ('A' <= *from && *from <= 'Z') ||
			   ('0' <= *from && *from <= '9')) {
			if (ignored_chunk == NULL && domain_name == NULL)
				domain_name = from;

		} else {
			if (ignored_chunk != NULL) {
				/*
				 * `from' reaches the end of the ignored chunk.
				 * Copy the chunk to `to'.
				 */
				len = from - ignored_chunk;
				if (tolen < len)
					return (idn_buffer_overflow);
				(void)memcpy(to, ignored_chunk, len);
				to += len;
				tolen -= len;

			} else if (domain_name != NULL) {
				/*
				 * `from' reaches the end of the domain name.
				 * Decode the domain name, and copy the result
				 * to `to'.
				 */
				save = *from;
				*from = '\0';
				r = idn_res_decodename(conf, actions,
						       domain_name, to, tolen);
				*from = save;

				if (r == idn_success) {
					len = strlen(to);
				} else if (r == idn_invalid_encoding) {
					len = from - domain_name;
					if (tolen < len)
						return (idn_buffer_overflow);
					(void)memcpy(to, domain_name, len);
				} else {
					return (r);
				}
				to += len;
				tolen -= len;
			}

			/*
			 * Copy a character `*from' to `to'.
			 */
			if (tolen < 1)
				return (idn_buffer_overflow);
			*to = *from;
			to++;
			tolen--;

			domain_name = NULL;
			ignored_chunk = NULL;

			if (*from == '\0')
				break;
		}

		from++;
	}

	return (idn_success);
}

void
set_defaults(idn_resconf_t conf) {
	idn_result_t r;

	if ((r = idn_resconf_setdefaults(conf)) != idn_success) {
		errormsg("error setting default configuration: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
load_conf_file(idn_resconf_t conf, const char *file) {
	idn_result_t r;

	if ((r = idn_resconf_loadfile(conf, file)) != idn_success) {
		errormsg("error reading configuration file: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
set_encoding_alias(const char *encoding_alias) {
	idn_result_t r;

	if ((r = idn_converter_resetalias()) != idn_success) {
		errormsg("cannot reset alias information: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}

	if ((r = idn_converter_aliasfile(encoding_alias)) != idn_success) {
		errormsg("cannot read alias file %s: %s\n",
			 encoding_alias, idn_result_tostring(r));
		exit(1);
	}
}

void
set_localcode(idn_resconf_t conf, const char *code) {
	idn_result_t r;

	r = idn_resconf_setlocalconvertername(conf, code,
					      IDN_CONVERTER_RTCHECK);
	if (r != idn_success) {
		errormsg("cannot create converter for codeset %s: %s\n",
			 code, idn_result_tostring(r));
		exit(1);
	}
}

void
set_idncode(idn_resconf_t conf, const char *code) {
	idn_result_t r;

	r = idn_resconf_setidnconvertername(conf, code,
					    IDN_CONVERTER_RTCHECK);
	if (r != idn_success) {
		errormsg("cannot create converter for codeset %s: %s\n",
			 code, idn_result_tostring(r));
		exit(1);
	}
}

void
set_delimitermapper(idn_resconf_t conf, unsigned long *delimiters,
		    int ndelimiters) {
	idn_result_t r;

	r = idn_resconf_addalldelimitermapucs(conf, delimiters, ndelimiters);
	if (r != idn_success) {
		errormsg("cannot add delimiter: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
set_localmapper(idn_resconf_t conf, char **mappers, int nmappers) {
	idn_result_t r;

	/* Add mapping. */
	r = idn_resconf_addalllocalmapselectornames(conf, 
						    IDN_MAPSELECTOR_DEFAULTTLD,
						    (const char **)mappers,
						    nmappers);
	if (r != idn_success) {
		errormsg("cannot add local map: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
set_nameprep(idn_resconf_t conf, char *version) {
	idn_result_t r;

	r = idn_resconf_setnameprepversion(conf, version);
	if (r != idn_success) {
		errormsg("error setting nameprep %s: %s\n",
			 version, idn_result_tostring(r));
		exit(1);
	}
}

void
set_mapper(idn_resconf_t conf, char **mappers, int nmappers) {
	idn_result_t r;

	/* Configure mapper. */
	r = idn_resconf_addallmappernames(conf, (const char **)mappers,
					  nmappers);
	if (r != idn_success) {
		errormsg("cannot add nameprep map: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
set_normalizer(idn_resconf_t conf, char **normalizers, int nnormalizer) {
	idn_result_t r;

	r = idn_resconf_addallnormalizernames(conf,
					      (const char **)normalizers,
					      nnormalizer);
	if (r != idn_success) {
		errormsg("cannot add normalizer: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
set_prohibit_checkers(idn_resconf_t conf, char **prohibits, int nprohibits) {
	idn_result_t r;

	r = idn_resconf_addallprohibitcheckernames(conf,
						   (const char **)prohibits,
						   nprohibits);
	if (r != idn_success) {
		errormsg("cannot add prohibit checker: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
set_unassigned_checkers(idn_resconf_t conf, char **unassigns, int nunassigns) {
	idn_result_t r;

	r = idn_resconf_addallunassignedcheckernames(conf,
						     (const char **)unassigns,
						     nunassigns);
	if (r != idn_success) {
		errormsg("cannot add unassigned checker: %s\n",
			 idn_result_tostring(r));
		exit(1);
	}
}

void
errormsg(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}


/*
 * Dynamic Stirng Buffer Utility
 */

void
strbuf_init(idnconv_strbuf_t *buf) {
	/*
	 * Initialize the given string buffer.
	 * Caller must allocate the structure (idnconv_strbuf_t)
	 * as an automatic variable or by malloc().
	 */
	buf->str = buf->local_buf;
	buf->str[0] = '\0';
	buf->size = sizeof(buf->local_buf);
}

void
strbuf_reset(idnconv_strbuf_t *buf) {
	/*
	 * Reset the given string buffer.
	 * Free memory allocated by this utility, and 
	 * re-initialize.
	 */
	if (buf->str != NULL && buf->str != buf->local_buf) {
		free(buf->str);
	}
	strbuf_init(buf);
}

char *
strbuf_get(idnconv_strbuf_t *buf) {
	/*
	 * Get the pointer of the buffer.
	 */
	return (buf->str);
}

size_t
strbuf_size(idnconv_strbuf_t *buf) {
	/*
	 * Get the allocated size of the buffer.
	 */
	return (buf->size);
}

char *
strbuf_copy(idnconv_strbuf_t *buf, const char *str) {
	/*
	 * Copy STR to BUF.
	 */
	size_t	len = strlen(str);

	if (strbuf_alloc(buf, len + 1) == NULL)
		return (NULL);
	strcpy(buf->str, str);
	return (buf->str);
}

char *
strbuf_append(idnconv_strbuf_t *buf, const char *str) {
	/*
	 * Append STR to the end of BUF.
	 */
	size_t	len1 = strlen(buf->str);
	size_t	len2 = strlen(str);
	char *p;
#define MARGIN	50

	p = strbuf_alloc(buf, len1 + len2 + 1 + MARGIN);
	if (p != NULL)
		strcpy(buf->str + len1, str);
	return (p);
}

char *
strbuf_alloc(idnconv_strbuf_t *buf, size_t size) {
	/*
	 * Reallocate the buffer of BUF if needed
	 * so that BUF can hold SIZE bytes of data at least.
	 */
	char *p;

	if (buf->size >= size)
		return (buf->str);
	if (buf->str == buf->local_buf) {
		if ((p = malloc(size)) == NULL)
			return (NULL);
		memcpy(p, buf->local_buf, sizeof(buf->local_buf));
	} else {
		if ((p = realloc(buf->str, size)) == NULL)
			return (NULL);
	}
	buf->str = p;
	buf->size = size;
	return (buf->str);
}

char *
strbuf_double(idnconv_strbuf_t *buf) {
	/*
	 * Double the size of the buffer of BUF.
	 */
	return (strbuf_alloc(buf, buf->size * 2));
}

char *
strbuf_getline(idnconv_strbuf_t *buf, FILE *fp) {
	/*
	 * Read a line from FP.
	 */
	char s[256];

	buf->str[0] = '\0';
	while (fgets(s, sizeof(s), fp) != NULL) {
		if (strbuf_append(buf, s) == NULL)
			return (NULL);
		if (strlen(s) < sizeof(s) - 1 || s[sizeof(s) - 2] == '\n')
			return (buf->str);
	}
	if (buf->str[0] != '\0')
		return (buf->str);
	return (NULL);
}
