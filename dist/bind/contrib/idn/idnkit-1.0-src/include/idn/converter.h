/* $Id: converter.h,v 1.1.1.1 2003-06-04 00:25:36 marka Exp $ */
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

#ifndef IDN_CONVERTER_H
#define IDN_CONVERTER_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Codeset converter.
 *
 * This module provides conversions from some local codeset to UCS4
 * and vice versa.
 */

#include <idn/export.h>
#include <idn/result.h>

/*
 * Converter context type (opaque).
 */
typedef struct idn_converter *idn_converter_t;

/*
 * Conversion flags.
 */
#define IDN_CONVERTER_DELAYEDOPEN	1
#define IDN_CONVERTER_RTCHECK		2

/*
 * Encoding types.
 */
#define IDN_NONACE			0
#define IDN_ACE_STRICTCASE		1
#define IDN_ACE_LOOSECASE		2

/*
 * Initialize module.  Must be called before any other calls of
 * the functions of this module.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_converter_initialize(void);

/*
 * Create a conversion context.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_name	-- specified codeset is not supported.
 *	idn_nomemory		-- malloc failed.
 *	idn_failure		-- other failure (unknown cause).
 */
IDN_EXPORT idn_result_t
idn_converter_create(const char *name, idn_converter_t *ctxp,
		     int flags);

/*
 * Decrement reference count of the converter `ctx' created by
 * 'idn_converter_create', if it is still refered by another object.
 * Otherwise, release all the memory allocated to the converter.
 */
IDN_EXPORT void
idn_converter_destroy(idn_converter_t ctx);

/*
 * Increment reference count of the converter `ctx' created by
 * 'idn_converter_create'.
 */
IDN_EXPORT void
idn_converter_incrref(idn_converter_t ctx);

/*
 * Convert between local codeset and UCS4.  Note that each conversion
 * is started with initial state.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_buffer_overflow	-- output buffer is too small.
 *	idn_invalid_encoding	-- the input string has invalid/illegal
 *				   byte sequence.
 *	idn_invalid_name	-- codeset is not supported (this error
 *				   should happen only if 'delayedopen'
 *				   flag was set when idn_converter_create
 *				   was called)
 *	idn_failure		-- other failure.
 */
IDN_EXPORT idn_result_t
idn_converter_convfromucs4(idn_converter_t ctx, 
			   const unsigned long *from, char *to, size_t tolen);

IDN_EXPORT idn_result_t
idn_converter_convtoucs4(idn_converter_t ctx, 
			 const char *from, unsigned long *to, size_t tolen);

/*
 * Get the name of local codeset.  The returned name may be different from
 * the one specified to idn_converter_create, if the specified one was an
 * alias.
 *
 * Returns:
 *	the local codeset name.
 */
IDN_EXPORT char *
idn_converter_localencoding(idn_converter_t ctx);

/*
 * Return the encoding type of this local encoding.
 *
 * Returns:
 *	IDN_NONACE		-- encoding is not ACE.
 *	IDN_ACE_STRICTCASE	-- encoding is ACE.
 *				   decoder of this ACE preserve letter case.
 *	IDN_ACE_LOOSECASE	-- encoding type is ACE.
 *				   decoder cannot preserve letter case.
 */
IDN_EXPORT int
idn_converter_encodingtype(idn_converter_t ctx);

/*
 * Return if this local encoding is ACE (Ascii Compatible Encoding).
 *
 * Returns:
 *	1	-- yes, it is ACE.
 *	0	-- no.
 */
IDN_EXPORT int
idn_converter_isasciicompatible(idn_converter_t ctx);

/*
 * Register an alias for a codeset name.
 *
 * If first_item is 0, alias pattern is placed as the last item of the
 * alias list.  Otherwise, it is done as the first item.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_converter_addalias(const char *alias_name, const char *real_name,
		       int first_item);

/*
 * Register aliases defined by the specified file.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nofile		-- no such file.
 *	idn_invalid_syntax	-- file is malformed.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_converter_aliasfile(const char *path);

/*
 * Unregister all the aliases.
 */
IDN_EXPORT idn_result_t
idn_converter_resetalias(void);

/*
 * resolve real encoding name from alias information.
 */
IDN_EXPORT const char *
idn_converter_getrealname(const char *name);


/*
 * New converter registration.
 */

/*
 * Conversion operation functions.
 */
typedef idn_result_t (*idn_converter_openproc_t)(idn_converter_t ctx,
						 void **privdata);
typedef idn_result_t (*idn_converter_closeproc_t)(idn_converter_t ctx,
						  void *privdata);
typedef idn_result_t
	(*idn_converter_convfromucs4proc_t)(idn_converter_t ctx,
					    void *privdata,
					    const unsigned long *from,
					    char *to, size_t tolen);
typedef idn_result_t
	(*idn_converter_convtoucs4proc_t)(idn_converter_t ctx,
					  void *privdata,
					  const char *from,
					  unsigned long *to,
					  size_t tolen);

/*
 * Register a new converter.
 * 'encoding_type' is a value which idn_converter_encodingtype() returns.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_converter_register(const char *name,
		       idn_converter_openproc_t openfromucs4,
		       idn_converter_openproc_t opentoucs4,
		       idn_converter_convfromucs4proc_t convfromucs4,
		       idn_converter_convtoucs4proc_t convtoucs4,
		       idn_converter_closeproc_t close,
		       int encoding_type);

#ifdef __cplusplus
}
#endif

#endif /* IDN_CONVERTER_H */
