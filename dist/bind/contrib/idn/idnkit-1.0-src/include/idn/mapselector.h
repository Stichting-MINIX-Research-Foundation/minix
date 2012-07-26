/* $Id: mapselector.h,v 1.1.1.1 2003-06-04 00:25:39 marka Exp $ */
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

#ifndef IDN_MAPSELECTOR_H
#define IDN_MAPSELECTOR_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Map selector.
 *
 * Perfom mapping the specified domain name according with the TLD
 * of the donmain name.
 */

#include <idn/export.h>
#include <idn/result.h>
#include <idn/mapper.h>

/*
 * Special TLDs for map selection.
 */
#define IDN_MAPSELECTOR_NOTLD		"-"
#define IDN_MAPSELECTOR_DEFAULTTLD	"."

IDN_EXPORT const unsigned long *
idn_mapselector_getnotld(void);

IDN_EXPORT const unsigned long *
idn_mapselector_getdefaulttld(void);

/*
 * Mapselector object type.
 */
typedef struct idn_mapselector *idn_mapselector_t;

/*
 * Initialize module.  Must be called before any other calls of
 * the functions of this module.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_mapselector_initialize(void);

/*
 * Create a mapselector context.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_mapselector_create(idn_mapselector_t *ctxp);

/*
 * Decrement reference count of the mapselector `ctx' created by
 * 'idn_mapselector_create', if it is still refered by another object.
 * Otherwise, release all the memory allocated to the mapselector.
 */
IDN_EXPORT void
idn_mapselector_destroy(idn_mapselector_t ctx);

/*
 * Increment reference count of the mapselector `ctx' created by
 * 'idn_mapselector_create'.
 */
IDN_EXPORT void
idn_mapselector_incrref(idn_mapselector_t ctx);

/*
 * Return the mapper for `tld' registered in `ctx', or return NULL if
 * mapper for `tld' is not registered.
 */
IDN_EXPORT idn_mapper_t
idn_mapselector_mapper(idn_mapselector_t ctx, const char *tld);

/*
 * Add mapping scheme `name' to the mapper for `tld' to the mapselector
 * context `ctx'.  If no mapper for `TLD' has not been registered, the
 * function creates a new mapper for `tld', and then adds the given mapping
 * scheme to the mapper.  Otherwise,  it adds the scheme to the mapper for
 * TLD registered in `ctx'.
 * 
 * Returns:
 *      idn_success             -- ok.
 *      idn_invalid_name        -- the given tld or name is not valid.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_mapselector_add(idn_mapselector_t ctx, const char *tld, const char *name);

IDN_EXPORT idn_result_t
idn_mapselector_addall(idn_mapselector_t ctx, const char *tld,
		       const char **names, int nnames);

/*
 * Map an UCS4 string with the mapper for TLD of the domain name.
 * If there is no mapper suitable for the domain name, the function
 * simply copies the doman name.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 *      idn_buffer_overflow     -- output buffer is too small.
 *	idn_invalid_name        -- the given tld is not valid.
 */
IDN_EXPORT idn_result_t
idn_mapselector_map(idn_mapselector_t ctx, const unsigned long *from,
		    const char *tld, unsigned long *to, size_t tolen);

IDN_EXPORT idn_result_t
idn_mapselector_map2(idn_mapselector_t ctx, const unsigned long *from,
		     const unsigned long *tld, unsigned long *to,
		     size_t tolen);

#ifdef __cplusplus
}
#endif

#endif /* IDN_MAPSELECTOR_H */
