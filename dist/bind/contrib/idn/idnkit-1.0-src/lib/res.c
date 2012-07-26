#ifndef lint
static char *rcsid = "$Id: res.c,v 1.1.1.1 2003-06-04 00:26:10 marka Exp $";
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/converter.h>
#include <idn/normalizer.h>
#include <idn/checker.h>
#include <idn/mapper.h>
#include <idn/mapselector.h>
#include <idn/delimitermap.h>
#include <idn/resconf.h>
#include <idn/res.h>
#include <idn/util.h>
#include <idn/debug.h>
#include <idn/ucs4.h>

#ifndef IDN_UTF8_ENCODING_NAME
#define IDN_UTF8_ENCODING_NAME "UTF-8"		/* by IANA */
#endif

#ifndef WITHOUT_ICONV
#define ENCODE_MASK \
	(IDN_LOCALCONV | IDN_DELIMMAP | IDN_LOCALMAP | IDN_MAP | \
	 IDN_NORMALIZE | IDN_PROHCHECK | IDN_UNASCHECK | IDN_BIDICHECK | \
	 IDN_ASCCHECK | IDN_IDNCONV | IDN_LENCHECK | IDN_ENCODE_QUERY | \
	 IDN_UNDOIFERR)
#define DECODE_MASK \
	(IDN_DELIMMAP | IDN_MAP | IDN_NORMALIZE | IDN_PROHCHECK | \
	 IDN_UNASCHECK | IDN_BIDICHECK | IDN_IDNCONV | IDN_ASCCHECK | \
	 IDN_RTCHECK | IDN_LOCALCONV | IDN_DECODE_QUERY)
#else
#define ENCODE_MASK \
	(IDN_DELIMMAP | IDN_LOCALMAP | IDN_MAP | IDN_NORMALIZE | \
	 IDN_PROHCHECK | IDN_UNASCHECK | IDN_BIDICHECK | IDN_ASCCHECK | \
	 IDN_IDNCONV | IDN_LENCHECK | IDN_ENCODE_QUERY | IDN_UNDOIFERR)
#define DECODE_MASK \
	(IDN_DELIMMAP | IDN_MAP | IDN_NORMALIZE | IDN_PROHCHECK | \
	 IDN_UNASCHECK | IDN_BIDICHECK | IDN_IDNCONV | IDN_ASCCHECK | \
	 IDN_RTCHECK | IDN_DECODE_QUERY)
#endif

#define MAX_LABEL_LENGTH	63

/*
 * label to convert.
 */
typedef struct labellist * labellist_t;
struct labellist {
	unsigned long *name;
	size_t name_length;
	unsigned long *undo_name;
	labellist_t next;
	labellist_t previous;
	int dot_followed;
};

typedef idn_result_t (*res_insnproc_t)(idn_resconf_t ctx,
				       labellist_t label);

static void		idn_res_initialize(void);
static idn_result_t	copy_verbatim(const char *from, char *to,
				      size_t tolen);
static idn_result_t	labellist_create(const unsigned long *name,
					 labellist_t *labelp);
static void		labellist_destroy(labellist_t label);
static idn_result_t	labellist_setname(labellist_t label,
					  const unsigned long *name);
static const unsigned long *
			labellist_getname(labellist_t label);
static const unsigned long *
			labellist_gettldname(labellist_t label);
static idn_result_t	labellist_getnamelist(labellist_t label,
					      unsigned long *name,
					      size_t label_length);
static void		labellist_undo(labellist_t label);
static labellist_t	labellist_tail(labellist_t label);
static labellist_t	labellist_previous(labellist_t label);

#ifndef WITHOUT_ICONV
static idn_result_t	label_localdecodecheck(idn_resconf_t ctx,
					       labellist_t label);
#endif
static idn_result_t	label_idnencode_ace(idn_resconf_t ctx,
					    labellist_t label);
static idn_result_t	label_idndecode(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_localmap(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_map(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_normalize(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_prohcheck(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_unascheck(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_bidicheck(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_asccheck(idn_resconf_t ctx, labellist_t label);
static idn_result_t	label_lencheck_ace(idn_resconf_t ctx,
					   labellist_t label);
static idn_result_t	label_lencheck_nonace(idn_resconf_t ctx,
					      labellist_t label);
static idn_result_t	label_rtcheck(idn_resconf_t ctx, idn_action_t actions,
				      labellist_t label,
				      const unsigned long *original_name);

static int initialized;
static int enabled;

void
idn_res_enable(int on_off) {
	if (!initialized) {
		idn_res_initialize();
	}

	if (on_off == 0) {
		enabled = 0;
	} else {
		enabled = 1;
	}
}

static void
idn_res_initialize(void) {
	if (!initialized) {
		char *value = getenv("IDN_DISABLE");

		if (value == NULL) {
			enabled = 1;
		} else {
			enabled = 0;
		}
		initialized = 1;
	}
}

idn_result_t
idn_res_encodename(idn_resconf_t ctx, idn_action_t actions, const char *from,
		    char *to, size_t tolen) {
	idn_converter_t local_converter = NULL;
	idn_converter_t idn_converter = NULL;
	idn_delimitermap_t delimiter_mapper;
	idn_result_t r;
	labellist_t labels = NULL, l;
	unsigned long *buffer = NULL;
	size_t buffer_length;
	int from_is_root;
	int idn_is_ace;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_res_encodename(actions=%s, from=\"%s\", tolen=%d)\n",
		idn__res_actionstostring(actions),
		idn__debug_xstring(from, 50), (int)tolen));

	if (actions & ~ENCODE_MASK) {
		WARNING(("idn_res_encodename: invalid actions 0x%x\n",
			 actions));
		r = idn_invalid_action;
		goto ret;
	}

	if (!initialized)
		idn_res_initialize();
	if (!enabled || actions == 0) {
		r = copy_verbatim(from, to, tolen);
		goto ret;
	} else if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto ret;
	}

	if (actions & IDN_ENCODE_QUERY) {
#ifndef WITHOUT_ICONV
		actions |= (IDN_LOCALCONV | IDN_DELIMMAP | IDN_LOCALMAP | \
			    IDN_MAP | IDN_NORMALIZE | IDN_PROHCHECK | \
			    IDN_BIDICHECK | IDN_IDNCONV | IDN_LENCHECK);
#else
		actions |= (IDN_DELIMMAP | IDN_LOCALMAP | IDN_MAP | \
			    IDN_NORMALIZE | IDN_PROHCHECK | IDN_BIDICHECK | \
			    IDN_IDNCONV | IDN_LENCHECK);
#endif
	}

	/*
	 * Convert `from' to UCS4.
	 */
	local_converter = idn_resconf_getlocalconverter(ctx);
#ifndef WITHOUT_ICONV
	if (local_converter == NULL) {
		r = idn_invalid_name;
		goto ret;
	}
#endif

	idn_converter = idn_resconf_getidnconverter(ctx);
	if (idn_converter != NULL &&
	    idn_converter_isasciicompatible(idn_converter))
		idn_is_ace = 1;
	else
		idn_is_ace = 0;

	buffer_length = tolen * 2;

	for (;;) {
		void *new_buffer;

		new_buffer = realloc(buffer, sizeof(*buffer) * buffer_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buffer = (unsigned long *)new_buffer;

		if (actions & IDN_LOCALCONV) {
			r = idn_converter_convtoucs4(local_converter, from,
						     buffer, buffer_length);
		} else {
			r = idn_ucs4_utf8toucs4(from, buffer, buffer_length);
		}
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buffer_length *= 2;
	}

	if (*buffer == '\0') {
		if (tolen <= 0) {
			r = idn_buffer_overflow;
			goto ret;
		}
		*to = '\0';
		r = idn_success;
		goto ret;
	}

	/*
	 * Delimiter map.
	 */
	if (actions & IDN_DELIMMAP) {
		TRACE(("res delimitermap(name=\"%s\")\n",
		       idn__debug_ucs4xstring(buffer, 50)));

		delimiter_mapper = idn_resconf_getdelimitermap(ctx);
		if (delimiter_mapper != NULL) {
			r = idn_delimitermap_map(delimiter_mapper, buffer,
						 buffer, buffer_length);
			idn_delimitermap_destroy(delimiter_mapper);
			if (r != idn_success)
				goto ret;
		}
		TRACE(("res delimitermap(): success (name=\"%s\")\n",
		       idn__debug_ucs4xstring(buffer, 50)));
	}

	from_is_root = (buffer[0] == '.' && buffer[1] == '\0');

	/*
	 * Split the name into a list of labels.
	 */
	r = labellist_create(buffer, &labels);
	if (r != idn_success)
		goto ret;

	/*
	 * Perform conversions and tests.
	 */
	for (l = labellist_tail(labels); l != NULL;
	     l = labellist_previous(l)) {

		if (!idn__util_ucs4isasciirange(labellist_getname(l))) {
			if (actions & IDN_LOCALMAP) {
				r = label_localmap(ctx, l);
				if (r != idn_success)
					goto ret;
			}
		}

		if (!idn__util_ucs4isasciirange(labellist_getname(l))) {
			if (actions & IDN_MAP) {
				r = label_map(ctx, l);
				if (r != idn_success)
					goto ret;
			}
			if (actions & IDN_NORMALIZE) {
				r = label_normalize(ctx, l);
				if (r != idn_success)
					goto ret;
			}
			if (actions & IDN_PROHCHECK) {
				r = label_prohcheck(ctx, l);
				if (r == idn_prohibited && 
				    (actions & IDN_UNDOIFERR)) {
					labellist_undo(l);
					continue;
				} else if (r != idn_success) {
					goto ret;
				}
			}
			if (actions & IDN_UNASCHECK) {
				r = label_unascheck(ctx, l);
				if (r == idn_prohibited && 
				    (actions & IDN_UNDOIFERR)) {
					labellist_undo(l);
					continue;
				} else if (r != idn_success) {
					goto ret;
				}
			}
			if (actions & IDN_BIDICHECK) {
				r = label_bidicheck(ctx, l);
				if (r == idn_prohibited && 
				    (actions & IDN_UNDOIFERR)) {
					labellist_undo(l);
					continue;
				} else if (r != idn_success) {
					goto ret;
				}
			}
		}

		if (actions & IDN_ASCCHECK) {
			r = label_asccheck(ctx, l);
			if (r == idn_prohibited && (actions & IDN_UNDOIFERR)) {
				labellist_undo(l);
				continue;
			} else if (r != idn_success) {
				goto ret;
			}
		}

		if (!idn__util_ucs4isasciirange(labellist_getname(l))) {
			if ((actions & IDN_IDNCONV) && idn_is_ace) {
				r = label_idnencode_ace(ctx, l);
				if (r != idn_success)
					goto ret;
			}
		}

		if (!from_is_root && (actions & IDN_LENCHECK)) {
			if (idn_is_ace)
				r = label_lencheck_ace(ctx, l);
			else
				r = label_lencheck_nonace(ctx, l);
			if (r == idn_invalid_length &&
			    (actions & IDN_UNDOIFERR)) {
				labellist_undo(l);
				continue;
			} else if (r != idn_success) {
				goto ret;
			}
		}
	}

	/*
	 * Concat a list of labels to a name.
	 */
	for (;;) {
		void *new_buffer;

		new_buffer = realloc(buffer, sizeof(*buffer) * buffer_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buffer = (unsigned long *)new_buffer;

		r = labellist_getnamelist(labels, buffer, buffer_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buffer_length *= 2;
	}

	if ((actions & IDN_IDNCONV) && idn_converter != NULL && !idn_is_ace) {
		r = idn_converter_convfromucs4(idn_converter, buffer, to,
					       tolen);
	} else {
		r = idn_ucs4_ucs4toutf8(buffer, to, tolen);
	}

ret:
	if (r == idn_success) {
		TRACE(("idn_res_encodename(): success (to=\"%s\")\n",
		       idn__debug_xstring(to, 50)));
	} else {
		TRACE(("idn_res_encodename(): %s\n", idn_result_tostring(r)));
	}
	free(buffer);
	if (local_converter != NULL)
		idn_converter_destroy(local_converter);
	if (idn_converter != NULL)
		idn_converter_destroy(idn_converter);
	if (labels != NULL)
		labellist_destroy(labels);
	return (r);
}

idn_result_t
idn_res_decodename(idn_resconf_t ctx, idn_action_t actions, const char *from,
		    char *to, size_t tolen) {
	idn_converter_t local_converter = NULL;
	idn_converter_t idn_converter = NULL;
	idn_delimitermap_t delimiter_mapper;
	idn_result_t r;
	labellist_t labels = NULL, l;
	unsigned long *buffer = NULL;
	unsigned long *saved_name = NULL;
	size_t buffer_length;
	int idn_is_ace;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_res_decodename(actions=%s, from=\"%s\", tolen=%d)\n",
		idn__res_actionstostring(actions),
		idn__debug_xstring(from, 50), (int)tolen));

	if (actions & ~DECODE_MASK) {
		WARNING(("idn_res_decodename: invalid actions 0x%x\n",
			 actions));
		r = idn_invalid_action;
		goto ret;
	}

	if (!initialized)
		idn_res_initialize();
	if (!enabled || actions == 0) {
		r = copy_verbatim(from, to, tolen);
		goto ret;
	} else if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto ret;
	}

	if (actions & IDN_DECODE_QUERY) {
#ifndef WITHOUT_ICONV
		actions |= (IDN_DELIMMAP | IDN_MAP | IDN_NORMALIZE | \
			    IDN_PROHCHECK | IDN_BIDICHECK | IDN_IDNCONV | \
			    IDN_RTCHECK | IDN_LOCALCONV);
#else
		actions |= (IDN_DELIMMAP | IDN_MAP | IDN_NORMALIZE | \
			    IDN_PROHCHECK | IDN_BIDICHECK | IDN_IDNCONV | \
			    IDN_RTCHECK);
#endif
	}

	/*
	 * Convert `from' to UCS4.
	 */
	local_converter = idn_resconf_getlocalconverter(ctx);
#ifndef WITHOUT_ICONV
	if (local_converter == NULL) {
		r = idn_invalid_name;
		goto ret;
	}
#endif

	idn_converter = idn_resconf_getidnconverter(ctx);
	if (idn_converter != NULL &&
	    idn_converter_isasciicompatible(idn_converter))
		idn_is_ace = 1;
	else
		idn_is_ace = 0;

	buffer_length = tolen * 2;

	TRACE(("res idndecode(name=\"%s\")\n", idn__debug_xstring(from, 50)));

	for (;;) {
		void *new_buffer;

		new_buffer = realloc(buffer, sizeof(*buffer) * buffer_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buffer = (unsigned long *)new_buffer;

		if ((actions & IDN_IDNCONV) &&
		     idn_converter != NULL && !idn_is_ace) {
			r = idn_converter_convtoucs4(idn_converter, from,
						     buffer, buffer_length);
		} else {
			r = idn_ucs4_utf8toucs4(from, buffer, buffer_length);
		}
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buffer_length *= 2;
	}

	if (*buffer == '\0') {
		if (tolen <= 0) {
			r = idn_buffer_overflow;
			goto ret;
		}
		*to = '\0';
		r = idn_success;
		goto ret;
	}

	/*
	 * Delimiter map.
	 */
	if (actions & IDN_DELIMMAP) {
		TRACE(("res delimitermap(name=\"%s\")\n",
		       idn__debug_ucs4xstring(buffer, 50)));

		delimiter_mapper = idn_resconf_getdelimitermap(ctx);
		if (delimiter_mapper != NULL) {
			r = idn_delimitermap_map(delimiter_mapper, buffer,
						 buffer, buffer_length);
			idn_delimitermap_destroy(delimiter_mapper);
			if (r != idn_success)
				goto ret;
		}
		TRACE(("res delimitermap(): success (name=\"%s\")\n",
		       idn__debug_ucs4xstring(buffer, 50)));
	}

	/*
	 * Split the name into a list of labels.
	 */
	r = labellist_create(buffer, &labels);
	if (r != idn_success)
		goto ret;

	/*
	 * Perform conversions and tests.
	 */
	for (l = labellist_tail(labels); l != NULL;
	     l = labellist_previous(l)) {

		free(saved_name);
		saved_name = NULL;

		if (!idn__util_ucs4isasciirange(labellist_getname(l))) {
			if (actions & IDN_MAP) {
				r = label_map(ctx, l);
				if (r != idn_success)
					goto ret;
			}
			if (actions & IDN_NORMALIZE) {
				r = label_normalize(ctx, l);
				if (r != idn_success)
					goto ret;
			}
			if (actions & IDN_PROHCHECK) {
				r = label_prohcheck(ctx, l);
				if (r == idn_prohibited) {
					labellist_undo(l);
					continue;
				} else if (r != idn_success) {
					goto ret;
				}
			}
			if (actions & IDN_UNASCHECK) {
				r = label_unascheck(ctx, l);
				if (r == idn_prohibited) {
					labellist_undo(l);
					continue;
				} else if (r != idn_success) {
					goto ret;
				}
			}
			if (actions & IDN_BIDICHECK) {
				r = label_bidicheck(ctx, l);
				if (r == idn_prohibited) {
					labellist_undo(l);
					continue;
				} else if (r != idn_success) {
					goto ret;
				}
			}
		}

		if ((actions & IDN_IDNCONV) && idn_is_ace) {
			saved_name = idn_ucs4_strdup(labellist_getname(l));
			if (saved_name == NULL) {
				r = idn_nomemory;
				goto ret;
			}
			r = label_idndecode(ctx, l);
			if (r == idn_invalid_encoding) {
				labellist_undo(l);
				continue;
			} else if (r != idn_success) {
				goto ret;
			}
		}
		if ((actions & IDN_RTCHECK) && saved_name != NULL) {
			r = label_rtcheck(ctx, actions, l, saved_name);
			if (r == idn_invalid_encoding) {
				labellist_undo(l);
				continue;
			} else if (r != idn_success) {
				goto ret;
			}
		}

#ifndef WITHOUT_ICONV
		if (actions & IDN_LOCALCONV) {
			r = label_localdecodecheck(ctx, l);
			if (r != idn_success)
				goto ret;
		}
#endif
	}

	/*
	 * Concat a list of labels to a name.
	 */
	for (;;) {
		void *new_buffer;

		new_buffer = realloc(buffer, sizeof(*buffer) * buffer_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buffer = (unsigned long *)new_buffer;

		r = labellist_getnamelist(labels, buffer, buffer_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buffer_length *= 2;
	}

	if (actions & IDN_LOCALCONV) {
		r = idn_converter_convfromucs4(local_converter, buffer, to,
					       tolen);
	} else {
		r = idn_ucs4_ucs4toutf8(buffer, to, tolen);
	}

ret:
	if (r == idn_success) {
		TRACE(("idn_res_decodename(): success (to=\"%s\")\n",
		       idn__debug_xstring(to, 50)));
	} else {
		TRACE(("idn_res_decodename(): %s\n", idn_result_tostring(r)));
	}
	free(saved_name);
	free(buffer);
	if (local_converter != NULL)
		idn_converter_destroy(local_converter);
	if (idn_converter != NULL)
		idn_converter_destroy(idn_converter);
	if (labels != NULL)
		labellist_destroy(labels);
	return (r);
}

idn_result_t
idn_res_decodename2(idn_resconf_t ctx, idn_action_t actions, const char *from,
		    char *to, size_t tolen, const char *auxencoding) {
#ifdef WITHOUT_ICONV
	return idn_failure;

#else /* WITHOUT_ICONV */
	idn_result_t r;
	idn_converter_t aux_converter = NULL;
	unsigned long *buffer_ucs4 = NULL;
	char *buffer_utf8 = NULL;
	size_t buffer_length;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_res_decodename2(actions=%s, from=\"%s\", tolen=%d, "
		"auxencoding=\"%s\")\n",
		idn__res_actionstostring(actions),
		idn__debug_xstring(from, 50), (int)tolen,
		(auxencoding != NULL) ? auxencoding : "<null>"));

	if (!initialized)
		idn_res_initialize();
	if (!enabled || actions == 0) {
		r = copy_verbatim(from, to, tolen);
		goto ret;
	} else if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto ret;
	}

	if (auxencoding == NULL ||
	    strcmp(auxencoding, IDN_UTF8_ENCODING_NAME) == 0 ||
	    strcmp(auxencoding, "UTF-8") == 0) {
		return idn_res_decodename(ctx, actions, from, to, tolen);
	}

	/*
	 * Convert `from' to UCS4.
	 */
	r = idn_resconf_setauxidnconvertername(ctx, auxencoding,
					       IDN_CONVERTER_DELAYEDOPEN);
	if (r != idn_success) {
		goto ret;
	}

	aux_converter = idn_resconf_getauxidnconverter(ctx);
	if (aux_converter == NULL) {
		r = idn_failure;
		goto ret;
	}

	buffer_length = tolen * 2;
	for (;;) {
		void *new_buffer;

		new_buffer = realloc(buffer_ucs4,
				     sizeof(*buffer_ucs4) * buffer_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buffer_ucs4 = (unsigned long *)new_buffer;

		r = idn_converter_convtoucs4(aux_converter, from,
					     buffer_ucs4,
					     buffer_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buffer_length *= 2;
	}

	if (*buffer_ucs4 == '\0') {
		if (tolen <= 0) {
			r = idn_buffer_overflow;
			goto ret;
		}
		*to = '\0';
		r = idn_success;
		goto ret;
	}

	/*
	 * Convert `buffer_ucs4' to UTF-8.
	 */
	buffer_length = tolen * 2;
	for (;;) {
		void *new_buffer;

		new_buffer = realloc(buffer_utf8,
				     sizeof(*buffer_utf8) * buffer_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buffer_utf8 = (char *)new_buffer;
		r = idn_ucs4_ucs4toutf8(buffer_ucs4, buffer_utf8,
					buffer_length);

		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buffer_length *= 2;
	}
	
	if (*buffer_utf8 == '\0') {
		if (tolen <= 0) {
			r = idn_buffer_overflow;
			goto ret;
		}
		*to = '\0';
		r = idn_success;
		goto ret;
	}

	r = idn_res_decodename(ctx, actions, buffer_utf8, to, tolen);

ret:
	if (r == idn_success) {
		TRACE(("idn_res_decodename2(): success (to=\"%s\")\n",
		       idn__debug_xstring(to, 50)));
	} else {
		TRACE(("idn_res_decodename2(): %s\n", idn_result_tostring(r)));
	}
	free(buffer_ucs4);
	free(buffer_utf8);
	if (aux_converter != NULL)
		idn_converter_destroy(aux_converter);

	return (r);

#endif /* WITHOUT_ICONV */
}

static idn_result_t
copy_verbatim(const char *from, char *to, size_t tolen) {
	size_t fromlen = strlen(from);

	if (fromlen + 1 > tolen)
		return (idn_buffer_overflow);
	(void)memcpy(to, from, fromlen + 1);
	return (idn_success);
}

static idn_result_t
labellist_create(const unsigned long *name, labellist_t *labelp) {
	size_t length, malloc_length;
	labellist_t head_label = NULL;
	labellist_t tail_label = NULL;
	labellist_t new_label = NULL;
	const unsigned long *endp = NULL;
	idn_result_t r;

	while (*name != '\0') {
		for (endp = name; *endp != '.' && *endp != '\0'; endp++)
			;  /* nothing to be done */
		length = (endp - name) + 1;
		malloc_length = length + 15;  /* add 15 for margin */

		new_label = (labellist_t)
			    malloc(sizeof(struct labellist));
		if (new_label == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		if (head_label == NULL)
			head_label = new_label;

		new_label->name = NULL;
		new_label->undo_name = NULL;
		new_label->name_length = malloc_length;
		new_label->next = NULL;
		new_label->previous = NULL;
		new_label->dot_followed = (*endp == '.');

		new_label->name = (unsigned long *)
				  malloc(sizeof(long) * malloc_length);
		if (new_label->name == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		memcpy(new_label->name, name, sizeof(long) * length);
		*(new_label->name + length - 1) = '\0';

		new_label->undo_name = (unsigned long *)
				       malloc(sizeof(long) * malloc_length);
		if (new_label->undo_name == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		memcpy(new_label->undo_name, name, sizeof(long) * length);
		*(new_label->undo_name + length - 1) = '\0';

		if (tail_label != NULL) {
			tail_label->next = new_label;
			new_label->previous = tail_label;
		}
		tail_label = new_label;

		if (*endp == '.')
			name = endp + 1;
		else
			name = endp;
	}

	*labelp = head_label;
	r = idn_success;

ret:
	if (r != idn_success) {
		if (new_label != NULL) {
			free(new_label->name);
			free(new_label->undo_name);
			free(new_label);
		}
		if (head_label != NULL)
			labellist_destroy(head_label);
	}
	return (r);
}


static void
labellist_destroy(labellist_t label) {
	labellist_t l, l_next;

	for (l = label; l != NULL; l = l_next) {
		l_next = l->next;
		free(l->name);
		free(l->undo_name);
		free(l);
	}
}

static idn_result_t
labellist_setname(labellist_t label, const unsigned long *name) {
	unsigned long *new_name;
	size_t length, new_length;

	length = idn_ucs4_strlen(name) + 1;
	new_length = length + 15;  /* add 15 for margin */

	if (label->name_length < new_length) {
		new_name = (unsigned long *)
			   realloc(label->name, sizeof(long) * new_length);
		if (new_name == NULL)
			return (idn_nomemory);
		label->name = new_name;
		label->name_length = new_length;
	}
	memcpy(label->name, name, sizeof(long) * length);

	return (idn_success);
}

static const unsigned long *
labellist_getname(labellist_t label) {
	return (label->name);
}

static const unsigned long *
labellist_gettldname(labellist_t label) {
	labellist_t l;

	if (label->previous == NULL && label->next == NULL &&
	    !label->dot_followed)
		return (idn_mapselector_getnotld());

	for (l = label; l->next != NULL; l = l->next)
		;  /* nothing to be done */

	return (l->name);
}

static idn_result_t
labellist_getnamelist(labellist_t label, unsigned long *name,
			  size_t name_length) {
	static const unsigned long dot_string[] = {0x002e, 0x0000};  /* "." */
	size_t length;
	labellist_t l;

	for (l = label, length = 0; l != NULL; l = l->next)
		length += idn_ucs4_strlen(l->name) + 1;  /* name + `.' */
	length++;  /* for NUL */

	if (name_length < length)
		return (idn_buffer_overflow);

	*name = '\0';
	for (l = label; l != NULL; l = l->next) {
		idn_ucs4_strcat(name, l->name);
		name += idn_ucs4_strlen(name);
		if (l->dot_followed)
			idn_ucs4_strcat(name, dot_string);
	}
	return (idn_success);
}

static void
labellist_undo(labellist_t label) {
	size_t length;

	length = idn_ucs4_strlen(label->undo_name) + 1;
	memcpy(label->name, label->undo_name, sizeof(long) * length);
}

static labellist_t
labellist_tail(labellist_t label) {
	labellist_t l;

	if (label == NULL)
		return (NULL);
	for (l = label; l->next != NULL; l = l->next)
		;  /* nothing to be done */
	return (l);
}

static labellist_t
labellist_previous(labellist_t label) {
	return (label->previous);
}

#ifndef WITHOUT_ICONV

static idn_result_t
label_localdecodecheck(idn_resconf_t ctx, labellist_t label) {
	idn_converter_t local_converter = NULL;
	const unsigned long *from;
	char *to = NULL;
	size_t to_length;
	idn_result_t r;

	from = labellist_getname(label);
	to_length = idn_ucs4_strlen(from) + 1 + 15;  /* 15 for margin */
	TRACE(("res ucs4tolocal_check(label=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50)));

	local_converter = idn_resconf_getlocalconverter(ctx);
	if (local_converter == NULL) {
		r = idn_success;
		goto ret;
	}

	for (;;) {
		char *new_buffer;

		new_buffer = (char *)realloc(to, to_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		to = new_buffer;
		r = idn_converter_convfromucs4(local_converter, from, to,
					       to_length);
		if (r == idn_success)
			break;
		else if (r == idn_nomapping) {
			r = label_idnencode_ace(ctx, label);
			if (r != idn_success)
				goto ret;
			break;
		} else if (r != idn_buffer_overflow) {
			goto ret;
		}
		to_length *= 2;
	}

	r = idn_success;
ret:
	TRACE(("res ucs4tolocal_check(): %s\n", idn_result_tostring(r)));
	if (local_converter != NULL)
		idn_converter_destroy(local_converter);
	free(to);
	return (r);
}

#endif /* !WITHOUT_ICONV */

static idn_result_t
label_idndecode(idn_resconf_t ctx, labellist_t label) {
	idn_converter_t idn_converter = NULL;
	const unsigned long *from;
	char *ascii_from = NULL;
	unsigned long *to = NULL;
	size_t from_length, to_length;
	idn_result_t r;

	from = labellist_getname(label);
	from_length = idn_ucs4_strlen(from) + 1;
	TRACE(("res idntoucs4(label=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50)));

	idn_converter = idn_resconf_getidnconverter(ctx);
	if (idn_converter == NULL) {
		r = idn_success;
		goto ret;
	}

	for (;;) {
		char *new_buffer;

		new_buffer = (char *) realloc(ascii_from, from_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		ascii_from = new_buffer;
		r = idn_ucs4_ucs4toutf8(from, ascii_from, from_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;
		from_length *= 2;
	}

	to = NULL;
	to_length = from_length;

	for (;;) {
		unsigned long *new_buffer;

		new_buffer = (unsigned long *)
			     realloc(to, sizeof(long) * to_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		to = new_buffer;
		r = idn_converter_convtoucs4(idn_converter, ascii_from, to,
					     to_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;
		to_length *= 2;
	}

	r = labellist_setname(label, to);
ret:
	if (r == idn_success) {
		TRACE(("res idntoucs4(): success (label=\"%s\")\n",
		       idn__debug_ucs4xstring(labellist_getname(label),
					      50)));
	} else {
		TRACE(("res idntoucs4(): %s\n", idn_result_tostring(r)));
	}
	if (idn_converter != NULL)
		idn_converter_destroy(idn_converter);
	free(to);
	free(ascii_from);
	return (r);
}

static idn_result_t
label_idnencode_ace(idn_resconf_t ctx, labellist_t label) {
	idn_converter_t idn_converter = NULL;
	const unsigned long *from;
	char *ascii_to = NULL;
	unsigned long *to = NULL;
	size_t to_length;
	idn_result_t r;

	from = labellist_getname(label);
	TRACE(("res ucs4toidn(label=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50)));

	idn_converter = idn_resconf_getidnconverter(ctx);
	if (idn_converter == NULL) {
		r = idn_success;
		goto ret;
	}

	ascii_to = NULL;
	to_length = idn_ucs4_strlen(from) * 4 + 16;  /* add mergin */

	for (;;) {
		char *new_buffer;

		new_buffer = (char *) realloc(ascii_to, to_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		ascii_to = new_buffer;
		r = idn_converter_convfromucs4(idn_converter, from, ascii_to,
					       to_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;
		to_length *= 2;
	}

	for (;;) {
		unsigned long *new_buffer;

		new_buffer = (unsigned long *)
			     realloc(to, sizeof(long) * to_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		to = new_buffer;
		r = idn_ucs4_utf8toucs4(ascii_to, to, to_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;
		to_length *= 2;
	}

	if (r != idn_success)
		goto ret;

	r = labellist_setname(label, to);
ret:
	if (r == idn_success) {
		TRACE(("res ucs4toidn(): success (label=\"%s\")\n",
		       idn__debug_ucs4xstring(labellist_getname(label),
					      50)));
	} else {
		TRACE(("res ucs4toidn(): %s\n", idn_result_tostring(r)));
	}
	if (idn_converter != NULL)
		idn_converter_destroy(idn_converter);
	free(to);
	free(ascii_to);
	return (r);
}

static idn_result_t
label_localmap(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *from;
	const unsigned long *tld;
	unsigned long *to = NULL;
	size_t to_length;
	idn_mapselector_t local_mapper;
	idn_result_t r;

	from = labellist_getname(label);
	tld = labellist_gettldname(label);
	TRACE(("res localmap(label=\"%s\", tld=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50),
	       idn__debug_ucs4xstring(tld, 50)));

	local_mapper = idn_resconf_getlocalmapselector(ctx);
	if (local_mapper == NULL) {
		r = idn_success;
		goto ret;
	}

	if (tld == from)
		tld = idn_mapselector_getdefaulttld();
	to_length = idn_ucs4_strlen(from) + 1 + 15;  /* 15 for margin */

	for (;;) {
		unsigned long *new_buffer;

		new_buffer = (unsigned long *)
			     realloc(to, sizeof(long) * to_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		to = new_buffer;
		r = idn_mapselector_map2(local_mapper, from, tld, to,
					 to_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;
		to_length *= 2;
	}

	r = labellist_setname(label, to);
ret:
	if (r == idn_success) {
		TRACE(("res localmap(): success (label=\"%s\")\n",
		       idn__debug_ucs4xstring(labellist_getname(label),
					      50)));
	} else {
		TRACE(("res localmap(): %s\n", idn_result_tostring(r)));
	}
	if (local_mapper != NULL)
		idn_mapselector_destroy(local_mapper);
	free(to);
	return (r);
}

static idn_result_t
label_map(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *from;
	unsigned long *to = NULL;
	size_t to_length;
	idn_mapper_t mapper;
	idn_result_t r;

	from = labellist_getname(label);
	TRACE(("res map(label=\"%s\")\n", idn__debug_ucs4xstring(from, 50)));

	mapper = idn_resconf_getmapper(ctx);
	if (mapper == NULL) {
		r = idn_success;
		goto ret;
	}
	to_length = idn_ucs4_strlen(from) + 1 + 15;  /* 15 for margin */

	for (;;) {
		unsigned long *new_buffer;

		new_buffer = (unsigned long *)
			     realloc(to, sizeof(long) * to_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		to = new_buffer;
		r = idn_mapper_map(mapper, from, to, to_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;
		to_length *= 2;
	}

	r = labellist_setname(label, to);
ret:
	if (r == idn_success) {
		TRACE(("res map(): success (label=\"%s\")\n",
		       idn__debug_ucs4xstring(labellist_getname(label),
					      50)));
	} else {
		TRACE(("res map(): %s\n", idn_result_tostring(r)));
	}
	if (mapper != NULL)
		idn_mapper_destroy(mapper);
	free(to);
	return (r);
}

static idn_result_t
label_normalize(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *from;
	unsigned long *to = NULL;
	size_t to_length;
	idn_normalizer_t normalizer;
	idn_result_t r;

	from = labellist_getname(label);
	TRACE(("res normalzie(label=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50)));

	normalizer = idn_resconf_getnormalizer(ctx);
	if (normalizer == NULL) {
		r = idn_success;
		goto ret;
	}
	to_length = idn_ucs4_strlen(from) + 1 + 15;  /* 15 for margin */

	for (;;) {
		unsigned long *new_buffer;

		new_buffer = (unsigned long *)
			     realloc(to, sizeof(long) * to_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		to = new_buffer;
		r = idn_normalizer_normalize(normalizer, from, to, to_length);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;
		to_length *= 2;
	}

	r = labellist_setname(label, to);
ret:
	if (r == idn_success) {
		TRACE(("res normalize(): success (label=\"%s\")\n",
		       idn__debug_ucs4xstring(labellist_getname(label),
					      50)));
	} else {
		TRACE(("res normalize(): %s\n", idn_result_tostring(r)));
	}
	if (normalizer != NULL)
		idn_normalizer_destroy(normalizer);
	free(to);
	return (r);
}

static idn_result_t
label_prohcheck(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *name, *found;
	idn_checker_t prohibit_checker;
	idn_result_t r;

	name = labellist_getname(label);
	TRACE(("res prohcheck(label=\"%s\")\n",
	       idn__debug_ucs4xstring(name, 50)));

	prohibit_checker = idn_resconf_getprohibitchecker(ctx);
	if (prohibit_checker == NULL) {
		r = idn_success;
		goto ret;
	}

	r = idn_checker_lookup(prohibit_checker, name, &found);
	idn_checker_destroy(prohibit_checker);
	if (r == idn_success && found != NULL)
		r = idn_prohibited;

ret:
	TRACE(("res prohcheck(): %s\n", idn_result_tostring(r)));
	return (r);
}

static idn_result_t
label_unascheck(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *name, *found;
	idn_checker_t unassigned_checker;
	idn_result_t r;

	name = labellist_getname(label);
	TRACE(("res unascheck(label=\"%s\")\n",
	       idn__debug_ucs4xstring(name, 50)));

	unassigned_checker = idn_resconf_getunassignedchecker(ctx);
	if (unassigned_checker == NULL) {
		r = idn_success;
		goto ret;
	}

	r = idn_checker_lookup(unassigned_checker, name, &found);
	idn_checker_destroy(unassigned_checker);
	if (r == idn_success && found != NULL)
		r = idn_prohibited;

ret:
	TRACE(("res unascheck(): %s\n", idn_result_tostring(r)));
	return (r);
}

static idn_result_t
label_bidicheck(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *name, *found;
	idn_checker_t bidi_checker;
	idn_result_t r;

	name = labellist_getname(label);
	TRACE(("res bidicheck(label=\"%s\")\n",
	       idn__debug_ucs4xstring(name, 50)));

	bidi_checker = idn_resconf_getbidichecker(ctx);
	if (bidi_checker == NULL) {
		r = idn_success;
		goto ret;
	}

	r = idn_checker_lookup(bidi_checker, name, &found);
	idn_checker_destroy(bidi_checker);
	if (r == idn_success && found != NULL)
		r = idn_prohibited;

ret:
	TRACE(("res bidicheck(): %s\n", idn_result_tostring(r)));
	return (r);
}

static idn_result_t
label_asccheck(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *name, *n;
	idn_result_t r;

	name = labellist_getname(label);
	TRACE(("res asccheck(label=\"%s\")\n",
	       idn__debug_ucs4xstring(name, 50)));

	if (*name == '-') {
		r = idn_prohibited;
		goto ret;
	}

	for (n = name; *n != '\0'; n++) {
		if (*n <= '\177') {
			if ((*n < '0' || *n > '9') &&
			    (*n < 'A' || *n > 'Z') &&
			    (*n < 'a' || *n > 'z') &&
			    *n != '-') {
				r  = idn_prohibited;
				goto ret;
			}
		}
	}

	if (n > name && *(n - 1) == '-') {
		r  = idn_prohibited;
		goto ret;
	}

	r = idn_success;
ret:	
	TRACE(("res asccheck(): %s\n", idn_result_tostring(r)));
	return (r);
}

static idn_result_t
label_lencheck_ace(idn_resconf_t ctx, labellist_t label) {
	const unsigned long *name;
	size_t name_length;
	idn_result_t r;

	name = labellist_getname(label);
	name_length = idn_ucs4_strlen(name);
	TRACE(("res lencheck(label=\"%s\")\n",
	       idn__debug_ucs4xstring(name, 50)));

	if (name_length == 0 || name_length > MAX_LABEL_LENGTH) {
		r = idn_invalid_length;
		goto ret;
	}

	r = idn_success;
ret:	
	TRACE(("res lencheck(): %s\n", idn_result_tostring(r)));
	return (r);
}

static idn_result_t
label_lencheck_nonace(idn_resconf_t ctx, labellist_t label) {
	idn_converter_t idn_converter;
	const unsigned long *from;
	size_t to_length;
	idn_result_t r;
	char *buffer = NULL;
	size_t buffer_length;

	from = labellist_getname(label);
	TRACE(("res lencheck(label=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50)));

	buffer_length = idn_ucs4_strlen(from) * 4 + 16; /* 16 for margin */
	idn_converter = idn_resconf_getidnconverter(ctx);

	for (;;) {
		void *new_buffer;

		new_buffer = realloc(buffer, sizeof(*buffer) * buffer_length);
		if (new_buffer == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buffer = (char *)new_buffer;

		if (idn_converter != NULL) {
			r = idn_converter_convfromucs4(idn_converter, from,
						       buffer, buffer_length);
		} else {
			r = idn_ucs4_ucs4toutf8(from, buffer, buffer_length);
		}
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buffer_length *= 2;
	}

	to_length = strlen(buffer);
	if (to_length == 0 || to_length > MAX_LABEL_LENGTH) {
		r = idn_invalid_length;
		goto ret;
	}

	r = idn_success;
ret:
	TRACE(("res lencheck(): %s\n", idn_result_tostring(r)));
	if (idn_converter != NULL)
		idn_converter_destroy(idn_converter);
	free(buffer);
	return (r);
}

static idn_result_t
label_rtcheck(idn_resconf_t ctx, idn_action_t actions, labellist_t label,
	    const unsigned long *original_name) {
	labellist_t rt_label = NULL;
	const unsigned long *rt_name;
	const unsigned long *cur_name;
	idn_result_t r;

	cur_name = labellist_getname(label);
	TRACE(("res rtcheck(label=\"%s\", org_label=\"%s\")\n",
		idn__debug_ucs4xstring(cur_name, 50),
		idn__debug_ucs4xstring(original_name, 50)));

	r = labellist_create(cur_name, &rt_label);
	if (r != idn_success)
		goto ret;
	if (rt_label == NULL) {
		if (*original_name == '\0')
			r = idn_success;
		else
			r = idn_invalid_encoding;
		goto ret;
	}

	if (!idn__util_ucs4isasciirange(labellist_getname(rt_label))) {
		r = label_map(ctx, rt_label);
		if (r != idn_success)
			goto ret;
		r = label_normalize(ctx, rt_label);
		if (r != idn_success)
			goto ret;
		r = label_prohcheck(ctx, rt_label);
		if (r != idn_success)
			goto ret;
		if (actions & IDN_UNASCHECK) {
			r = label_unascheck(ctx, rt_label);
			if (r != idn_success)
				goto ret;
		}
		r = label_bidicheck(ctx, rt_label);
		if (r != idn_success)
			goto ret;
	}

	if (actions & IDN_ASCCHECK) {
		r = label_asccheck(ctx, rt_label);
		if (r != idn_success)
			goto ret;
	}
	if (!idn__util_ucs4isasciirange(labellist_getname(rt_label))) {
		r = label_idnencode_ace(ctx, rt_label);
		if (r != idn_success)
			goto ret;
	}
	r = label_lencheck_ace(ctx, rt_label);
	if (r != idn_success)
		goto ret;
	rt_name = labellist_getname(rt_label);

	if (idn_ucs4_strcasecmp(rt_name, original_name) != 0) {
		TRACE(("res rtcheck(): round trip failed, org =\"%s\", rt=\"%s\"\n",
		       idn__debug_ucs4xstring(original_name, 50),
		       idn__debug_ucs4xstring(rt_name, 50)));
		r = idn_invalid_encoding;
		goto ret;
	}

	r  = idn_success;
ret:
	if (r != idn_nomemory && r != idn_success)
		r = idn_invalid_encoding;
	TRACE(("res rtcheck(): %s\n", idn_result_tostring(r)));
	if (rt_label != NULL)
		labellist_destroy(rt_label);
	return (r);
}

const char *
idn__res_actionstostring(idn_action_t actions) {
	static char buf[100];

	buf[0] = '\0';

	if (actions == IDN_ENCODE_QUERY)
		strcpy(buf, "encode-query");
	else if (actions == IDN_DECODE_QUERY)
		strcpy(buf, "decode-query");
	else if (actions == IDN_ENCODE_APP)
		strcpy(buf, "encode-app");
	else if (actions == IDN_DECODE_APP)
		strcpy(buf, "decode-app");
	else if (actions == IDN_ENCODE_STORED)
		strcpy(buf, "encode-stored");
	else if (actions == IDN_DECODE_STORED)
		strcpy(buf, "decode-stored");
	else {
		if (actions & IDN_LOCALCONV)
			strcat(buf, "|localconv");
		if (actions & IDN_DELIMMAP)
			strcat(buf, "|delimmap");
		if (actions & IDN_LOCALMAP)
			strcat(buf, "|localmap");

		if (actions & IDN_MAP)
			strcat(buf, "|map");
		if (actions & IDN_NORMALIZE)
			strcat(buf, "|normalize");
		if (actions & IDN_PROHCHECK)
			strcat(buf, "|prohcheck");
		if (actions & IDN_UNASCHECK)
			strcat(buf, "|unascheck");
		if (actions & IDN_BIDICHECK)
			strcat(buf, "|bidicheck");

		if (actions & IDN_IDNCONV)
			strcat(buf, "|idnconv");
		if (actions & IDN_ASCCHECK)
			strcat(buf, "|asccheck");
		if (actions & IDN_LENCHECK)
			strcat(buf, "|lencheck");
		if (actions & IDN_RTCHECK)
			strcat(buf, "|rtcheck");
	}

	if (buf[0] == '|')
		return (buf + 1);
	else
		return (buf);
}
