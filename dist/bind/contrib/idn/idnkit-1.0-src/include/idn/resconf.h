/* $Id: resconf.h,v 1.1.1.1 2003-06-04 00:25:41 marka Exp $ */
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

#ifndef IDN_RESCONF_H
#define IDN_RESCONF_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDN resolver configuration.
 */

#include <idn/export.h>
#include <idn/result.h>
#include <idn/converter.h>
#include <idn/normalizer.h>
#include <idn/checker.h>
#include <idn/mapper.h>
#include <idn/mapselector.h>
#include <idn/delimitermap.h>

/*
 * Configuration type (opaque).
 */
typedef struct idn_resconf *idn_resconf_t;

/*
 * Initialize.
 *
 * Initialize this module and underlying ones.  Must be called before
 * any other functions of this module.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_resconf_initialize(void);

/*
 * Create a configuration context.
 *
 * Create an empty context and store it in '*ctxp'.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_resconf_create(idn_resconf_t *ctxp);

/*
 * Destroy the configuration context.
 *
 * Destroy the configuration context created by 'idn_resconf_create',
 * and release memory for it.
 */
IDN_EXPORT void
idn_resconf_destroy(idn_resconf_t ctx);

/*
 * Increment reference count of the context created by 'idn_resconf_create'.
 */
IDN_EXPORT void
idn_resconf_incrref(idn_resconf_t ctx);

/*
 * Set default configurations to resconf context.
 *
 * "default configurations" means current nameprep and IDN encoding
 * which IDN standard document suggests.
 * 
 * Warning: configurations set previously are removed.
 * 
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_syntax	-- syntax error found.
 *	idn_invalid_name	-- invalid encoding/nomalization name is
 *				   specified.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_resconf_setdefaults(idn_resconf_t ctx);

/*
 * Load configuration file.
 *
 * Parse a configuration file whose name is specified by 'file',
 * store the result in 'ctx'.  If 'file' is NULL, the default file is
 * loaded.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nofile		-- couldn't open specified file.
 *	idn_invalid_syntax	-- syntax error found.
 *	idn_invalid_name	-- invalid encoding/nomalization name is
 *				   specified.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_resconf_loadfile(idn_resconf_t ctx, const char *file);

/*
 * Get the pathname of the default configuration file.
 *
 * Returns:
 *	the pathname of the default configuration file.
 */
IDN_EXPORT char *
idn_resconf_defaultfile(void);

/*
 * Get an object of lower module that `ctx' holds.
 */
IDN_EXPORT idn_delimitermap_t
idn_resconf_getdelimitermap(idn_resconf_t ctx);

IDN_EXPORT idn_converter_t
idn_resconf_getidnconverter(idn_resconf_t ctx);

IDN_EXPORT idn_converter_t
idn_resconf_getauxidnconverter(idn_resconf_t ctx);

IDN_EXPORT idn_converter_t
idn_resconf_getlocalconverter(idn_resconf_t ctx);

IDN_EXPORT idn_mapselector_t
idn_resconf_getlocalmapselector(idn_resconf_t ctx);

IDN_EXPORT idn_mapper_t
idn_resconf_getmapper(idn_resconf_t ctx);

IDN_EXPORT idn_normalizer_t
idn_resconf_getnormalizer(idn_resconf_t ctx);

IDN_EXPORT idn_checker_t
idn_resconf_getprohibitchecker(idn_resconf_t ctx);

IDN_EXPORT idn_checker_t
idn_resconf_getunassignedchecker(idn_resconf_t ctx);

IDN_EXPORT idn_checker_t
idn_resconf_getbidichecker(idn_resconf_t ctx);

/*
 * Set an object of lower module to `ctx'.
 */
IDN_EXPORT void
idn_resconf_setdelimitermap(idn_resconf_t ctx,
			    idn_delimitermap_t delimiter_mapper);

IDN_EXPORT void
idn_resconf_setidnconverter(idn_resconf_t ctx,
                            idn_converter_t idn_coverter);

IDN_EXPORT void
idn_resconf_setauxidnconverter(idn_resconf_t ctx,
                               idn_converter_t aux_idn_coverter);

IDN_EXPORT void
idn_resconf_setlocalconverter(idn_resconf_t ctx,
			      idn_converter_t local_converter);

IDN_EXPORT void
idn_resconf_setlocalmapselector(idn_resconf_t ctx,
				idn_mapselector_t map_selector);

IDN_EXPORT void
idn_resconf_setmapper(idn_resconf_t ctx, idn_mapper_t mapper);

IDN_EXPORT void
idn_resconf_setnormalizer(idn_resconf_t ctx, idn_normalizer_t normalizer);

IDN_EXPORT void
idn_resconf_setprohibitchecker(idn_resconf_t ctx,
			       idn_checker_t prohibit_checker);

IDN_EXPORT void
idn_resconf_setunassignedchecker(idn_resconf_t ctx,
				 idn_checker_t unassigned_checker);

IDN_EXPORT void
idn_resconf_setbidichecker(idn_resconf_t ctx,
			   idn_checker_t bidi_checker);

/*
 * Set name or add names to an object of lower module that `ctx' holds.
 */
IDN_EXPORT idn_result_t
idn_resconf_setidnconvertername(idn_resconf_t ctx, const char *name,
				int flags);

IDN_EXPORT idn_result_t
idn_resconf_setauxidnconvertername(idn_resconf_t ctx, const char *name,
				   int flags);

IDN_EXPORT idn_result_t
idn_resconf_addalldelimitermapucs(idn_resconf_t ctx, unsigned long *v, int nv);

IDN_EXPORT idn_result_t
idn_resconf_setlocalconvertername(idn_resconf_t ctx, const char *name,
				  int flags);

IDN_EXPORT idn_result_t
idn_resconf_addalllocalmapselectornames(idn_resconf_t ctx, const char *tld,
					const char **names, int nnames);

IDN_EXPORT idn_result_t
idn_resconf_addallmappernames(idn_resconf_t ctx, const char **names,
			      int nnames);

IDN_EXPORT idn_result_t
idn_resconf_addallnormalizernames(idn_resconf_t ctx, const char **names,
				  int nnames);

IDN_EXPORT idn_result_t
idn_resconf_addallprohibitcheckernames(idn_resconf_t ctx, const char **names,
				       int nnames);

IDN_EXPORT idn_result_t
idn_resconf_addallunassignedcheckernames(idn_resconf_t ctx, const char **names,
					 int nnames);

IDN_EXPORT idn_result_t
idn_resconf_addallbidicheckernames(idn_resconf_t ctx, const char **names,
				   int nnames);

IDN_EXPORT idn_result_t
idn_resconf_setnameprepversion(idn_resconf_t ctx, const char *version);

/*
 * These macros are provided for backward compatibility to mDNkit 2.1
 * and older.
 */
IDN_EXPORT void
idn_resconf_setalternateconverter(idn_resconf_t ctx,
                                  idn_converter_t alternate_converter);

IDN_EXPORT idn_result_t
idn_resconf_setalternateconvertername(idn_resconf_t ctx, const char *name,
				      int flags);

IDN_EXPORT idn_converter_t
idn_resconf_getalternateconverter(idn_resconf_t ctx);


/*
 * These macros are provided for backward compatibility to idnkit 1.x.
 */
#define idn_resconf_localconverter(ctx) \
	idn_resconf_getlocalconverter(ctx)

#define idn_resconf_idnconverter(ctx) \
	idn_resconf_getidnconverter(ctx)

#define idn_resconf_alternateconverter(ctx) \
	idn_resconf_getalternateconverter(ctx)

#define idn_resconf_normalizer(ctx) \
	idn_resconf_getnormalizer(ctx)

#define idn_resconf_mapper(ctx) \
	idn_resconf_getmapper(ctx)

#define idn_resconf_delimitermap(ctx) \
	idn_resconf_getdelimitermap(ctx)

#define idn_resconf_localmapselector(ctx) \
	idn_resconf_getlocalmapselector(ctx)

#define idn_resconf_prohibitchecker(ctx) \
	idn_resconf_getprohibitchecker(ctx)

#define idn_resconf_unassignedchecker(ctx) \
	idn_resconf_getunassignedchecker(ctx)

#ifdef __cplusplus
}
#endif

#endif /* IDN_RESCONF_H */
