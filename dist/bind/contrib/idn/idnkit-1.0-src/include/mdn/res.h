/* $Id: res.h,v 1.1.1.1 2003-06-04 00:25:45 marka Exp $ */
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

#ifndef MDN_RES_H
#define MDN_RES_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <mdn/resconf.h>
#include <mdn/result.h>
#include <idn/res.h>

#define MDN_LOCALCONV \
    	IDN_LOCALCONV
#define MDN_DELIMMAP \
	IDN_DELIMMAP
#define MDN_LOCALMAP \
	IDN_LOCALMAP
#define MDN_MAP \
	IDN_MAP
#define MDN_NORMALIZE \
	IDN_NORMALIZE
#define MDN_PROHCHECK \
	IDN_PROHCHECK
#define MDN_UNASCHECK \
	IDN_UNASCHECK
#define MDN_ASCCHECK \
	IDN_ASCCHECK
#define MDN_IDNCONV \
	IDN_IDNCONV
#define MDN_LENCHECK \
	IDN_LENCHECK
#define MDN_RTCHECK \
	IDN_RTCHECK
#define MDN_UNDOIFERR \
	IDN_UNDOIFERR
#define MDN_ENCODE_APP \
	IDN_ENCODE_APP
#define MDN_DECODE_APP \
	IDN_DECODE_APP
#define MDN_NAMEPREP \
	IDN_NAMEPREP

#define mdn_res_enable \
	idn_res_enable
#define mdn_res_encodename \
	idn_res_encodename
#define mdn_res_decodename \
	idn_res_decodename
#define mdn_res_actiontostring \
	idn_res_actiontostring

#define mdn_res_localtoutf8 \
	idn_res_localtoutf8
#define mdn_res_delimitermap \
	idn_res_delimitermap
#define mdn_res_localmap \
	idn_res_localmap
#define mdn_res_nameprep \
	idn_res_nameprep
#define mdn_res_utf8toidn \
	idn_res_utf8toidn
#define mdn_res_idntoutf8 \
	idn_res_idntoutf8
#define mdn_res_utf8tolocal \
	idn_res_utf8tolocal
#define mdn_res_nameprepcheck \
	idn_res_nameprepcheck
#define mdn_res_localtoidn \
	idn_res_localtoidn
#define mdn_res_idntolocal \
	idn_res_idntolocal

#ifdef __cplusplus
}
#endif

#endif /* MDN_RES_H */
