/* $Id: strhash.h,v 1.1.1.1 2003-06-04 00:25:42 marka Exp $ */
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

#ifndef IDN_STRHASH_H
#define IDN_STRHASH_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * String-keyed hash table
 *
 * Just a hash table.  Nothing special.  Number of hash buckets
 * grows automatically.
 */

#include <idn/result.h>

/*
 * Hash table type, which is opaque.
 */
typedef struct idn__strhash *idn__strhash_t;

/*
 * Hash value free proc.
 */
typedef void (*idn__strhash_freeproc_t)(void *value);

/*
 * Create a hash table.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
extern idn_result_t
idn__strhash_create(idn__strhash_t *hashp);

/*
 * Delete a hash table created by 'idn__strhash_create'.
 * If 'proc' is not NULL, it is called for each value in the
 * hash to release memory for them.
 */
extern void
idn__strhash_destroy(idn__strhash_t hash, idn__strhash_freeproc_t proc);

/*
 * Register an item to the hash table.  This function makes a
 * private copy of the key string.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nomemory		-- malloc failed.
 */
extern idn_result_t
idn__strhash_put(idn__strhash_t hash, const char *key, void *value);

/*
 * Find an item with the specified key.
 *
 * Returns:
 *	idn_success		-- ok. found.
 *	idn_noentry		-- not found.
 */
extern idn_result_t
idn__strhash_get(idn__strhash_t hash, const char *key, void **valuep);

/*
 * Check if an item with the specified key exists.
 *
 * Returns:
 *	1			-- yes.
 *	0			-- no.
 */
extern int
idn__strhash_exists(idn__strhash_t hash, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* IDN_STRHASH_H */
