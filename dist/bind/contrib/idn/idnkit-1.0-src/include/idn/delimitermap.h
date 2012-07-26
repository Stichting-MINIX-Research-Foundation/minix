/* $Id: delimitermap.h,v 1.1.1.1 2003-06-04 00:25:37 marka Exp $ */
/*
 * Copyright (c) 2001 Japan Network Information Center.  All rights reserved.
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

#ifndef IDN_DELIMITERMAP_H
#define IDN_DELIMITERMAP_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mapper.
 *
 * Perfom mapping local delimiters to `.'.
 */

#include <idn/export.h>
#include <idn/result.h>

/*
 * Map object type.
 */
typedef struct idn_delimitermap *idn_delimitermap_t;

/*
 * Create a delimitermap context.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_delimitermap_create(idn_delimitermap_t *ctxp);

/*
 * Decrement reference count of the delimitermap `ctx' created by
 * 'idn_delimitermap_create', if it is still refered by another object.
 * Otherwise, release all the memory allocated to the delimitermap.
 */
IDN_EXPORT void
idn_delimitermap_destroy(idn_delimitermap_t ctx);

/*
 * Increment reference count of the delimitermap `ctx' created by
 * 'idn_delimitermap_create'.
 */
IDN_EXPORT void
idn_delimitermap_incrref(idn_delimitermap_t ctx);

/*
 * Add a local delimiter.
 * 
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 *      idn_invalid_codepoint   -- delimiter is not valid UCS4 character.
 */
IDN_EXPORT idn_result_t
idn_delimitermap_add(idn_delimitermap_t ctx, unsigned long delimiter);

IDN_EXPORT idn_result_t
idn_delimitermap_addall(idn_delimitermap_t ctx, unsigned long *delimiters,
			int ndelimiters);

/*
 * Map local delimiters in `from' to `.'.
 *
 * Note that if no delimiter is added to the context, the function copies
 * the string.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_buffer_overflow     -- output buffer is too small.
 */
IDN_EXPORT idn_result_t
idn_delimitermap_map(idn_delimitermap_t ctx, const unsigned long *from,
		     unsigned long *to, size_t tolen);

#ifdef __cplusplus
}
#endif

#endif /* IDN_DELIMITERMAP_H */
