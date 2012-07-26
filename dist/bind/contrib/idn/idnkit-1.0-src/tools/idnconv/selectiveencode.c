#ifndef lint
static char *rcsid = "$Id: selectiveencode.c,v 1.1.1.1 2003-06-04 00:27:07 marka Exp $";
#endif

/*
 * Copyright (c) 2000,2002 Japan Network Information Center.
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

#include <config.h>

#include <stddef.h>

#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/result.h>
#include <idn/utf8.h>
#include <idn/debug.h>

#include "selectiveencode.h"

static int	is_domain_delimiter(char c);
static char	*find_nonascii(const char *s);

idn_result_t
idn_selectiveencode_findregion(const char *s,
			       char **startp, char **endp)
{
	char *non_ascii;
	char *start, *end;

	assert(s != NULL && startp != NULL && endp != NULL);

	TRACE(("idn_selectiveencode_findregion(s=\"%s\")\n",
	      idn__debug_xstring(s, 20)));

	/*
	 * Scan the specified string looking for non-ascii character.
	 */
	if ((non_ascii = find_nonascii(s)) == NULL)
		return (idn_notfound);

	/*
	 * Non-ascii character found.
	 * Determine the region to encode.
	 */
	
	/*
	 * First, we scan backwards to find the beginning of the region
	 * that should be converted.
	 */
	start = non_ascii;
	while (start > s) {
		char *prev = idn_utf8_findfirstbyte(start - 1, s);
		if (is_domain_delimiter(*prev))
			break;			/* Found */
		start = prev;
	}
	*startp = start;

	/*
	 * Next we scan forwards looking for the end of the region.
	 */
	end = non_ascii + idn_utf8_mblen(non_ascii);
	while (!is_domain_delimiter(*end))
		end += idn_utf8_mblen(end);
	*endp = end;

	return (idn_success);
}

static int
is_domain_delimiter(char c) {
	return ((unsigned char)c < 0x80 &&
		!('A' <= c && c <= 'Z') &&
		!('a' <= c && c <= 'z') &&
		!('0' <= c && c <= '9') &&
		c != '-' && c != '.');
}

static char *
find_nonascii(const char *s) {
	while (*s != '\0' && (unsigned char)*s < 0x80)
		s++;
	if (*s == '\0')
		return (NULL);
	else
		return ((char *)s);
}
