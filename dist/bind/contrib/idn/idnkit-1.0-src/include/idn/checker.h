/* $Id: checker.h,v 1.1.1.1 2003-06-04 00:25:36 marka Exp $ */
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

#ifndef IDN_CHECKER_H
#define IDN_CHECKER_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Character Checker.
 *
 * Perfom checking characters in the specified domain name.
 */

#include <idn/result.h>
#include <idn/filechecker.h>
#include <idn/nameprep.h>

/*
 * Schems name prefixes for the standard nameprep prohibit/unassigned
 * checks.
 *
 * If you'd like to add the unassigned check scheme of "RFC3491"
 * to a checker context, IDN_CHECKER_UNASSIGNED_PREFIX + "RFC3491"
 * (i.e. "unassigned#RFC3491") is the scheme name passed to
 * idn_checker_add().
 */
#define IDN_CHECKER_PROHIBIT_PREFIX	"prohibit#"
#define IDN_CHECKER_UNASSIGNED_PREFIX	"unassigned#"
#define IDN_CHECKER_BIDI_PREFIX		"bidi#"

/*
 * Checker object type.
 */
typedef struct idn_checker *idn_checker_t;

/*
 * Initialize module.  Must be called before any other calls of
 * the functions of this module.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
extern idn_result_t
idn_checker_initialize(void);

/*
 * Create a checker context.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
extern idn_result_t
idn_checker_create(idn_checker_t *ctxp);

/*
 * Decrement reference count of the checker `ctx' created by
 * 'idn_checker_create', if it is still refered by another object.
 * Otherwise, release all the memory allocated to the checker.
 */
extern void
idn_checker_destroy(idn_checker_t ctx);

/*
 * Increment reference count of the checker `ctx' created by
 * 'idn_checker_create'.
 */
extern void
idn_checker_incrref(idn_checker_t ctx);

/*
 * Add checking scheme `name' to the checker to `ctx'.
 * 
 * Returns:
 *      idn_success             -- ok.
 *      idn_invalid_name        -- the given name is not valid.
 *      idn_nomemory            -- malloc failed.
 */
extern idn_result_t
idn_checker_add(idn_checker_t ctx, const char *name);

extern idn_result_t
idn_checker_addall(idn_checker_t ctx, const char **names, int nnames);

/*
 * Check a domain name.  All checking schemes regsitered in `ctx' are
 * applied in the regisration order.
 *
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 *      idn_buffer_overflow     -- output buffer is too small.
 */
extern idn_result_t
idn_checker_lookup(idn_checker_t ctx, const unsigned long *ucs4,
		   const unsigned long **found);

/*
 * Checking procedure type.
 */
typedef idn_result_t (*idn_checker_createproc_t)(const char *parameter,
						 void **ctxp);
typedef void         (*idn_checker_destroyproc_t)(void *ctx);
typedef idn_result_t (*idn_checker_lookupproc_t)(void *ctx,
						 const unsigned long *ucs4,
                                                 const unsigned long **found);
                                              
/*
 * Register a new checking scheme.
 *
 * You can override the default normalization schemes, if you want.
 * 
 * Returns:
 *      idn_success             -- ok.
 *      idn_nomemory            -- malloc failed.
 */
extern idn_result_t
idn_checker_register(const char *prefix,
		     idn_checker_createproc_t create,
		     idn_checker_destroyproc_t destroy,
		     idn_checker_lookupproc_t lookup);

#ifdef __cplusplus
}
#endif

#endif /* IDN_CHECKER_H */
