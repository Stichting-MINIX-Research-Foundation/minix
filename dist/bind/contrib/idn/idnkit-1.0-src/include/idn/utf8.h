/* $Id: utf8.h,v 1.1.1.1 2003-06-04 00:25:44 marka Exp $ */
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

#ifndef IDN_UTF8_H
#define IDN_UTF8_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UTF-8 encoded string facility.
 */

#include <idn/export.h>

/*
 * Get the length of a character.
 *
 * Get the length (in bytes) of a character whose first byte is pointed
 * by 's'.  Since this function only looks one first byte to determine the
 * length, it is possible some of the following bytes are invalid.
 */
IDN_EXPORT int
idn_utf8_mblen(const char *s);

/*
 * Get one character.
 *
 * Get the first character of the string pointed by 's', and copy it
 * to 'buf', whose length is 'len'.  Returns the number of bytes copied,
 * or zero if the encoding is invalid or len is too small.
 *
 * The area pointed by 'buf' must be large enough to store any UTF-8 encoded
 * character.
 *
 * Note that the copied string is not NUL-terminated.
 */
IDN_EXPORT int
idn_utf8_getmb(const char *s, size_t len, char *buf);

/*
 * Get one character in UCS-4.
 *
 * Similar to 'idn_utf8_getmb', except that the result is not in UTF-8
 * encoding, but in UCS-4 format (plain 32bit integer value).
 */
IDN_EXPORT int
idn_utf8_getwc(const char *s, size_t len, unsigned long *vp);

/*
 * Put one character.
 *
 * This function is an opposite of 'idn_utf8_getwc'.  It takes a UCS-4
 * value 'v', convert it to UTF-8 encoded sequence, and store it to 's',
 * whose length is 'len'.  It returns the number of bytes written, or
 * zero 'v' is out of range or 'len' is too small.
 */
IDN_EXPORT int
idn_utf8_putwc(char *s, size_t len, unsigned long v);

/*
 * Check the validity of UTF-8 encoded character.
 *
 * Check if the character pointed by 's' is a valid UTF-8 encoded
 * character.  Return the length of the character (in bytes) if it is valid,
 * 0 otherwise.
 */
IDN_EXPORT int
idn_utf8_isvalidchar(const char *s);

/*
 * Check the validity of UTF-8 encoded string.
 *
 * Check if the NUL-terminated string 's' is valid as a UTF-8 encoded
 * string.  Return 1 if it is valid, 0 otherwise.
 */
IDN_EXPORT int
idn_utf8_isvalidstring(const char *s);

/*
 * Find first byte of a character.
 *
 * Find the first byte of a character 's' points to.  's' may point
 * the 2nd or later byte of a character.  'known_top' is a pointer to
 * a string which contains 's', and is known to be the first byte of
 * a character.  If it couldn't find the first byte between 'known_top'
 * and 's', NULL will be returned.
 */
IDN_EXPORT char *
idn_utf8_findfirstbyte(const char *s, const char *known_top);

#ifdef __cplusplus
}
#endif

#endif /* IDN_UTF8_H */
