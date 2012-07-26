/* $Id: ucsmap.h,v 1.1.1.1 2003-06-04 00:25:42 marka Exp $ */
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

#ifndef IDN_UCSMAP_H
#define IDN_UCSMAP_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Perform UCS character mapping.
 * This module support one-to-N mapping (N may be zero, one or more).
 */

#include <idn/export.h>
#include <idn/result.h>

/*
 * Mapper type (opaque).
 */
typedef struct idn_ucsmap *idn_ucsmap_t;

/*
 * Create an empty mapping.  The reference count is set to 1.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_ucsmap_create(idn_ucsmap_t *ctxp);

/*
 * Decrement the reference count of the given set, and if it reaches zero,
 * release all the memory allocated for it.
 */
IDN_EXPORT void
idn_ucsmap_destroy(idn_ucsmap_t ctx);

/*
 * Increment the reference count of the given set by one, so that
 * the map can be shared.
 */
IDN_EXPORT void
idn_ucsmap_incrref(idn_ucsmap_t ctx);

/*
 * Add a mapping.
 * 'ucs' is the character to be mapped, 'map' points an array of mapped
 * characters of length 'maplen'.  'map' may be NULL if 'maplen' is zero,
 * meaning one-to-none mapping.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 *	idn_failure		-- already fixed by 'idn_ucsmap_fix',
 *				   or too large maplen.
 */
IDN_EXPORT idn_result_t
idn_ucsmap_add(idn_ucsmap_t ctx, unsigned long ucs, unsigned long *map,
	       size_t maplen);

/*
 * Perform internal arrangement of the map for lookup.
 * Once it is fixed, 'idn_ucsmap_add' cannot be permitted to the map.
 */
IDN_EXPORT void
idn_ucsmap_fix(idn_ucsmap_t ctx);

/*
 * Find the mapping for the given character.
 * 'idn_ucsmap_fix' must be performed before calling this function.
 * Find the mapping for 'v' and store the result to 'to'.  The length
 * of the mapped sequence is stored in '*maplenp'.  'tolen' specifies
 * the length allocated for 'to'.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomapping		-- specified character has no mapping.
 *	idn_failure		-- not fixed by 'idn_ucsmap_fix' yet.
 */
IDN_EXPORT idn_result_t
idn_ucsmap_map(idn_ucsmap_t ctx, unsigned long v, unsigned long *to,
	       size_t tolen, size_t *maplenp);

#ifdef __cplusplus
}
#endif

#endif /* IDN_UCSMAP_H */
