/* $Id: ucsset.h,v 1.1.1.1 2003-06-04 00:25:43 marka Exp $ */
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

#ifndef IDN_UCSSET_H
#define IDN_UCSSET_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A 'set' of UCS codepoints.
 */

#include <idn/export.h>
#include <idn/result.h>

/*
 * Type representing a set (opaque).
 */
typedef struct idn_ucsset *idn_ucsset_t;


/*
 * Create an empty set.  The reference count is set to 1.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_ucsset_create(idn_ucsset_t *ctxp);

/*
 * Decrement the reference count of the given set, and if it reaches zero,
 * release all the memory allocated for it.
 */
IDN_EXPORT void
idn_ucsset_destroy(idn_ucsset_t ctx);

/*
 * Increments the reference count by one.
 */
IDN_EXPORT void
idn_ucsset_incrref(idn_ucsset_t ctx);

/*
 * Add a UCS code point to the set.
 * The set must be in the building phase -- that is, before 'idn_ucsset_fix'
 * is called for the set.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_code	-- code point out of range.
 *	idn_nomemory		-- malloc failed.
 *	idn_failure		-- already fixed by 'idn_ucsset_fix'.
 */
IDN_EXPORT idn_result_t
idn_ucsset_add(idn_ucsset_t ctx, unsigned long v);

/*
 * Add a range of code points (from 'from' to 'to', inclusive) to the set.
 * 'from' must not be greater than 'to'.
 * This function is similar to 'idn_ucsset_add' except that it accepts
 * range of code points.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_code	-- code point out of range, or the range
 *				   specification is invalid.
 *	idn_nomemory		-- malloc failed.
 *	idn_failure		-- already fixed by 'idn_ucsset_fix'.
 */
IDN_EXPORT idn_result_t
idn_ucsset_addrange(idn_ucsset_t ctx, unsigned long from, unsigned long to);

/*
 * Perform internal arrangement of the set for lookup.
 * Before calling this function, a set is in 'building' phase, and code
 * points can be added freely by 'idn_ucsset_add' or 'idn_ucsset_addrange'.
 * But once it is fixed by this function, the set becomes immutable, and
 * it shifts into 'lookup' phase.
 */
IDN_EXPORT void
idn_ucsset_fix(idn_ucsset_t ctx);

/*
 * Find if the given code point is in the set.
 * The set must be in the lookup phase -- in other words, 'idn_ucsset_fix'
 * must be called for the set before calling this function.
 * '*found' is set to 1 if the specified code point is in the set, 0 otherwise.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_invalid_code	-- specified code point is out of range.
 *	idn_failure		-- not fixed by 'idn_ucsset_fix' yet.
 */
IDN_EXPORT idn_result_t
idn_ucsset_lookup(idn_ucsset_t ctx, unsigned long v, int *found);

#ifdef __cplusplus
}
#endif

#endif /* IDN_UCSSET_H */
