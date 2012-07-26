#ifndef lint
static char *rcsid = "$Id: resconf.c,v 1.1.1.1 2003-06-04 00:26:12 marka Exp $";
#endif

/*
 * Copyright (c) 2000 Japan Network Information Center.  All rights reserved.
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/converter.h>
#include <idn/nameprep.h>
#include <idn/normalizer.h>
#include <idn/checker.h>
#include <idn/mapper.h>
#include <idn/mapselector.h>
#include <idn/delimitermap.h>
#include <idn/localencoding.h>
#include <idn/resconf.h>
#include <idn/debug.h>
#include <idn/util.h>

#ifdef WIN32
#define MAX_PATH_SIZE		500	/* a good longer than MAX_PATH */
#define IDNVAL_CONFFILE		"ConfFile"
#else /* WIN32 */

#ifndef IDN_RESCONF_DIR
#define IDN_RESCONF_DIR		"/etc"
#endif
#define IDN_RESCONF_FILE	IDN_RESCONF_DIR "/idn.conf"
#define IDN_USER_RESCONF_FILE	"/.idnrc"

#endif /* WIN32 */

#define MAX_CONF_LINE_LENGTH	255
#define MAX_CONF_LINE_ARGS	63

#define DEFAULT_CONF_NAMEPREP		0x0001
#define DEFAULT_CONF_IDN_ENCODING	0x0010
#define DEFAULT_CONF_ALL		(DEFAULT_CONF_NAMEPREP | \
					DEFAULT_CONF_IDN_ENCODING)

#define IDN_ENCODING_CURRENT	"Punycode"

#ifdef ENABLE_MDNKIT_COMPAT
#define MDN_RESCONF_FILE	IDN_RESCONF_DIR "/mdn.conf"
#endif

struct idn_resconf {
	int local_converter_is_static;
	idn_converter_t local_converter;
	idn_converter_t idn_converter;
        idn_converter_t aux_idn_converter;
	idn_normalizer_t normalizer;
	idn_checker_t prohibit_checker;
	idn_checker_t unassigned_checker;
	idn_checker_t bidi_checker;
	idn_mapper_t mapper;
	idn_mapselector_t local_mapper;
	idn_delimitermap_t delimiter_mapper;
	int reference_count;
};

static int initialized;

#ifndef WIN32
static const char *	userhomedir(void);
#endif
static idn_result_t	open_userdefaultfile(FILE **fpp);
static idn_result_t	open_defaultfile(FILE **fpp);
static idn_result_t	parse_conf(idn_resconf_t ctx, FILE *fp);
static idn_result_t	parse_idn_encoding(idn_resconf_t ctx, char *args,
					   int lineno);
static idn_result_t	parse_local_map(idn_resconf_t ctx, char *args,
					int lineno);
static idn_result_t	parse_nameprep(idn_resconf_t ctx, char *args,
				       int lineno);
static int		split_args(char *s, char **av, int max_ac);
static void		resetconf(idn_resconf_t ctx);
#ifndef WITHOUT_ICONV
static idn_result_t	update_local_converter(idn_resconf_t ctx);
#endif
static idn_result_t	setdefaults_body(idn_resconf_t ctx, int conf_mask);

idn_result_t
idn_resconf_initialize(void) {
	idn_result_t r;

	TRACE(("idn_resconf_initialize()\n"));

	if (initialized) {
		r = idn_success;
		goto ret;
	}

	/*
	 * Initialize sub modules.
	 */
	if ((r = idn_converter_initialize()) != idn_success)
		goto ret;
	if ((r = idn_normalizer_initialize()) != idn_success)
		goto ret;
	if ((r = idn_checker_initialize()) != idn_success)
		goto ret;
	if ((r = idn_mapselector_initialize()) != idn_success)
		goto ret;
	if ((r = idn_mapper_initialize()) != idn_success)
		goto ret;

	r = idn_success;
	initialized = 1;
ret:
	TRACE(("idn_resconf_initialize(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_resconf_create(idn_resconf_t *ctxp) {
	idn_resconf_t ctx = NULL;
	idn_result_t r;

	assert(ctxp != NULL);

	TRACE(("idn_resconf_create()\n"));

	if (!initialized) {
		r = idn_failure;
		goto ret;
	}
	if ((ctx = malloc(sizeof(*ctx))) == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	ctx->local_converter_is_static = 0;
	ctx->local_converter = NULL;
	ctx->idn_converter = NULL;
	ctx->aux_idn_converter = NULL;
	ctx->normalizer = NULL;
	ctx->prohibit_checker = NULL;
	ctx->unassigned_checker = NULL;
	ctx->bidi_checker = NULL;
	ctx->mapper = NULL;
	ctx->local_mapper = NULL;
	ctx->reference_count = 1;

	r = idn_delimitermap_create(&ctx->delimiter_mapper);
	if (r != idn_success)
		goto ret;

	*ctxp = ctx;
	r = idn_success;
ret:
	TRACE(("idn_resconf_create(): %s\n", idn_result_tostring(r)));
	return (r);
}

char *
idn_resconf_defaultfile() {
#ifdef WIN32
	static char default_path[MAX_PATH_SIZE];

	if (idn__util_getregistrystring(idn__util_hkey_localmachine,
					IDNVAL_CONFFILE, default_path,
					sizeof(default_path))) {
		return (default_path);
	} else {
		return (NULL);
	}
#else
	return (IDN_RESCONF_FILE);
#endif
}

#ifndef WIN32
static const char *
userhomedir() {
	uid_t uid;
	struct passwd *pwd;

	uid = getuid();
	pwd = getpwuid(uid);
	if (pwd == NULL) {
		return (NULL);
	}

	return (pwd->pw_dir);
}
#endif

static idn_result_t
open_userdefaultfile(FILE **fpp) {
#ifdef WIN32
	char user_path[MAX_PATH_SIZE];

	TRACE(("open_userdefaultfile()\n"));

	if (idn__util_getregistrystring(idn__util_hkey_currentuser,
					IDNVAL_CONFFILE, user_path,
					sizeof(user_path)) == 0) {
		return (idn_nofile);
	}
	*fpp = fopen(user_path, "r");
	if (*fpp == NULL) {
		return (idn_nofile);
	}
	return (idn_success);
#else /* WIN32 */
	const char *homedir;
	char *file;
	int len;

	TRACE(("open_userdefaultfile()\n"));

	homedir = userhomedir();
	len = strlen(IDN_USER_RESCONF_FILE) + 1;
	if (homedir != NULL) {
		len += strlen(homedir);
	} else {
		return (idn_notfound);
	}

	file = (char *)malloc(sizeof(char) * len);
	if (file == NULL) {
		WARNING(("open_userdefaultfile(): malloc failed\n"));
		return (idn_nomemory);
	}

	(void)strcpy(file, homedir);
	strcat(file, IDN_USER_RESCONF_FILE);
	
	*fpp = fopen(file, "r");
	free(file);
	if (*fpp == NULL) {
		return (idn_nofile);
	}

	return (idn_success);
#endif /* WIN32 */
}

static idn_result_t
open_defaultfile(FILE **fpp) {
	idn_result_t r;
	const char *file;

	r = open_userdefaultfile(fpp);
	if (r == idn_nofile || r == idn_notfound) {
		TRACE(("open_defaultfile: "
		       "cannot open user configuration file\n"));
		file = idn_resconf_defaultfile();
		*fpp = fopen(file, "r");
#ifdef ENABLE_MDNKIT_COMPAT
		if (*fpp == NULL)
			*fpp = fopen(MDN_RESCONF_FILE, "r");
#endif
		if (*fpp == NULL) {
			TRACE(("open_defaultfile: "
			       "cannot open system configuration file\n"));
			return (idn_nofile);
		}
	} else if (r != idn_success) {
		return (r);
	}

	return (idn_success);
}

idn_result_t
idn_resconf_loadfile(idn_resconf_t ctx, const char *file) {
	FILE *fp = NULL;
	idn_result_t r;

	assert(ctx != NULL);

	TRACE(("idn_resconf_loadfile(file=%s)\n",
	      file == NULL ? "<null>" : file));

	resetconf(ctx);
	r = idn_delimitermap_create(&ctx->delimiter_mapper);
	if (r != idn_success) {
		goto ret;
	}

	if (file == NULL) {
		r = open_defaultfile(&fp);
		if (r == idn_nofile || r == idn_notfound) {
			r = setdefaults_body(ctx, 0);
			goto ret;
		} else if (r != idn_success) {
			goto ret;
		}
	} else {
		fp = fopen(file, "r");
		if (fp == NULL) {
			TRACE(("idn_resconf_loadfile: cannot open %-.40s\n",
			       file));
			r = idn_nofile;
			goto ret;
		}
	}

	r = parse_conf(ctx, fp);
	fclose(fp);

ret:
	TRACE(("idn_resconf_loadfile(): %s\n", idn_result_tostring(r)));
	return (r);
}

void
idn_resconf_destroy(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_destroy()\n"));

	ctx->reference_count--;
	if (ctx->reference_count <= 0) {
		resetconf(ctx);
		free(ctx);
		TRACE(("idn_resconf_destroy: the object is destroyed\n"));
	} else {
		TRACE(("idn_resconf_destroy(): "
		       "update reference count (%d->%d)\n",
		       ctx->reference_count + 1, ctx->reference_count));
	}
}

void
idn_resconf_incrref(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_incrref()\n"));
	TRACE(("idn_resconf_incrref: update reference count (%d->%d)\n",
		ctx->reference_count, ctx->reference_count + 1));

	ctx->reference_count++;
}

idn_converter_t
idn_resconf_getalternateconverter(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getalternateconverter()\n"));

	return (idn_resconf_getidnconverter(ctx));
}

idn_delimitermap_t
idn_resconf_getdelimitermap(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getdelimitermap()\n"));

	if (ctx->delimiter_mapper != NULL)
		idn_delimitermap_incrref(ctx->delimiter_mapper);
	return (ctx->delimiter_mapper);
}

idn_converter_t
idn_resconf_getidnconverter(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getidnconverter()\n"));

	if (ctx->idn_converter != NULL)
		idn_converter_incrref(ctx->idn_converter);
	return (ctx->idn_converter);
}

idn_converter_t
idn_resconf_getauxidnconverter(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getauxidnconverter()\n"));

	if (ctx->aux_idn_converter != NULL)
		idn_converter_incrref(ctx->aux_idn_converter);
	return (ctx->aux_idn_converter);
}

idn_converter_t
idn_resconf_getlocalconverter(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getlocalconverter()\n"));

#ifdef WITHOUT_ICONV
	return NULL;

#else /* WITHOUT_ICONV */
	if (update_local_converter(ctx) != idn_success)
		return (NULL);

	idn_converter_incrref(ctx->local_converter);
	return (ctx->local_converter);

#endif /* WITHOUT_ICONV */
}

idn_mapselector_t
idn_resconf_getlocalmapselector(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getlocalmapselector()\n"));

	if (ctx->local_mapper != NULL)
		idn_mapselector_incrref(ctx->local_mapper);
	return (ctx->local_mapper);
}

idn_mapper_t
idn_resconf_getmapper(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getmapper()\n"));

	if (ctx->mapper != NULL)
		idn_mapper_incrref(ctx->mapper);
	return (ctx->mapper);
}

idn_normalizer_t
idn_resconf_getnormalizer(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getnormalizer()\n"));

	if (ctx->normalizer != NULL)
		idn_normalizer_incrref(ctx->normalizer);
	return (ctx->normalizer);
}

idn_checker_t
idn_resconf_getprohibitchecker(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getprohibitchecker()\n"));

	if (ctx->prohibit_checker != NULL)
		idn_checker_incrref(ctx->prohibit_checker);
	return (ctx->prohibit_checker);
}

idn_checker_t
idn_resconf_getunassignedchecker(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getunassignedchecker()\n"));

	if (ctx->unassigned_checker != NULL)
		idn_checker_incrref(ctx->unassigned_checker);
	return (ctx->unassigned_checker);
}

idn_checker_t
idn_resconf_getbidichecker(idn_resconf_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_getbidichecker()\n"));

	if (ctx->bidi_checker != NULL)
		idn_checker_incrref(ctx->bidi_checker);
	return (ctx->bidi_checker);
}

void
idn_resconf_setalternateconverter(idn_resconf_t ctx,
				  idn_converter_t alternate_converter) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setalternateconverter()\n"));
}

void
idn_resconf_setdelimitermap(idn_resconf_t ctx,
			    idn_delimitermap_t delimiter_mapper) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setdelimitermap()\n"));

	if (ctx->delimiter_mapper != NULL)
		idn_delimitermap_destroy(ctx->delimiter_mapper);
	ctx->delimiter_mapper = delimiter_mapper;
	if (delimiter_mapper != NULL)
		idn_delimitermap_incrref(ctx->delimiter_mapper);
}

void
idn_resconf_setidnconverter(idn_resconf_t ctx, 
			    idn_converter_t idn_converter) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setidnconverter()\n"));

	if (ctx->idn_converter != NULL)
		idn_converter_destroy(ctx->idn_converter);
	ctx->idn_converter = idn_converter;
	if (idn_converter != NULL)
		idn_converter_incrref(ctx->idn_converter);
}

void
idn_resconf_setauxidnconverter(idn_resconf_t ctx,
				idn_converter_t aux_idn_converter) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setauxidnconverter()\n"));

	if (ctx->aux_idn_converter != NULL)
		idn_converter_destroy(ctx->aux_idn_converter);
	ctx->aux_idn_converter = aux_idn_converter;
	if (aux_idn_converter != NULL)
		idn_converter_incrref(ctx->aux_idn_converter);
}

void
idn_resconf_setlocalconverter(idn_resconf_t ctx,
			      idn_converter_t local_converter) {
#ifndef WITHOUT_ICONV
	assert(ctx != NULL);

	TRACE(("idn_resconf_setlocalconverter()\n"));

	if (ctx->local_converter != NULL) {
		idn_converter_destroy(ctx->local_converter);
		ctx->local_converter = NULL;
	}

	if (local_converter == NULL)
		ctx->local_converter_is_static = 0;
	else {
		ctx->local_converter = local_converter;
		idn_converter_incrref(local_converter);
		ctx->local_converter_is_static = 1;
	}
#endif /* WITHOUT_ICONV */
}

void
idn_resconf_setlocalmapselector(idn_resconf_t ctx,
				idn_mapselector_t local_mapper) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setlocalmapselector()\n"));

	if (ctx->local_mapper != NULL)
		idn_mapselector_destroy(ctx->local_mapper);
	ctx->local_mapper = local_mapper;
	if (local_mapper != NULL)
		idn_mapselector_incrref(ctx->local_mapper);
}

void
idn_resconf_setmapper(idn_resconf_t ctx, idn_mapper_t mapper) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setmapper()\n"));

	if (ctx->mapper != NULL)
		idn_mapper_destroy(ctx->mapper);
	ctx->mapper = mapper;
	if (mapper != NULL)
		idn_mapper_incrref(ctx->mapper);
}

void
idn_resconf_setnormalizer(idn_resconf_t ctx, idn_normalizer_t normalizer) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setnormalizer()\n"));

	if (ctx->normalizer != NULL)
		idn_normalizer_destroy(ctx->normalizer);
	ctx->normalizer = normalizer;
	if (normalizer != NULL)
		idn_normalizer_incrref(ctx->normalizer);
}

void
idn_resconf_setprohibitchecker(idn_resconf_t ctx,
			       idn_checker_t prohibit_checker) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setprohibitchecker()\n"));

	if (ctx->prohibit_checker != NULL)
		idn_checker_destroy(ctx->prohibit_checker);
	ctx->prohibit_checker = prohibit_checker;
	if (prohibit_checker != NULL)
		idn_checker_incrref(ctx->prohibit_checker);
}

void
idn_resconf_setunassignedchecker(idn_resconf_t ctx,
				 idn_checker_t unassigned_checker) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setunassignedchecker()\n"));

	if (ctx->unassigned_checker != NULL)
		idn_checker_destroy(ctx->unassigned_checker);
	ctx->unassigned_checker = unassigned_checker;
	if (unassigned_checker != NULL)
		idn_checker_incrref(ctx->unassigned_checker);
}

void
idn_resconf_setbidichecker(idn_resconf_t ctx,
			   idn_checker_t bidi_checker) {
	assert(ctx != NULL);

	TRACE(("idn_resconf_setbidichecker()\n"));

	if (ctx->bidi_checker != NULL)
		idn_checker_destroy(ctx->bidi_checker);
	ctx->bidi_checker = bidi_checker;
	if (bidi_checker != NULL)
		idn_checker_incrref(ctx->bidi_checker);
}

idn_result_t
idn_resconf_setnameprepversion(idn_resconf_t ctx, const char *version)
{
	char prohibit_scheme_name[MAX_CONF_LINE_LENGTH + 1];
	char unassigned_scheme_name[MAX_CONF_LINE_LENGTH + 1];
	char bidi_scheme_name[MAX_CONF_LINE_LENGTH + 1];
	idn_mapper_t mapper = NULL;
	idn_normalizer_t normalizer = NULL;
	idn_checker_t prohibit_checker = NULL;
	idn_checker_t unassigned_checker = NULL;
	idn_checker_t bidi_checker = NULL;
	idn_result_t r;

	assert(ctx != NULL && version != NULL);

	TRACE(("idn_resconf_setnameprepversion()\n"));

	/*
	 * Set canonical scheme names.
	 */
	if (strlen(version) + strlen(IDN_CHECKER_PROHIBIT_PREFIX)
	    > MAX_CONF_LINE_LENGTH) {
		r = idn_invalid_name;
		goto failure;
	}
	sprintf(prohibit_scheme_name, "%s%s",
	        IDN_CHECKER_PROHIBIT_PREFIX, version);

	if (strlen(version) + strlen(IDN_CHECKER_UNASSIGNED_PREFIX)
	    > MAX_CONF_LINE_LENGTH) {
		r = idn_invalid_name;
		goto failure;
	}
	sprintf(unassigned_scheme_name, "%s%s",
	        IDN_CHECKER_UNASSIGNED_PREFIX, version);

	if (strlen(version) + strlen(IDN_CHECKER_BIDI_PREFIX)
	    > MAX_CONF_LINE_LENGTH) {
		r = idn_invalid_name;
		goto failure;
	}
	sprintf(bidi_scheme_name, "%s%s",
	        IDN_CHECKER_BIDI_PREFIX, version);

	/*
	 * Create objects.
	 */
	r = idn_mapper_create(&mapper);
	if (r != idn_success)
		goto failure;
	r = idn_normalizer_create(&normalizer);
	if (r != idn_success)
		goto failure;
	r = idn_checker_create(&prohibit_checker);
	if (r != idn_success)
		goto failure;
	r = idn_checker_create(&unassigned_checker);
	if (r != idn_success)
		goto failure;
	r = idn_checker_create(&bidi_checker);
	if (r != idn_success)
		goto failure;

	r = idn_mapper_add(mapper, version);
	if (r != idn_success)
		goto failure;
	r = idn_normalizer_add(normalizer, version);
	if (r != idn_success)
		goto failure;
	r = idn_checker_add(prohibit_checker, prohibit_scheme_name);
	if (r != idn_success)
		goto failure;
	r = idn_checker_add(unassigned_checker, unassigned_scheme_name);
	if (r != idn_success)
		goto failure;
	r = idn_checker_add(bidi_checker, bidi_scheme_name);
	if (r != idn_success)
		goto failure;

	/*
	 * Set the objects.
	 */
	idn_resconf_setmapper(ctx, mapper);
	idn_resconf_setnormalizer(ctx, normalizer);
	idn_resconf_setprohibitchecker(ctx, prohibit_checker);
	idn_resconf_setunassignedchecker(ctx, unassigned_checker);
	idn_resconf_setbidichecker(ctx, bidi_checker);

	/*
	 * Destroy the objects.
	 */
	idn_mapper_destroy(mapper);
	idn_normalizer_destroy(normalizer);
	idn_checker_destroy(prohibit_checker);
	idn_checker_destroy(unassigned_checker);
	idn_checker_destroy(bidi_checker);

	return (idn_success);

failure:
	if (mapper != NULL)
		idn_mapper_destroy(mapper);
	if (normalizer != NULL)
		idn_normalizer_destroy(normalizer);
	if (prohibit_checker != NULL)
		idn_checker_destroy(prohibit_checker);
	if (unassigned_checker != NULL)
		idn_checker_destroy(unassigned_checker);
	if (bidi_checker != NULL)
		idn_checker_destroy(bidi_checker);

	return (r);
}

idn_result_t
idn_resconf_setalternateconvertername(idn_resconf_t ctx, const char *name,
				      int flags) {
	assert(ctx != NULL && name != NULL);

	TRACE(("idn_resconf_setalternateconvertername(name=%s, flags=%d)\n",
	      name, flags));

	return (idn_success);
}

idn_result_t
idn_resconf_setidnconvertername(idn_resconf_t ctx, const char *name,
				int flags) {
	idn_converter_t idn_converter;
	idn_result_t r;

	assert(ctx != NULL && name != NULL);

	TRACE(("idn_resconf_setidnconvertername(name=%s, flags=%d)\n",
	      name, flags));

	r = idn_converter_create(name, &idn_converter, flags);
	if (r != idn_success)
		return (r);

	if (ctx->idn_converter != NULL)
		idn_converter_destroy(ctx->idn_converter);
	ctx->idn_converter = idn_converter;

	return (idn_success);
}

idn_result_t
idn_resconf_setauxidnconvertername(idn_resconf_t ctx, const char *name,
				    int flags) {
	idn_converter_t aux_idn_converter;
	const char *old_name;
	idn_result_t r;

	assert(ctx != NULL && name != NULL);

	TRACE(("idn_resconf_setauxidnconvertername(name=%s, flags=%d)\n",
	      name, flags));

	if (ctx->aux_idn_converter != NULL) {
	    old_name = idn_converter_localencoding(ctx->aux_idn_converter);
	    if (old_name != NULL && strcmp(old_name, name) == 0)
		return (idn_success);
	}

	r = idn_converter_create(name, &aux_idn_converter, flags);
	if (r != idn_success)
		return (r);

	if (ctx->aux_idn_converter != NULL)
		idn_converter_destroy(ctx->aux_idn_converter);
	ctx->aux_idn_converter = aux_idn_converter;

	return (idn_success);
}

idn_result_t
idn_resconf_setlocalconvertername(idn_resconf_t ctx, const char *name,
				  int flags) {
#ifdef WITHOUT_ICONV
	return idn_failure;

#else /* WITHOUT_ICONV */
	idn_converter_t local_converter;
	idn_result_t r;

	assert(ctx != NULL);

	TRACE(("idn_resconf_setlocalconvertername(name=%s, flags=%d)\n",
	      name == NULL ? "<null>" : name, flags));

	if (ctx->local_converter != NULL) {
		idn_converter_destroy(ctx->local_converter);
		ctx->local_converter = NULL;
	}
	ctx->local_converter_is_static = 0;

	if (name != NULL) {
		r = idn_converter_create(name, &local_converter, flags);
		if (r != idn_success)
			return (r);
		ctx->local_converter = local_converter;
		ctx->local_converter_is_static = 1;
	}

	return (idn_success);

#endif /* WITHOUT_ICONV */
}

idn_result_t
idn_resconf_addalldelimitermapucs(idn_resconf_t ctx, unsigned long *v,
				  int nv) {
	idn_result_t r;

	TRACE(("idn_resconf_addalldelimitermapucs(nv=%d)\n", nv));

	if (ctx->delimiter_mapper == NULL) {
		r = idn_delimitermap_create(&(ctx->delimiter_mapper));
		if (r != idn_success)
			return (r);
	}

	r = idn_delimitermap_addall(ctx->delimiter_mapper, v, nv);
	return (r);
}

idn_result_t
idn_resconf_addalllocalmapselectornames(idn_resconf_t ctx, const char *tld,
					const char **names, int nnames) {
	idn_result_t r;

	assert(ctx != NULL && names != NULL && tld != NULL);

	TRACE(("idn_resconf_addalllocalmapselectorname(tld=%s, nnames=%d)\n",
	      tld, nnames));

	if (ctx->local_mapper == NULL) {
		r = idn_mapselector_create(&(ctx->local_mapper));
		if (r != idn_success)
			return (r);
	}

	r = idn_mapselector_addall(ctx->local_mapper, tld, names, nnames);
	return (r);
}

idn_result_t
idn_resconf_addallmappernames(idn_resconf_t ctx, const char **names,
			      int nnames) {
	idn_result_t r;

	assert(ctx != NULL && names != NULL);

	TRACE(("idn_resconf_addallmappername()\n"));

	if (ctx->mapper == NULL) {
		r = idn_mapper_create(&(ctx->mapper));
		if (r != idn_success)
			return (r);
	}

	r = idn_mapper_addall(ctx->mapper, names, nnames);
	return (r);
}

idn_result_t
idn_resconf_addallnormalizernames(idn_resconf_t ctx, const char **names,
				  int nnames) {
	idn_result_t r;

	assert(ctx != NULL && names != NULL);

	TRACE(("idn_resconf_addallnormalizername(nnames=%d)\n", nnames));

	if (ctx->normalizer == NULL) {
		r = idn_normalizer_create(&(ctx->normalizer));
		if (r != idn_success)
			return (r);
	}

	r = idn_normalizer_addall(ctx->normalizer, names, nnames);
	return (r);
}

idn_result_t
idn_resconf_addallprohibitcheckernames(idn_resconf_t ctx, const char **names,
				       int nnames) {
	char long_name[MAX_CONF_LINE_LENGTH + 1];
	idn_result_t r;
	int i;

	assert(ctx != NULL && names != NULL);

	TRACE(("idn_resconf_addallprohibitcheckername(nnames=%d)\n", nnames));

	if (ctx->prohibit_checker == NULL) {
		r = idn_checker_create(&(ctx->prohibit_checker));
		if (r != idn_success)
			return (r);
	}

	for (i = 0; i < nnames; i++, names++) {
		if (strlen(*names) + strlen(IDN_CHECKER_PROHIBIT_PREFIX)
			> MAX_CONF_LINE_LENGTH) {
			return (idn_invalid_name);
		}
		strcpy(long_name, IDN_CHECKER_PROHIBIT_PREFIX);
		strcat(long_name, *names);

		r = idn_checker_add(ctx->prohibit_checker, long_name);
		if (r != idn_success)
			return (r);
	}

	return (idn_success);
}

idn_result_t
idn_resconf_addallunassignedcheckernames(idn_resconf_t ctx, const char **names,
					 int nnames) {
	char long_name[MAX_CONF_LINE_LENGTH + 1];
	idn_result_t r;
	int i;

	assert(ctx != NULL && names != NULL);

	TRACE(("idn_resconf_addallunassignedcheckername(nnames=%d)\n",
	      nnames));

	if (ctx->unassigned_checker == NULL) {
		r = idn_checker_create(&(ctx->unassigned_checker));
		if (r != idn_success)
			return (r);
	}

	for (i = 0; i < nnames; i++, names++) {
		if (strlen(*names) + strlen(IDN_CHECKER_UNASSIGNED_PREFIX)
			> MAX_CONF_LINE_LENGTH) {
			return (idn_invalid_name);
		}
		strcpy(long_name, IDN_CHECKER_UNASSIGNED_PREFIX);
		strcat(long_name, *names);

		r = idn_checker_add(ctx->unassigned_checker, long_name);
		if (r != idn_success)
			return (r);
	}

	return (idn_success);
}

idn_result_t
idn_resconf_addallbidicheckernames(idn_resconf_t ctx, const char **names,
				   int nnames) {
	char long_name[MAX_CONF_LINE_LENGTH + 1];
	idn_result_t r;
	int i;

	assert(ctx != NULL && names != NULL);

	TRACE(("idn_resconf_addallbidicheckername(nnames=%d)\n", nnames));

	if (ctx->bidi_checker == NULL) {
		r = idn_checker_create(&(ctx->bidi_checker));
		if (r != idn_success)
			return (r);
	}

	for (i = 0; i < nnames; i++, names++) {
		if (strlen(*names) + strlen(IDN_CHECKER_BIDI_PREFIX)
			> MAX_CONF_LINE_LENGTH) {
			return (idn_invalid_name);
		}
		strcpy(long_name, IDN_CHECKER_BIDI_PREFIX);
		strcat(long_name, *names);

		r = idn_checker_add(ctx->bidi_checker, long_name);
		if (r != idn_success)
			return (r);
	}

	return (idn_success);
}

static idn_result_t
parse_conf(idn_resconf_t ctx, FILE *fp) {
	char line[MAX_CONF_LINE_LENGTH + 1];
	int lineno = 0;
	char *argv[3];
	int argc;
	idn_result_t r;
	int conf_mask = 0;

	TRACE(("parse_conf()\n"));

	/*
	 * Parse config file.  parsing of 'idn-encoding' line is
	 * postponed because 'alias-file' line must be processed
	 * before them.
	 */
	while (fgets(line, sizeof(line), fp) != NULL) {
		char *newline;

		lineno++;
		newline = strpbrk(line, "\r\n");
		if (newline != NULL)
			*newline = '\0';
		else if (fgetc(fp) != EOF) {
			ERROR(("libidnkit: too long line \"%-.30s\", "
			       "line %d\n", line, lineno));
			return (idn_invalid_syntax);
		}

		argc = split_args(line, argv, 2);
		if (argc == -1) {
			ERROR(("libidnkit: syntax error, line %d\n", lineno));
			return (idn_invalid_syntax);
		} else if (argc == 0 || argv[0][0] == '#') {
			continue;
		} else if (argc == 1) {
			ERROR(("libidnkit: syntax error, line %d\n", lineno));
			return (idn_invalid_syntax);
		}

		if (strcmp(argv[0], "idn-encoding") == 0) {
			if (conf_mask & DEFAULT_CONF_IDN_ENCODING) {
				ERROR(("libidnkit: \"%s\" redefined, "
				       "line %d\n", argv[0], lineno));
				r = idn_invalid_syntax;
			} else {
				conf_mask |= DEFAULT_CONF_IDN_ENCODING;
				r = parse_idn_encoding(ctx, argv[1], lineno);
			}
		} else if (strcmp(argv[0], "local-map") == 0) {
			r = parse_local_map(ctx, argv[1], lineno);

		} else if (strcmp(argv[0], "nameprep") == 0) {
			if (conf_mask & DEFAULT_CONF_NAMEPREP) {
				ERROR(("libidnkit: \"%s\" redefined, "
				       "line %d\n", argv[0], lineno));
				r = idn_invalid_syntax;
			} else {
				conf_mask |= DEFAULT_CONF_NAMEPREP;
				r = parse_nameprep(ctx, argv[1], lineno);
			}
		} else if (strcmp(argv[0], "nameprep-map") == 0 ||
			   strcmp(argv[0], "nameprep-normalize") == 0 ||
			   strcmp(argv[0], "nameprep-prohibit") == 0 ||
			   strcmp(argv[0], "nameprep-unassigned") == 0 ||
			   strcmp(argv[0], "alias-file") == 0 ||
			   strcmp(argv[0], "encoding-alias-file") == 0 ||
			   strcmp(argv[0], "normalize") == 0 ||
			   strcmp(argv[0], "server-encoding") == 0 ||
		           strcmp(argv[0], "alternate-encoding") == 0 ||
			   strcmp(argv[0], "delimiter-map") == 0) {
			WARNING(("libidnkit: obsolete command \"%s\", line %d "
			         "(ignored)\n", argv[0], lineno));
			r = idn_success;
		} else {
			ERROR(("libidnkit: unknown command \"%-.30s\", "
			       "line %d\n", argv[0], lineno));
			r = idn_invalid_syntax;
		}
		if (r != idn_success)
			return (r);
	}

	lineno++;

	if (conf_mask != DEFAULT_CONF_ALL) {
		return setdefaults_body(ctx, conf_mask);
	}

	return (idn_success);
}

static idn_result_t
parse_idn_encoding(idn_resconf_t ctx, char *args, int lineno) {
	idn_result_t r;
	char *argv[MAX_CONF_LINE_ARGS + 1];
	int argc;

	argc = split_args(args, argv, MAX_CONF_LINE_ARGS + 1);

	if (argc != 1) {
		ERROR(("libidnkit: wrong # of args for idn-encoding, "
		       "line %d\n", lineno));
		return (idn_invalid_syntax);
	}

	r = idn_converter_create(argv[0], &ctx->idn_converter,
				 IDN_CONVERTER_DELAYEDOPEN |
				 IDN_CONVERTER_RTCHECK);
	if (r != idn_success) {
		ERROR(("libidnkit: cannot create idn converter, %s, "
		       "line %d\n", idn_result_tostring(r), lineno));
	}

	return (r);
}

static idn_result_t
parse_local_map(idn_resconf_t ctx, char *args, int lineno) {
	idn_result_t r;
	char *argv[MAX_CONF_LINE_ARGS + 1];
	int argc;
	int i;

	argc = split_args(args, argv, MAX_CONF_LINE_ARGS + 1);

	if (argc < 2 || argc > MAX_CONF_LINE_ARGS) {
		ERROR(("libidnkit: wrong # of args for local-map, line %d\n",
		       lineno));
		return (idn_invalid_syntax);
	}

	if (ctx->local_mapper == NULL) {
		r = idn_mapselector_create(&ctx->local_mapper);
		if (r != idn_success) {
			ERROR(("libidnkit: cannot create local mapper, %s, "
			       "line %d\n", idn_result_tostring(r), lineno));
			return (r);
		}
	}

	for (i = 1; i < argc; i++) {
		r = idn_mapselector_add(ctx->local_mapper, argv[0], argv[i]);
		if (r == idn_invalid_name) {
			ERROR(("libidnkit: map scheme unavailable \"%-.30s\""
			       " or invalid TLD \"%-.30s\", line %d\n",
			       argv[i], argv[0], lineno));
			return (r);
		} else if (r != idn_success) {
			return (r);
		}
	}

	return (idn_success);
}

static idn_result_t
parse_nameprep(idn_resconf_t ctx, char *args, int lineno) {
	idn_result_t r;
	char *argv[MAX_CONF_LINE_ARGS + 1];
	char scheme_name[MAX_CONF_LINE_LENGTH + 1];
	int argc;

	argc = split_args(args, argv, MAX_CONF_LINE_ARGS + 1);

	if (argc != 1) {
		ERROR(("libidnkit: wrong # of args for nameprep, line %d\n",
		       lineno));
		return (idn_invalid_syntax);
	}

	/*
	 * Set mapper.
	 */
	r = idn_mapper_create(&ctx->mapper);
	if (r != idn_success) {
		ERROR(("libidnkit: cannot create mapper, %s, line %d\n",
		       idn_result_tostring(r), lineno));
		return (r);
	}

	r = idn_mapper_add(ctx->mapper, argv[0]);
	if (r == idn_invalid_name) {
		ERROR(("libidnkit: map scheme unavailable \"%-.30s\", "
		       "line %d\n", argv[0], lineno));
		return (r);
	} else if (r != idn_success) {
		return (r);
	}

	/*
	 * Set normalizer.
	 */
	r = idn_normalizer_create(&ctx->normalizer);
	if (r != idn_success) {
		ERROR(("libidnkit: cannot create normalizer, %s, line %d\n",
		       idn_result_tostring(r), lineno));
		return (r);
	}

	r = idn_normalizer_add(ctx->normalizer, argv[0]);
	if (r == idn_invalid_name) {
		ERROR(("libidnkit: unknown normalization scheme \"%-.30s\", "
		       "line %d\n", argv[0], lineno));
		return (r);
	} else if (r != idn_success) {
		return (r);
	}

	/*
	 * Set prohibit checker.
	 */
	r = idn_checker_create(&ctx->prohibit_checker);
	if (r != idn_success) {
		ERROR(("libidnkit: cannot create prohibit checker, %s, "
		       "line %d\n", idn_result_tostring(r), lineno));
		return (r);
	}

	sprintf(scheme_name, "%s%s", IDN_CHECKER_PROHIBIT_PREFIX, argv[0]);
	r = idn_checker_add(ctx->prohibit_checker, scheme_name);
	if (r == idn_invalid_name) {
		ERROR(("libidnkit: unknown prohibit scheme \"%-.30s\", "
		       "line %d\n", argv[0], lineno));
		return (r);
	} else if (r != idn_success) {
		return (r);
	}

	/*
	 * Set unassigned checker.
	 */
	r = idn_checker_create(&ctx->unassigned_checker);
	if (r != idn_success) {
		ERROR(("libidnkit: cannot create unassigned checker, %s, "
		       "line %d\n", idn_result_tostring(r), lineno));
		return (r);
	}

	sprintf(scheme_name, "%s%s", IDN_CHECKER_UNASSIGNED_PREFIX, argv[0]);
	r = idn_checker_add(ctx->unassigned_checker, scheme_name);
	if (r == idn_invalid_name) {
		ERROR(("libidnkit: unknown unassigned scheme \"%-.30s\", "
		       "line %d\n", argv[0], lineno));
		return (r);
	} else if (r != idn_success) {
		return (r);
	}

	/*
	 * Set bidi checker.
	 */
	r = idn_checker_create(&ctx->bidi_checker);
	if (r != idn_success) {
		ERROR(("libidnkit: cannot create bidi checker, %s, line %d\n",
		       idn_result_tostring(r), lineno));
		return (r);
	}

	sprintf(scheme_name, "%s%s", IDN_CHECKER_BIDI_PREFIX, argv[0]);
	r = idn_checker_add(ctx->bidi_checker, scheme_name);
	if (r == idn_invalid_name) {
		ERROR(("libidnkit: unknown bidi scheme \"%-.30s\", "
		       "line %d\n", argv[0], lineno));
		return (r);
	} else if (r != idn_success) {
		return (r);
	}

	return (idn_success);
}

static int
split_args(char *s, char **av, int max_ac) {
	int ac;
	int i;

	for (ac = 0; *s != '\0' && ac < max_ac; ac++) {
		if (ac > 0)
			*s++ = '\0';
		while (isspace((unsigned char)*s))
			s++;
		if (*s == '\0')
			break;
		if (*s == '"' || *s == '\'') {
			int qc = *s++;
			av[ac] = s;
			while (*s != qc) {
				if (*s == '\0')
					return (-1);
				s++;
			}
		} else {
			av[ac] = s;
			while (*s != '\0' && !isspace((unsigned char)*s))
				s++;
		}
	}

	for (i = ac; i < max_ac; i++)
		av[i] = NULL;

	return (ac);
}

static void
resetconf(idn_resconf_t ctx) {
#ifndef WITHOUT_ICONV
	idn_resconf_setlocalconverter(ctx, NULL);
#endif
	idn_resconf_setidnconverter(ctx, NULL);
	idn_resconf_setauxidnconverter(ctx, NULL);
	idn_resconf_setdelimitermap(ctx, NULL);
	idn_resconf_setlocalmapselector(ctx, NULL);
	idn_resconf_setmapper(ctx, NULL);
	idn_resconf_setnormalizer(ctx, NULL);
	idn_resconf_setprohibitchecker(ctx, NULL);
	idn_resconf_setunassignedchecker(ctx, NULL);
	idn_resconf_setbidichecker(ctx, NULL);
}

#ifndef WITHOUT_ICONV
static idn_result_t
update_local_converter(idn_resconf_t ctx) {
	idn_result_t r;
	const char *old_encoding;
	const char *new_encoding;

	/*
	 * We don't update local converter, if the converter is set
	 * by idn_resconf_setlocalconverter() or
	 * idn_resconf_setlocalconvertername().
	 */
	if (ctx->local_converter_is_static)
		return (idn_success);

	/*
	 * Update the local converter if the local encoding is changed.
	 */
	old_encoding = (ctx->local_converter != NULL) ?
		       idn_converter_localencoding(ctx->local_converter) :
		       NULL;
	new_encoding = idn_localencoding_name();
	if (new_encoding == NULL) {
		ERROR(("cannot determine local codeset name\n"));
		return (idn_notfound);
	}

	if (old_encoding != NULL &&
	    new_encoding != NULL &&
	    strcmp(old_encoding, new_encoding) == 0) {
		return (idn_success);
	}

	if (ctx->local_converter != NULL) {
		idn_converter_destroy(ctx->local_converter);
		ctx->local_converter = NULL;
	}

	r = idn_converter_create(new_encoding,
				 &ctx->local_converter,
				 IDN_CONVERTER_RTCHECK);
	return (r);
}
#endif

idn_result_t
idn_resconf_setdefaults(idn_resconf_t ctx)
{
	idn_result_t r;

	assert(ctx != NULL);

	TRACE(("idn_resconf_setdefaults()\n"));

	resetconf(ctx);
	r = idn_delimitermap_create(&ctx->delimiter_mapper);
	if (r != idn_success) {
		ERROR(("libidnkit: cannot create delimiter mapper, %s\n",
		       idn_result_tostring(r)));
		return (r);
	}

	return setdefaults_body(ctx, 0);
}

static idn_result_t
setdefaults_body(idn_resconf_t ctx, int conf_mask) {
	idn_result_t r;

	TRACE(("setdefaults_body()\n"));
	assert(ctx != NULL);

	if (!(conf_mask & DEFAULT_CONF_NAMEPREP)) {
		TRACE(("set default nameprep\n"));
		r = idn_resconf_setnameprepversion(ctx, IDN_NAMEPREP_CURRENT);
		if (r != idn_success) {
			return (r);
		}
	}
	if (!(conf_mask & DEFAULT_CONF_IDN_ENCODING)) {
		TRACE(("set default idn encoding\n"));
		r = idn_converter_create(IDN_ENCODING_CURRENT,
					 &ctx->idn_converter,
					 IDN_CONVERTER_DELAYEDOPEN |
					 IDN_CONVERTER_RTCHECK);
		if (r != idn_success) {
			ERROR(("libidnkit: cannot create idn converter, %s\n",
			       idn_result_tostring(r)));
			return (r);
		}
	}

	return (idn_success);
}
