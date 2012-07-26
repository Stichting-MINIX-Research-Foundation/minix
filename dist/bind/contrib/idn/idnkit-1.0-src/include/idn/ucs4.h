/* $Id: ucs4.h,v 1.1.1.1 2003-06-04 00:25:42 marka Exp $ */
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

#ifndef IDN_UCS4_H
#define IDN_UCS4_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UCS4 encoded string facility.
 */

#include <idn/export.h>
#include <idn/result.h>

/*
 * UCS4 to UTF-16 conversion and vice versa.
 */
IDN_EXPORT idn_result_t
idn_ucs4_ucs4toutf16(const unsigned long *ucs4, unsigned short *utf16,
		     size_t tolen);

IDN_EXPORT idn_result_t
idn_ucs4_utf16toucs4(const unsigned short *utf16, unsigned long *ucs4,
		     size_t tolen);

/*
 * UCS4 to UTF-8 conversion and vice versa.
 */
IDN_EXPORT idn_result_t
idn_ucs4_utf8toucs4(const char *utf8, unsigned long *ucs4, size_t tolen);

IDN_EXPORT idn_result_t
idn_ucs4_ucs4toutf8(const unsigned long *ucs4, char *utf8, size_t tolen);

/*
 * UCS4 version of string operation functions.
 */
IDN_EXPORT size_t
idn_ucs4_strlen(const unsigned long *ucs4);

IDN_EXPORT unsigned long *
idn_ucs4_strcpy(unsigned long *to, const unsigned long *from);

IDN_EXPORT unsigned long *
idn_ucs4_strcat(unsigned long *to, const unsigned long *from);

IDN_EXPORT int
idn_ucs4_strcmp(const unsigned long *str1, const unsigned long *str2);

IDN_EXPORT int
idn_ucs4_strcasecmp(const unsigned long *str1, const unsigned long *str2);

IDN_EXPORT unsigned long *
idn_ucs4_strdup(const unsigned long *str);

#ifdef __cplusplus
}
#endif

#endif /* IDN_UCS4_H */
