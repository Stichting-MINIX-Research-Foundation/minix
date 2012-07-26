/* $Id: aliaslist.h,v 1.1.1.1 2003-06-04 00:25:34 marka Exp $ */
/*
 * Copyright (c) 2002 Japan Network Information Center.  All rights reserved.
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

#ifndef IDN_ALIASLIST_H
#define IDN_ALIASLIST_H 1

#include <idn/result.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct idn__aliaslist *idn__aliaslist_t;

/*
 * Create a list.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
extern idn_result_t
idn__aliaslist_create(idn__aliaslist_t *listp);

/*
 * Delete a list created by 'idn__aliaslist_create'.
 */
extern void
idn__aliaslist_destroy(idn__aliaslist_t list);

/*
 * Parse alias information file and set items to the list.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nofile		-- no such file.
 *	idn_invalid_syntax	-- file is malformed.
 *	idn_nomemory		-- malloc failed.
 */
extern idn_result_t
idn__aliaslist_aliasfile(idn__aliaslist_t list, const char *path);

/*
 * Add an item to the list.
 *
 * If top is 0, item is placed as the last item of the alias list.
 * Otherwise, it is done as the first item.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
extern idn_result_t
idn__aliaslist_additem(idn__aliaslist_t list,
		       const char *pattern, const char *encoding,
		       int first_item);

/*
 * Find the encoding name with the specified pattern by wildcard
 * match.
 *
 * Returns:
 *	idn_success		-- ok. found.
 *	idn_noentry		-- not found.
 */
extern idn_result_t
idn__aliaslist_find(idn__aliaslist_t list,
		   const char *pattern, char **encodingp);

#ifdef __cplusplus
}
#endif

#endif /* IDN_ALIASLIST_H */
