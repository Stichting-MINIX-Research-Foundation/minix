/* $Id: api.h,v 1.1.1.1 2003-06-04 00:25:45 marka Exp $ */
/*
 * Copyright (c) 2001,2002 Japan Network Information Center.
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

#ifndef MDN_API_H
#define MDN_API_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <mdn/result.h>
#include <mdn/res.h>
#include <idn/api.h>

#define mdn_enable idn_enable

extern idn_result_t
mdn_nameinit(void);

extern idn_result_t
mdn_encodename(int actions, const char *from, char *to, size_t tolen);

extern idn_result_t
mdn_decodename(int actions, const char *from, char *to, size_t tolen);

#define mdn_localtoutf8(from, to, tolen) \
	mdn_encodename(IDN_LOCALCONV, from, to, len)
#define mdn_delimitermap(from, to, tolen) \
	mdn_encodename(IDN_DELIMMAP, from, to, len)
#define mdn_localmap(from, to, tolen) \
	mdn_encodename(IDN_LOCALMAP, from, to, len)
#define mdn_nameprep(from, to, tolen) \
	mdn_encodename(IDN_NAMEPREP, from, to, len)
#define mdn_utf8toidn(from, to, tolen) \
	mdn_encodename(IDN_IDNCONV, from, to, len)
#define mdn_idntoutf8(from, to, tolen) \
	mdn_decodename(IDN_IDNCONV, from, to, tolen)
#define mdn_utf8tolocal(from, to, tolen) \
	mdn_decodename(IDN_LOCALCONV, from, to, tolen)

#define mdn_localtoidn(from, to, tolen) \
	mdn_encodename(IDN_ENCODE_APP, from, to, tolen)
#define mdn_idntolocal(from, to, tolen) \
	mdn_decodename(IDN_DECODE_APP, from, to, tolen)

#ifdef __cplusplus
}
#endif

#endif /* MDN_API_H */
