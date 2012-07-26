/* $Id: util.h,v 1.1.1.1 2003-06-04 00:27:09 marka Exp $ */
/*
 * Copyright (c) 2000,2001 Japan Network Information Center.
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

#ifndef IDN_IDNCONV_UTIL_H
#define IDN_IDNCONV_UTIL_H 1

#include <idn/res.h>

#define IDNCONV_LOCALBUF_SIZE	512

typedef struct {
	char	*str;
	size_t	size;
	char	local_buf[IDNCONV_LOCALBUF_SIZE];
} idnconv_strbuf_t;

extern idn_result_t	selective_encode(idn_resconf_t conf,
					 idn_action_t actions, char *from,
					 char *to, int tolen);
extern idn_result_t	selective_decode(idn_resconf_t conf,
					 idn_action_t actions, char *from,
					 char *to, int tolen);
extern void		set_defaults(idn_resconf_t conf);
extern void		load_conf_file(idn_resconf_t conf, const char *file);
extern void		set_encoding_alias(const char *encoding_alias);
extern void		set_localcode(idn_resconf_t conf, const char *code);
extern void		set_idncode(idn_resconf_t conf, const char *code);
extern void		set_delimitermapper(idn_resconf_t conf,
					    unsigned long *delimiters,
					    int ndelimiters);
extern void		set_localmapper(idn_resconf_t conf,
					char **mappers, int nmappers);
extern void		set_nameprep(idn_resconf_t conf, char *version);
extern void		set_mapper(idn_resconf_t conf,
				   char **mappers, int nmappers);
extern void		set_normalizer(idn_resconf_t conf,
				       char **normalizer, int nnormalizer);
extern void		set_prohibit_checkers(idn_resconf_t conf,
					      char **prohibits,
					      int nprohibits);
extern void		set_unassigned_checkers(idn_resconf_t conf,
						char **unassigns,
						int nunassigns);
extern void		errormsg(const char *fmt, ...);
extern void		strbuf_init(idnconv_strbuf_t *buf);
extern void		strbuf_reset(idnconv_strbuf_t *buf);
extern char		*strbuf_get(idnconv_strbuf_t *buf);
extern size_t		strbuf_size(idnconv_strbuf_t *buf);
extern char		*strbuf_copy(idnconv_strbuf_t *buf, const char *str);
extern char		*strbuf_append(idnconv_strbuf_t *buf, const char *str);
extern char		*strbuf_alloc(idnconv_strbuf_t *buf, size_t size);
extern char		*strbuf_double(idnconv_strbuf_t *buf);
extern char		*strbuf_getline(idnconv_strbuf_t *buf, FILE *fp);

#endif /* IDN_IDNCONV_UTIL_H */
