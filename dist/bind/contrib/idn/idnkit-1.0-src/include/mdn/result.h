/* $Id: result.h,v 1.1.1.1 2003-06-04 00:25:46 marka Exp $ */
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

#ifndef MDN_RESULT_H
#define MDN_RESULT_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <idn/result.h>

#define mdn_result_t \
	idn_result_t

#define mdn_success \
	idn_success
#define mdn_notfound \
	idn_notfound
#define mdn_invalid_encoding \
	idn_invalid_encoding
#define mdn_invalid_syntax \
	idn_invalid_syntax
#define mdn_invalid_name \
	idn_invalid_name
#define mdn_invalid_message \
	idn_invalid_message
#define mdn_invalid_action \
	idn_invalid_action
#define mdn_invalid_codepoint \
	idn_invalid_codepoint
#define mdn_invalid_length \
	idn_invalid_length
#define mdn_buffer_overflow \
	idn_buffer_overflow
#define mdn_noentry \
	idn_noentry
#define mdn_nomemory \
	idn_nomemory
#define mdn_nofile \
	idn_nofile
#define mdn_nomapping \
	idn_nomapping
#define mdn_context_required \
	idn_context_required
#define mdn_prohibited \
	idn_prohibited
#define mdn_failure \
	idn_failure
#define mdn_result_tostring \
	idn_result_tostring

#ifdef __cplusplus
}
#endif

#endif /* MDN_RESULT_H */
