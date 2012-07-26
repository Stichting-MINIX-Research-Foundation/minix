/* $Id: util.h,v 1.1.1.1 2003-06-04 00:25:44 marka Exp $ */
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

#ifndef IDN_UTIL_H
#define IDN_UTIL_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Utility functions.
 */

/*
 * Check ACE prefix.
 *
 * These functions examine whether `str' begins with `prefix'.
 * They disregard the case difference of ASCII letters ([A-Za-z]).
 * They return 1 if `str' has the ACE prefix, 0 otherwise.
 */
extern int
idn__util_asciihaveaceprefix(const char *str, const char *prefix);
extern int
idn__util_ucs4haveaceprefix(const unsigned long *str, const char *prefix);

/*
 * Check if all codepoints in the UCS4 string `str' are in the ASCII
 * range (i.e. U+0000...U+007F).
 *
 * The function return 1 if it is, 0 otherwise.
 */
extern int
idn__util_ucs4isasciirange(const unsigned long *str);

/*
 * Get registry information from the system. (Windows only)
 */
#ifdef WIN32
/*
 * registry top type.
 */
typedef enum {
	idn__util_hkey_currentuser,
	idn__util_hkey_localmachine
} idn__util_hkey_t;

extern int
idn__util_getregistrystring(idn__util_hkey_t topkey, const char *name,
			    char *str, size_t length);
#endif /* WIN32 */

#ifdef __cplusplus
}
#endif

#endif /* IDN_UTIL_H */
