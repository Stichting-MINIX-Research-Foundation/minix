/* $Id: filechecker.h,v 1.1.1.1 2003-06-04 00:25:37 marka Exp $ */
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

#ifndef IDN_FILECHECKER_H
#define IDN_FILECHECKER_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Character checker -- check if there are any characters specified
 * by a file in the given string.
 */

#include <idn/result.h>

/*
 * Check object type.
 */
typedef struct idn__filechecker *idn__filechecker_t;

/*
 * Read the contents of the given file and create a context for
 * checking.
 *
 * 'file' is the pathname of the file, which specifies the set of
 * characters to be checked.  The file is a simple text file, and
 * each line must be of the form either
 *   <code_point>
 * or
 *   <code_point>-<code_point>
 * (or comment, see below) where <code_point> is a UCS code point
 * represented as hexadecimal string with optional prefix `U+'
 * (ex. `0041' or `U+FEDC').
 *
 * The former specifies just one character (a code point, to be precise),
 * while the latter specified a range of characters.  In the case of
 * a character range, the first code point (before hyphen) must not be
 * greater than the second code point (after hyphen).
 *
 * Lines starting with `#' are comments.
 *
 * If file is read with no errors, the created context is stored in
 * '*ctxp', and 'idn_success' is returned.  Otherwise, the contents
 * of '*ctxp' is undefined.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_nofile		-- cannot open the specified file.
 *	idn_nomemory		-- malloc failed.
 *	idn_invalid_syntax	-- file format is not valid.
 */
extern idn_result_t
idn__filechecker_create(const char *file, idn__filechecker_t *ctxp);

/*
 * Release memory for the specified context.
 */
extern void
idn__filechecker_destroy(idn__filechecker_t ctx);

/*
 * See if the given string contains any specified characters.
 *
 * Check if there is any characters pecified by the context 'ctx' in
 * the UCS4 string 'str'.  If there are none, NULL is stored in '*found'.
 * Otherwise, the pointer to the first occurence of such character is
 * stored in '*found'.
 *
 * Returns:
 *	idn_success		-- ok.
 */
extern idn_result_t
idn__filechecker_lookup(idn__filechecker_t ctx, const unsigned long *str,
			const unsigned long **found);

/*
 * The following functions are for internal use.
 * They are used for this module to be add to the checker module.
 */
extern idn_result_t
idn__filechecker_createproc(const char *parameter, void **ctxp);

extern void
idn__filechecker_destroyproc(void *ctxp);

extern idn_result_t
idn__filechecker_lookupproc(void *ctx, const unsigned long *str,
			    const unsigned long **found);

#ifdef __cplusplus
}
#endif

#endif /* IDN_FILECHECKER_H */
