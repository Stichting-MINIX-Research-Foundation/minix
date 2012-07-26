/* $Id: mapper.h,v 1.1.1.1 2003-06-04 00:25:38 marka Exp $ */
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

#ifndef IDN_MAPPER_H
#define IDN_MAPPER_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mapper.
 *
 * Perfom mapping the specified domain name.
 */

#include <idn/export.h>
#include <idn/result.h>
#include <idn/filemapper.h>
#include <idn/nameprep.h>

/*
 * Map object type.
 */
typedef struct idn_mapper *idn_mapper_t;

/*
 * Initialize module.  Must be called before any other calls of
 * the functions of this module.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_mapper_initialize(void);

/*
 * Create a mapper context.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_mapper_create(idn_mapper_t *ctxp);

/*
 * Decrement reference count of the mapper `ctx' created by
 * 'idn_mapper_create', if it is still refered by another object.
 * Otherwise, release all the memory allocated to the mapper.
 */
IDN_EXPORT void
idn_mapper_destroy(idn_mapper_t ctx);

/*
 * Increment reference count of the mapper `ctx' created by
 * 'idn_mapper_create'.
 */
IDN_EXPORT void
idn_mapper_incrref(idn_mapper_t ctx);

/*
 * Add mapping scheme `name' to the mapper to `ctx'.
 * 
 * Returns:
 *      idn_success             -- ok.
 *      idn_invalid_name        -- the given name is not valid.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_mapper_add(idn_mapper_t ctx, const char *name);

IDN_EXPORT idn_result_t
idn_mapper_addall(idn_mapper_t ctx, const char **names, int nnames);

/*
 * Map an UCS4 string.  All mapping schemes regsitered in `ctx'
 * are applied in the regisration order.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 *      idn_buffer_overflow     -- output buffer is too small.
 */
IDN_EXPORT idn_result_t
idn_mapper_map(idn_mapper_t ctx, const unsigned long *from,
	       unsigned long *to, size_t tolen);

/*
 * Mapping procedure type.
 */
typedef idn_result_t (*idn_mapper_createproc_t)(const char *parameter,
						void **ctxp);
typedef void         (*idn_mapper_destroyproc_t)(void *ctxp);
typedef idn_result_t (*idn_mapper_mapproc_t)(void *ctx,
					     const unsigned long *from,
                                             unsigned long *, size_t);
                                              
/*
 * Register a new mapping scheme.
 *
 * You can override the default normalization schemes, if you want.
 * 
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
IDN_EXPORT idn_result_t
idn_mapper_register(const char *prefix,
		    idn_mapper_createproc_t create,
		    idn_mapper_destroyproc_t destroy,
		    idn_mapper_mapproc_t map);

#ifdef __cplusplus
}
#endif

#endif /* IDN_MAPPER_H */
