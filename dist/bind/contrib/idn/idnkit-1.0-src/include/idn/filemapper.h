/* $Id: filemapper.h,v 1.1.1.1 2003-06-04 00:25:38 marka Exp $ */
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

#ifndef IDN_FILEMAPPER_H
#define IDN_FILEMAPPER_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Perform character mapping (substitution) according to a
 * map file.
 */

#include <idn/result.h>

/*
 * Mapping object type.
 */
typedef struct idn__filemapper *idn__filemapper_t;

/*
 * Read the contents of the given map file and create a context for mapping.
 *
 * 'file' is the pathname of the file, which specifies the character
 * mapping.  The file is a simple text file, and each line specifies
 * a mapping of a single character.  The format of each line is
 *
 *   <code_point>; [<code_point>..][;]
 *
 * where <code_point> is a UCS code point represented as a hexadecimal
 * string with optional prefix `U+' (ex. `0041' or `U+FEDC').
 * The code point before the first semicolon will be mapped to the
 * sequence of code points separated by space characters after the
 * first semicolon.  The sequence may be empty, denoting wiping out
 * the character.
 *
 * For example,
 *	U+0041; U+0061		-- maps 'A' to 'a'
 *	20;;			-- wipes out ' '
 *	
 * Anything after the second semicolon is ignored.  Also lines beginning
 * with '#' are treated as comments.
 *
 * If there is no error, the created context is stored in '*ctxp'.
 * 
 * Returns:
 *	idn_success		-- ok.
 *	idn_nofile		-- cannot open the specified file.
 *	idn_nomemory		-- malloc failed.
 *	idn_invalid_syntax	-- file format is not valid.
 */
extern idn_result_t
idn__filemapper_create(const char *file, idn__filemapper_t *ctxp);

/*
 * Release memory for the given context.
 */
extern void
idn__filemapper_destroy(idn__filemapper_t ctx);

/*
 * Perform character substitution.
 *
 * Each character in the string 'from' is examined and if it
 * has a mapping, it is substituted to the corresponding
 * character sequence.  The substituted string is stored in 'to',
 * whose length is specified by 'tolen'.
 *
 * Returns:
 *	idn_success		-- ok.
 *	idn_buffer_overflow	-- result buffer is too small.
 */
extern idn_result_t
idn__filemapper_map(idn__filemapper_t ctx, const unsigned long *from,
		    unsigned long *to, size_t tolen);

/*
 * The following functions are for internal use.
 * They are used for this module to be add to the mapper module.
 */
extern idn_result_t
idn__filemapper_createproc(const char *parameter, void **ctxp);

extern void
idn__filemapper_destroyproc(void *ctxp);

extern idn_result_t
idn__filemapper_mapproc(void *ctx, const unsigned long *from,
			unsigned long *to, size_t tolen);

#ifdef __cplusplus
}
#endif

#endif /* IDN_FILEMAPPER_H */
