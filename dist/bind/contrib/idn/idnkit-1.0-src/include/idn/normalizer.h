/* $Id: normalizer.h,v 1.1.1.1 2003-06-04 00:25:40 marka Exp $ */
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

#ifndef IDN_NORMALIZER_H
#define IDN_NORMALIZER_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Domain name normalizer.
 *
 * Perform normalization on the specified strings.  String must be
 * in UCS4 encoding.
 */

#include <idn/export.h>
#include <idn/result.h>

/*
 * Normalizer type (opaque).
 */
typedef struct idn_normalizer *idn_normalizer_t;

/*
 * Normalizer procedure type.
 */
typedef idn_result_t (*idn_normalizer_proc_t)(const unsigned long *from,
					      unsigned long *to, size_t tolen);

/*
 * Initialize this module.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_normalizer_initialize(void);

/*
 * Create a empty normalizer.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_normalizer_create(idn_normalizer_t *ctxp);

/*
 * Decrement reference count of the normalizer `ctx' created by
 * 'idn_normalizer_create', if it is still refered by another object.
 * Otherwise, release all the memory allocated to the normalizer.
 */
IDN_EXPORT void
idn_normalizer_destroy(idn_normalizer_t ctx);

/*
 * Increment reference count of the normalizer `ctx' created by
 * 'idn_normalizer_create'.
 */
IDN_EXPORT void
idn_normalizer_incrref(idn_normalizer_t ctx);

/*
 * Add a normalization scheme to a normalizer.
 *
 * Multiple shemes can be added to a normalizer, and they will be
 * applied in order.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_name	-- unknown scheme was specified.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_normalizer_add(idn_normalizer_t ctx, const char *scheme_name);

IDN_EXPORT idn_result_t
idn_normalizer_addall(idn_normalizer_t ctx, const char **scheme_names,
		      int nschemes);

/*
 * Perform normalization(s) defined by a normalizer to the specified string, 
 * If the normalizer has two or more normalization schemes, they are
 * applied in order.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_buffer_overflow	-- output buffer is too small.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_normalizer_normalize(idn_normalizer_t ctx, const unsigned long *from,
			 unsigned long *to, size_t tolen);

/*
 * Register a new normalization scheme.
 *
 * You can override the default normalization schemes, if you want.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_normalizer_register(const char *scheme_name, idn_normalizer_proc_t proc);

#ifdef __cplusplus
}
#endif

#endif /* IDN_NORMALIZER_H */
