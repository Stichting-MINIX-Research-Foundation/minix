/* $Id: unicode.h,v 1.1.1.1 2003-06-04 00:25:43 marka Exp $ */
/*
 * Copyright (c) 2000,2001 Japan Network Information Center.
 * All rights reserved.
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

#ifndef IDN_UNICODE_H
#define IDN_UNICODE_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Unicode attributes retriever.
 *
 * All the information this module provides is based on UnicodeData.txt,
 * CompositionExclusions-1.txt and SpecialCasing.txt, all of which can be
 * obtained from unicode.org.
 *
 * Unicode characters are represented as 'unsigned long'.
 */

#include <idn/result.h>

/*
 * A Handle for Unicode versions.
 */
typedef struct idn__unicode_ops *idn__unicode_version_t;

/*
 * Context information for case conversion.
 */
typedef enum {
	idn__unicode_context_unknown,
	idn__unicode_context_final,
	idn__unicode_context_nonfinal
} idn__unicode_context_t;

/*
 * Create a handle for a specific Unicode version.
 * The version number (such as "3.0.1") is specified by 'version' parameter.
 * If it is NULL, the latest version is used.
 * The handle is stored in '*versionp', which is used various functions
 * in this and unormalize modules.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_notfound		-- specified version not found.
 */
extern idn_result_t
idn__unicode_create(const char *version, idn__unicode_version_t *versionp);

/*
 * Close a handle which was created by 'idn__unicode_create'.
 */
extern void
idn__unicode_destroy(idn__unicode_version_t version);

/*
 * Get canonical class.
 *
 * For characters out of unicode range (i.e. above 0xffff), 0 will
 * be returned.
 */
extern int
idn__unicode_canonicalclass(idn__unicode_version_t version, unsigned long c);

/*
 * Decompose a character.
 *
 * Decompose character given by 'c', and put the result into 'v',
 * which can hold 'vlen' characters.  The number of decomposed characters
 * will be stored in '*decomp_lenp'.
 *
 * If 'compat' is true, compatibility decomposition is performed.
 * Otherwise canonical decomposition is done.
 *
 * Since decomposition is done recursively, no further decomposition
 * will be needed.
 *
 * Returns:
 *	idn_success		-- ok, decomposed.
 *	idn_notfound		-- no decomposition possible.
 *	idn_buffer_overflow	-- 'vlen' is too small.
 */
extern idn_result_t
idn__unicode_decompose(idn__unicode_version_t version,
		       int compat, unsigned long *v, size_t vlen,
		       unsigned long c, int *decomp_lenp);

/*
 * Perform canonical composition.
 *
 * Do canonical composition to the character sequence 'c1' and 'c2', put the
 * result into '*compp'.
 *
 * Since Unicode Nomalization Froms requires only canonical composition,
 * compatibility composition is not supported.
 *
 * Returns:
 *	idn_success		-- ok, composed.
 *	idn_notfound		-- no composition possible.
 */
extern idn_result_t
idn__unicode_compose(idn__unicode_version_t version,
		     unsigned long c1, unsigned long c2, unsigned long *compp);

/*
 * Returns if there may be a canonical composition sequence which starts
 * with the given character.
 *
 * Returns:
 *	1			-- there may be a composition sequence
 *				   (maybe not).
 *	0			-- no, there is definitely no such sequences.
 */
extern int
idn__unicode_iscompositecandidate(idn__unicode_version_t version,
				  unsigned long c);

#ifdef __cplusplus
}
#endif

#endif /* IDN_UNICODE_H */
