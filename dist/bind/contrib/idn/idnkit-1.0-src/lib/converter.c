#ifndef lint
static char *rcsid = "$Id: converter.c,v 1.1.1.1 2003-06-04 00:25:51 marka Exp $";
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifndef WITHOUT_ICONV
#include <iconv.h>
#endif

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/converter.h>
#include <idn/aliaslist.h>
#include <idn/strhash.h>
#include <idn/debug.h>
#include <idn/ucs4.h>
#include <idn/punycode.h>
#include <idn/race.h>
#include <idn/util.h>

#ifndef IDN_UTF8_ENCODING_NAME
#define IDN_UTF8_ENCODING_NAME "UTF-8"		/* by IANA */
#endif
#ifndef IDN_RACE_ENCODING_NAME
#define IDN_RACE_ENCODING_NAME "RACE"
#endif
#ifndef IDN_AMCACEZ_ENCODING_NAME
#define IDN_AMCACEZ_ENCODING_NAME "AMC-ACE-Z"
#endif
#ifndef IDN_PUNYCODE_ENCODING_NAME
#define IDN_PUNYCODE_ENCODING_NAME "Punycode"
#endif

#define MAX_RECURSE	20

#ifdef WIN32

#define IDNKEY_IDNKIT		"Software\\JPNIC\\IDN"
#define IDNVAL_ALIASFILE	"AliasFile"

#else /* WIN32 */

#ifndef IDN_RESCONF_DIR
#define IDN_RESCONF_DIR		"/etc"
#endif
#define IDN_ALIAS_FILE		IDN_RESCONF_DIR "/idnalias.conf"

#endif /* WIN32 */

typedef struct {
	idn_converter_openproc_t openfromucs4;
	idn_converter_openproc_t opentoucs4;
	idn_converter_convfromucs4proc_t convfromucs4;
	idn_converter_convtoucs4proc_t convtoucs4;
	idn_converter_closeproc_t close;
	int encoding_type;
} converter_ops_t;

struct idn_converter {
	char *local_encoding_name;
	converter_ops_t *ops;
	int flags;
	int opened_convfromucs4;
	int opened_convtoucs4;
	int reference_count;
	void *private_data;
};

static idn__strhash_t encoding_name_hash;
static idn__aliaslist_t encoding_alias_list;

static idn_result_t	register_standard_encoding(void);
static idn_result_t	roundtrip_check(idn_converter_t ctx,
					const unsigned long *from,
					const char *to);

static idn_result_t
       converter_none_open(idn_converter_t ctx, void **privdata);
static idn_result_t
       converter_none_close(idn_converter_t ctx, void *privdata);
static idn_result_t
       converter_none_convfromucs4(idn_converter_t ctx,
				   void *privdata,
				   const unsigned long *from,
				   char *to, size_t tolen);
static idn_result_t
       converter_none_convtoucs4(idn_converter_t ctx,
				 void *privdata, const char *from,
				 unsigned long *to, size_t tolen);

#ifndef WITHOUT_ICONV
static idn_result_t
       converter_iconv_openfromucs4(idn_converter_t ctx, void **privdata);
static idn_result_t
       converter_iconv_opentoucs4(idn_converter_t ctx, void **privdata);
static idn_result_t
       converter_iconv_close(idn_converter_t ctx, void *privdata);
static idn_result_t
       converter_iconv_convfromucs4(idn_converter_t ctx,
				    void *privdata,
				    const unsigned long *from,
				    char *to, size_t tolen);
static idn_result_t
       converter_iconv_convtoucs4(idn_converter_t ctx,
				  void *privdata,
				  const char *from,
				  unsigned long *to, size_t tolen);

static idn_result_t
iconv_initialize_privdata(void **privdata);
static void
iconv_finalize_privdata(void *privdata);

static char *		get_system_aliasfile(void);
static int		file_exist(const char *filename);

#endif /* !WITHOUT_ICONV */

#ifdef DEBUG
static idn_result_t
       converter_uescape_convfromucs4(idn_converter_t ctx,
				      void *privdata,
				      const unsigned long *from,
				      char *to, size_t tolen);
static idn_result_t
       converter_uescape_convtoucs4(idn_converter_t ctx,
				    void *privdata,
				    const char *from,
				    unsigned long *to,
				    size_t tolen);
#endif /* DEBUG */

static converter_ops_t none_converter_ops = {
	converter_none_open,
	converter_none_open,
	converter_none_convfromucs4,
	converter_none_convtoucs4,
	converter_none_close,
	IDN_NONACE,
};

#ifndef WITHOUT_ICONV
static converter_ops_t iconv_converter_ops = {
	converter_iconv_openfromucs4,
	converter_iconv_opentoucs4,
	converter_iconv_convfromucs4,
	converter_iconv_convtoucs4,
	converter_iconv_close,
	IDN_NONACE,
};
#endif

/*
 * Initialize.
 */

idn_result_t
idn_converter_initialize(void) {
	idn_result_t r;
	idn__strhash_t hash;
	idn__aliaslist_t list;
#ifndef WITHOUT_ICONV
	const char *fname;
#endif

	TRACE(("idn_converter_initialize()\n"));

	if (encoding_name_hash == NULL) {
		if ((r = idn__strhash_create(&hash)) != idn_success)
			goto ret;
		encoding_name_hash = hash;
		r = register_standard_encoding();
	}
	if (encoding_alias_list == NULL) {
		if ((r = idn__aliaslist_create(&list)) != idn_success)
			goto ret;
		encoding_alias_list = list;
#ifndef WITHOUT_ICONV
		fname = get_system_aliasfile();
		if (fname != NULL && file_exist(fname))
			idn_converter_aliasfile(fname);
#endif
	}

	r = idn_success;
ret:
	TRACE(("idn_converter_initialize(): %s\n", idn_result_tostring(r)));
	return (r);
}

#ifndef WITHOUT_ICONV
static char *
get_system_aliasfile() {
#ifdef WIN32
	static char alias_path[500];	/* a good longer than MAX_PATH */

	if (idn__util_getregistrystring(idn__util_hkey_localmachine,
					IDNVAL_ALIASFILE,
					alias_path, sizeof(alias_path))) {
		return (alias_path);
	} else {
		return (NULL);
	}
#else
	return (IDN_ALIAS_FILE);
#endif
}

static int
file_exist(const char *filename) {
	FILE  *fp;

	if ((fp = fopen(filename, "r")) == NULL)
		return (0);
	fclose(fp);
	return (1);
}
#endif

idn_result_t
idn_converter_create(const char *name, idn_converter_t *ctxp, int flags) {
	const char *realname;
	idn_converter_t ctx;
	idn_result_t r;
	void *v;

	assert(name != NULL && ctxp != NULL);

	TRACE(("idn_converter_create(%s)\n", name));

	realname = idn_converter_getrealname(name);
#ifdef DEBUG
	if (strcmp(name, realname) != 0) {
		TRACE(("idn_converter_create: realname=%s\n", realname));
	}
#endif

	*ctxp = NULL;

	/* Allocate memory for a converter context and the name. */
	ctx = malloc(sizeof(struct idn_converter) + strlen(realname) + 1);
	if (ctx == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	ctx->local_encoding_name = (char *)(ctx + 1);
	(void)strcpy(ctx->local_encoding_name, realname);
	ctx->flags = flags;
	ctx->reference_count = 1;
	ctx->opened_convfromucs4 = 0;
	ctx->opened_convtoucs4 = 0;
	ctx->private_data = NULL;

	assert(encoding_name_hash != NULL);

	if (strcmp(realname, IDN_UTF8_ENCODING_NAME) == 0) {
		/* No conversion needed */
		ctx->ops = &none_converter_ops;
	} else if ((r = idn__strhash_get(encoding_name_hash, realname, &v))
		   == idn_success) {
		/* Special converter found */
		ctx->ops = (converter_ops_t *)v;
	} else {
		/* General case */
#ifdef WITHOUT_ICONV
		free(ctx);
		*ctxp = NULL;
		r = idn_invalid_name;
		goto ret;
#else
		ctx->ops = &iconv_converter_ops;
#endif
	}

	if ((flags & IDN_CONVERTER_DELAYEDOPEN) == 0) {
		r = (ctx->ops->openfromucs4)(ctx, &(ctx->private_data));
		if (r != idn_success) {
			WARNING(("idn_converter_create(): open failed "
			     "(ucs4->local)\n"));
			free(ctx);
			*ctxp = NULL;
			goto ret;
		}
		ctx->opened_convfromucs4 = 1;

		r = (*ctx->ops->opentoucs4)(ctx, &(ctx->private_data));
		if (r != idn_success) {
			WARNING(("idn_converter_create(): open failed "
			     "(local->ucs4)\n"));
			free(ctx);
			*ctxp = NULL;
			goto ret;
		}
		ctx->opened_convtoucs4 = 1;
	}

	*ctxp = ctx;
	r = idn_success;
ret:
	TRACE(("idn_converter_create(): %s\n", idn_result_tostring(r)));
	return (r);
}

void
idn_converter_destroy(idn_converter_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_converter_destroy(ctx=%s)\n", ctx->local_encoding_name));

	ctx->reference_count--;
	if (ctx->reference_count <= 0) {
		TRACE(("idn_converter_destroy(): the object is destroyed\n"));
		(void)(*ctx->ops->close)(ctx, ctx->private_data);
		free(ctx);
	} else {
		TRACE(("idn_converter_destroy(): "
		       "update reference count (%d->%d)\n",
		       ctx->reference_count + 1, ctx->reference_count));
	}
}

void
idn_converter_incrref(idn_converter_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_converter_incrref(ctx=%s)\n", ctx->local_encoding_name));
	TRACE(("idn_converter_incrref: update reference count (%d->%d)\n",
	    ctx->reference_count, ctx->reference_count + 1));

	ctx->reference_count++;
}

char *
idn_converter_localencoding(idn_converter_t ctx) {
	assert(ctx != NULL);
	TRACE(("idn_converter_localencoding(ctx=%s)\n",
	       ctx->local_encoding_name));
	return (ctx->local_encoding_name);
}
	
int
idn_converter_encodingtype(idn_converter_t ctx) {
	int encoding_type;

	assert(ctx != NULL);
	TRACE(("idn_converter_encodingtype(ctx=%s)\n",
	       ctx->local_encoding_name));

	encoding_type = ctx->ops->encoding_type;
	TRACE(("idn_converter_encodingtype(): %d\n", encoding_type));
	return (encoding_type);
}

int
idn_converter_isasciicompatible(idn_converter_t ctx) {
	int iscompat;

	assert(ctx != NULL);
	TRACE(("idn_converter_isasciicompatible(ctx=%s)\n",
	       ctx->local_encoding_name));

	iscompat = (ctx->ops->encoding_type != IDN_NONACE);
	TRACE(("idn_converter_isasciicompatible(): %d\n", iscompat));
	return (iscompat);
}

idn_result_t
idn_converter_convfromucs4(idn_converter_t ctx, const unsigned long *from,
			   char *to, size_t tolen) {
	idn_result_t r;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_converter_convfromucs4(ctx=%s, from=\"%s\", tolen=%d)\n",
	       ctx->local_encoding_name, idn__debug_ucs4xstring(from, 50),
	       (int)tolen));

	if (!ctx->opened_convfromucs4) {
		r = (*ctx->ops->openfromucs4)(ctx, &(ctx->private_data));
		if (r != idn_success)
			goto ret;
		ctx->opened_convfromucs4 = 1;
	}

	r = (*ctx->ops->convfromucs4)(ctx, ctx->private_data, from, to, tolen);
	if (r != idn_success)
		goto ret;
	if ((ctx->flags & IDN_CONVERTER_RTCHECK) != 0) {
		r = roundtrip_check(ctx, from, to);
		if (r != idn_success)
			goto ret;
	}
	
	r = idn_success;
ret:
	if (r == idn_success) {
		TRACE(("idn_converter_convfromucs4(): success (to=\"%s\")\n",
		       idn__debug_xstring(to, 50)));
	} else {
		TRACE(("idn_converter_convfromucs4(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_converter_convtoucs4(idn_converter_t ctx, const char *from,
			 unsigned long *to, size_t tolen) {
	idn_result_t r;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_converter_convtoucs4(ctx=%s, from=\"%s\", tolen=%d)\n",
	       ctx->local_encoding_name, idn__debug_xstring(from, 50),
	       (int)tolen));

	if (!ctx->opened_convtoucs4) {
		r = (*ctx->ops->opentoucs4)(ctx, &(ctx->private_data));
		if (r != idn_success)
			goto ret;
		ctx->opened_convtoucs4 = 1;
	}

	r = (*ctx->ops->convtoucs4)(ctx, ctx->private_data, from, to, tolen);
ret:
	if (r == idn_success) {
		TRACE(("idn_converter_convtoucs4(): success (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to, 50)));
	} else {
		TRACE(("idn_converter_convtoucs4(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}

/*
 * Encoding registration.
 */

idn_result_t
idn_converter_register(const char *name,
		       idn_converter_openproc_t openfromucs4,
		       idn_converter_openproc_t opentoucs4,
		       idn_converter_convfromucs4proc_t convfromucs4,
		       idn_converter_convtoucs4proc_t convtoucs4,
		       idn_converter_closeproc_t close,
		       int encoding_type) {
	converter_ops_t *ops;
	idn_result_t r;

	assert(name != NULL && convfromucs4 != NULL && convtoucs4 != NULL);

	TRACE(("idn_converter_register(name=%s)\n", name));

	if ((ops = malloc(sizeof(*ops))) == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	if (openfromucs4 == NULL)
		openfromucs4 = converter_none_open;
	if (opentoucs4 == NULL)
		opentoucs4 = converter_none_open;
	if (close == NULL)
		close = converter_none_close;

	ops->openfromucs4 = openfromucs4;
	ops->opentoucs4 = opentoucs4;
	ops->convfromucs4 = convfromucs4;
	ops->convtoucs4 = convtoucs4;
	ops->close = close;
	ops->encoding_type = encoding_type;

	r = idn__strhash_put(encoding_name_hash, name, ops);
	if (r != idn_success) {
		free(ops);
		goto ret;
	}

	r = idn_success;
ret:
	TRACE(("idn_converter_register(): %s\n", idn_result_tostring(r)));
	return (r);
}

static idn_result_t
register_standard_encoding(void) {
	idn_result_t r;

	r = idn_converter_register(IDN_PUNYCODE_ENCODING_NAME,
				   NULL,
				   NULL,
				   idn__punycode_encode,
				   idn__punycode_decode,
				   converter_none_close,
				   IDN_ACE_STRICTCASE);
	if (r != idn_success)
		return (r);

#ifdef IDN_EXTRA_ACE
	r = idn_converter_register(IDN_AMCACEZ_ENCODING_NAME,
				   NULL,
				   NULL,
				   idn__punycode_encode,
				   idn__punycode_decode,
				   converter_none_close,
				   IDN_ACE_STRICTCASE);
	if (r != idn_success)
		return (r);

	r = idn_converter_register(IDN_RACE_ENCODING_NAME,
				   NULL,
				   NULL,
				   idn__race_encode,
				   idn__race_decode,
				   converter_none_close,
				   IDN_ACE_LOOSECASE);
	if (r != idn_success)
		return (r);
#endif /* IDN_EXTRA_ACE */

#ifdef DEBUG
	/* This is convenient for debug.  Not useful for other purposes. */
	r = idn_converter_register("U-escape",
				   NULL,
				   NULL,
				   converter_uescape_convfromucs4,
				   converter_uescape_convtoucs4,
				   NULL,
				   IDN_NONACE);
	if (r != idn_success)
		return (r);
#endif /* DEBUG */

	return (r);
}

/*
 * Encoding alias support.
 */
idn_result_t
idn_converter_addalias(const char *alias_name, const char *real_name,
		       int first_item) {
	idn_result_t r;

	assert(alias_name != NULL && real_name != NULL);

	TRACE(("idn_converter_addalias(alias_name=%s,real_name=%s)\n",
	       alias_name, real_name));

	if (strlen(alias_name) == 0 || strlen(real_name) == 0) {
		return idn_invalid_syntax;
	}

	if (strcmp(alias_name, real_name) == 0) {
		r = idn_success;
		goto ret;
	}

	if (encoding_alias_list == NULL) {
		WARNING(("idn_converter_addalias(): the module is not "
			 "initialized\n"));
		r = idn_failure;
		goto ret;
	}

	r = idn__aliaslist_additem(encoding_alias_list, alias_name, real_name,
				   first_item);
ret:
	TRACE(("idn_converter_addalias(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_converter_aliasfile(const char *path) {
	idn_result_t r;

	assert(path != NULL);

	TRACE(("idn_converter_aliasfile(path=%s)\n", path));

	if (encoding_alias_list == NULL) {
		WARNING(("idn_converter_aliasfile(): the module is not "
			 "initialized\n"));
		return (idn_failure);
	}

	r = idn__aliaslist_aliasfile(encoding_alias_list, path);

	TRACE(("idn_converter_aliasfile(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_converter_resetalias(void) {
	idn__aliaslist_t list;
	idn_result_t r;

 	TRACE(("idn_converter_resetalias()\n"));
 
	if (encoding_alias_list == NULL) {
		WARNING(("idn_converter_resetalias(): the module is not "
			 "initialized\n"));
		return (idn_failure);
	}

	list = encoding_alias_list;
	encoding_alias_list = NULL;
	idn__aliaslist_destroy(list);
	list = NULL;
	r = idn__aliaslist_create(&list);
	encoding_alias_list = list;

	TRACE(("idn_converter_resetalias(): %s\n", idn_result_tostring(r)));
	return (r);
}

const char *
idn_converter_getrealname(const char *name) {
	char *realname;
	idn_result_t r;

 	TRACE(("idn_converter_getrealname()\n"));

	assert(name != NULL);

	if (encoding_alias_list == NULL) {
		WARNING(("idn_converter_getrealname(): the module is not "
			 "initialized\n"));
		return (name);
	}

	r = idn__aliaslist_find(encoding_alias_list, name, &realname);
	if (r != idn_success) {
		return (name);
	}
	return (realname);
}

/*
 * Round trip check.
 */

static idn_result_t
roundtrip_check(idn_converter_t ctx, const unsigned long *from, const char *to)
{
	/*
	 * One problem with iconv() convertion is that
	 * iconv() doesn't signal an error if the input
	 * string contains characters which are valid but
	 * do not have mapping to the output codeset.
	 * (the behavior of iconv() for that case is defined as
	 * `implementation dependent')
	 * One way to check this case is to perform round-trip
	 * conversion and see if it is same as the original string.
	 */
	idn_result_t r;
	unsigned long *back;
	unsigned long backbuf[256];
	size_t fromlen;
	size_t backlen;

	TRACE(("idn_converter_convert: round-trip checking (from=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50)));

	/* Allocate enough buffer. */
	fromlen = idn_ucs4_strlen(from) + 1;
	if (fromlen * sizeof(*back) <= sizeof(backbuf)) {
		backlen = sizeof(backbuf);
		back = backbuf;
	} else {
		backlen = fromlen;
		back = (unsigned long *)malloc(backlen * sizeof(*back));
		if (back == NULL)
			return (idn_nomemory);
	}

	/*
	 * Perform backward conversion.
	 */
	r = idn_converter_convtoucs4(ctx, to, back, backlen);
	switch (r) {
	case idn_success:
		if (memcmp(back, from, sizeof(*from) * fromlen) != 0)
			r = idn_nomapping;
		break;
	case idn_invalid_encoding:
	case idn_buffer_overflow:
		r = idn_nomapping;
		break;
	default:
		break;
	}

	if (back != backbuf)
		free(back);

	if (r != idn_success) {
		TRACE(("round-trip check failed: %s\n",
		       idn_result_tostring(r)));
	}

	return (r);
}

/*
 * Identity conversion (or, no conversion at all).
 */

static idn_result_t
converter_none_open(idn_converter_t ctx, void **privdata) {
	assert(ctx != NULL);

	return (idn_success);
}

static idn_result_t
converter_none_close(idn_converter_t ctx, void *privdata) {
	assert(ctx != NULL);

	return (idn_success);
}

static idn_result_t
converter_none_convfromucs4(idn_converter_t ctx, void *privdata,
		       const unsigned long *from, char *to, size_t tolen) {
	assert(ctx != NULL && from != NULL && to != NULL);

	return idn_ucs4_ucs4toutf8(from, to, tolen);
}

static idn_result_t
converter_none_convtoucs4(idn_converter_t ctx, void *privdata,
		     const char *from, unsigned long *to, size_t tolen) {
	assert(ctx != NULL && from != NULL && to != NULL);

	return idn_ucs4_utf8toucs4(from, to, tolen);
}

#ifndef WITHOUT_ICONV

/*
 * Conversion using iconv() interface.
 */

static idn_result_t
converter_iconv_openfromucs4(idn_converter_t ctx, void **privdata) {
	iconv_t *ictxp;
	idn_result_t r;

	assert(ctx != NULL);

	r = iconv_initialize_privdata(privdata);
	if (r != idn_success)
		return (r);

	ictxp = (iconv_t *)*privdata;
	*ictxp = iconv_open(ctx->local_encoding_name, IDN_UTF8_ENCODING_NAME);
	if (*ictxp == (iconv_t)(-1)) {
		free(*privdata);
		*privdata = NULL;
		switch (errno) {
		case ENOMEM:
			return (idn_nomemory);
		case EINVAL:
			return (idn_invalid_name);
		default:
			WARNING(("iconv_open failed with errno %d\n", errno));
			return (idn_failure);
		}
	}

	return (idn_success);
}

static idn_result_t
converter_iconv_opentoucs4(idn_converter_t ctx, void **privdata) {
	iconv_t *ictxp;
	idn_result_t r;

	assert(ctx != NULL);

	r = iconv_initialize_privdata(privdata);
	if (r != idn_success)
		return (r);

	ictxp = (iconv_t *)*privdata + 1;
	*ictxp = iconv_open(IDN_UTF8_ENCODING_NAME, ctx->local_encoding_name);
	if (*ictxp == (iconv_t)(-1)) {
		free(*privdata);
		*privdata = NULL;
		switch (errno) {
		case ENOMEM:
			return (idn_nomemory);
		case EINVAL:
			return (idn_invalid_name);
		default:
			WARNING(("iconv_open failed with errno %d\n", errno));
			return (idn_failure);
		}
	}

	return (idn_success);
}

static idn_result_t
iconv_initialize_privdata(void **privdata) {
	if (*privdata == NULL) {
		*privdata = malloc(sizeof(iconv_t) * 2);
		if (*privdata == NULL)
			return (idn_nomemory);
		*((iconv_t *)*privdata) = (iconv_t)(-1);
		*((iconv_t *)*privdata + 1) = (iconv_t)(-1);
	}

	return (idn_success);
}

static void
iconv_finalize_privdata(void *privdata) {
	iconv_t *ictxp;
	
	if (privdata != NULL) {
		ictxp = (iconv_t *)privdata;
		if (*ictxp != (iconv_t)(-1))
			iconv_close(*ictxp);

		ictxp++;
		if (*ictxp != (iconv_t)(-1))
			iconv_close(*ictxp);
		free(privdata);
	}
}

static idn_result_t
converter_iconv_close(idn_converter_t ctx, void *privdata) {
	assert(ctx != NULL);

	iconv_finalize_privdata(privdata);

	return (idn_success);
}

static idn_result_t
converter_iconv_convfromucs4(idn_converter_t ctx, void *privdata,
			     const unsigned long *from, char *to,
			     size_t tolen) {
	iconv_t ictx;
	char *utf8 = NULL;
	size_t utf8size = 256;  /* large enough */
	idn_result_t r;
	size_t sz;
	size_t inleft;
	size_t outleft;
	char *inbuf, *outbuf;

	assert(ctx != NULL && from != NULL && to != NULL);

	if (tolen <= 0) {
		r = idn_buffer_overflow;	/* need space for NUL */
		goto ret;
	}

	/*
	 * UCS4 -> UTF-8 conversion.
	 */
	utf8 = (char *)malloc(utf8size);
	if (utf8 == NULL) {
		r = idn_nomemory;
		goto ret;
	}

try_again:
	r = idn_ucs4_ucs4toutf8(from, utf8, utf8size);
	if (r == idn_buffer_overflow) {
		char *new_utf8;

		utf8size *= 2;
		new_utf8 = (char *)realloc(utf8, utf8size);
		if (new_utf8 == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		utf8 = new_utf8;
		goto try_again;
	} else if (r != idn_success) {
		goto ret;
	}

	ictx = ((iconv_t *)privdata)[0];

	/*
	 * Reset internal state.
	 * 
	 * The following code should work according to the SUSv2 spec,
	 * but causes segmentation fault with Solaris 2.6.
	 * So.. a work-around.
	 * 
	 * (void)iconv(ictx, (const char **)NULL, (size_t *)NULL, 
	 * 	    (char **)NULL, (size_t *)NULL);
	 */
	inleft = 0;
	outbuf = NULL;
	outleft = 0;
	(void)iconv(ictx, (const char **)NULL, &inleft, &outbuf, &outleft);

	inleft = strlen(utf8);
	inbuf = utf8;
	outleft = tolen - 1;	/* reserve space for terminating NUL */
	sz = iconv(ictx, (const char **)&inbuf, &inleft, &to, &outleft);

	if (sz == (size_t)(-1) || inleft > 0) {
		switch (errno) {
		case EILSEQ:
		case EINVAL:
			/*
			 * We already checked the validity of the input
			 * string.  So we assume a mapping error.
			 */
			r = idn_nomapping;
			goto ret;
		case E2BIG:
			r = idn_buffer_overflow;
			goto ret;
		default:
			WARNING(("iconv failed with errno %d\n", errno));
			r = idn_failure;
			goto ret;
		}
	}

	/*
	 * For UTF-8 -> local conversion, append a sequence of
	 * state reset.
	 */
	inleft = 0;
	sz = iconv(ictx, (const char **)NULL, &inleft, &to, &outleft);
	if (sz == (size_t)(-1)) {
		switch (errno) {
		case EILSEQ:
		case EINVAL:
			r = idn_invalid_encoding;
			goto ret;
		case E2BIG:
			r = idn_buffer_overflow;
			goto ret;
		default:
			WARNING(("iconv failed with errno %d\n", errno));
			r = idn_failure;
			goto ret;
		}
	}
	*to = '\0';
	r = idn_success;

ret:
	free(utf8);
	return (r);

}

static idn_result_t
converter_iconv_convtoucs4(idn_converter_t ctx, void *privdata,
			   const char *from, unsigned long *to, size_t tolen) {
	iconv_t ictx;
	char *utf8 = NULL;
	size_t utf8size = 256;  /* large enough */
	idn_result_t r;
	size_t sz;
	size_t inleft;
	size_t outleft;
	const char *from_ptr;
	char *outbuf;

	assert(ctx != NULL && from != NULL && to != NULL);

	if (tolen <= 0) {
		r = idn_buffer_overflow;	/* need space for NUL */
		goto ret;
	}
	ictx = ((iconv_t *)privdata)[1];
	utf8 = (char *)malloc(utf8size);
	if (utf8 == NULL) {
		r = idn_nomemory;
		goto ret;
	}

try_again:
	/*
	 * Reset internal state.
	 */
	inleft = 0;
	outbuf = NULL;
	outleft = 0;
	(void)iconv(ictx, (const char **)NULL, &inleft, &outbuf, &outleft);

	from_ptr = from;
	inleft = strlen(from);
	outbuf = utf8;
	outleft = utf8size - 1;    /* reserve space for terminating NUL */
	sz = iconv(ictx, (const char **)&from_ptr, &inleft, &outbuf, &outleft);

	if (sz == (size_t)(-1) || inleft > 0) {
		char *new_utf8;

		switch (errno) {
		case EILSEQ:
		case EINVAL:
			/*
			 * We assume all the characters in the local
			 * codeset are included in UCS.  This means mapping
			 * error is not possible, so the input string must
			 * have some problem.
			 */
			r = idn_invalid_encoding;
			goto ret;
		case E2BIG:
			utf8size *= 2;
			new_utf8 = (char *)realloc(utf8, utf8size);
			if (new_utf8 == NULL) {
				r = idn_nomemory;
				goto ret;
			}
			utf8 = new_utf8;
			goto try_again;
		default:
			WARNING(("iconv failed with errno %d\n", errno));
			r = idn_failure;
			goto ret;
		}
	}
	*outbuf = '\0';

	/*
	 * UTF-8 -> UCS4 conversion.
	 */
	r = idn_ucs4_utf8toucs4(utf8, to, tolen);

ret:
	free(utf8);
	return (r);
}

#endif /* !WITHOUT_ICONV */

#ifdef DEBUG
/*
 * Conversion to/from unicode escape string.
 * Arbitrary UCS-4 character can be specified by a special sequence
 *	\u{XXXXXX}
 * where XXXXX denotes any hexadecimal string up to FFFFFFFF.
 * This is designed for debugging.
 */

static idn_result_t
converter_uescape_convfromucs4(idn_converter_t ctx, void *privdata,
			  const unsigned long *from, char *to,
			  size_t tolen) {
	idn_result_t r;
	unsigned long v;

	while (*from != '\0') {
		v = *from++;

		if (v <= 0x7f) {
			if (tolen < 1) {
				r = idn_buffer_overflow;
				goto failure;
			}
			*to++ = v;
			tolen--;
		} else if (v <= 0xffffffff) {
			char tmp[20];
			int len;

			(void)sprintf(tmp, "\\u{%lx}", v);
			len = strlen(tmp);
			if (tolen < len) {
				r = idn_buffer_overflow;
				goto failure;
			}
			(void)memcpy(to, tmp, len);
			to += len;
			tolen -= len;
		} else {
			r = idn_invalid_encoding;
			goto failure;
		}
	}

	if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto failure;
	}
	*to = '\0';

	return (idn_success);

failure:
	if (r != idn_buffer_overflow) {
		WARNING(("idn_uescape_convfromucs4(): %s\n",
			 idn_result_tostring(r)));
	}
	return (r);
}

static idn_result_t
converter_uescape_convtoucs4(idn_converter_t ctx, void *privdata,
			const char *from, unsigned long *to, size_t tolen)
{
	idn_result_t r;
	size_t fromlen = strlen(from);

	while (*from != '\0') {
		if (tolen <= 0) {
			r = idn_buffer_overflow;
			goto failure;
		}
		if (strncmp(from, "\\u{", 3) == 0 ||
		    strncmp(from, "\\U{", 3) == 0) {
			size_t ullen;
			unsigned long v;
			char *end;

			v = strtoul(from + 3, &end, 16);
			ullen = end - (from + 3);
			if (*end == '}' && ullen > 1 && ullen < 8) {
				*to = v;
				from = end + 1;
				fromlen -= ullen;
			} else {
				*to = '\\';
				from++;
				fromlen--;
			}
		} else {
			int c = *(unsigned char *)from;
			size_t width;
			char buf[8];

			if (c < 0x80)
				width = 1;
			else if (c < 0xc0)
				width = 0;
			else if (c < 0xe0)
				width = 2;
			else if (c < 0xf0)
				width = 3;
			else if (c < 0xf8)
				width = 4;
			else if (c < 0xfc)
				width = 5;
			else if (c < 0xfe)
				width = 6;
			else
				width = 0;
			if (width == 0 || width > fromlen) {
				r = idn_invalid_encoding;
				goto failure;
			}

			memcpy(buf, from, width);
			buf[width] = '\0';
			r = idn_ucs4_utf8toucs4(buf, to, tolen);
			if (r != idn_success) {
				r = idn_invalid_encoding;
				goto failure;
			}
			from += width;
			fromlen -= width;
		}
		to++;
		tolen--;
	}

	if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto failure;
	}
	*to = '\0';

	return (idn_success);

failure:
	if (r != idn_buffer_overflow) {
		WARNING(("idn_uescape_convtoucs4(): %s\n",
			 idn_result_tostring(r)));
	}
	return (r);
}

#endif
