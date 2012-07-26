/* $Id: resconf.h,v 1.1.1.1 2003-06-04 00:25:46 marka Exp $ */
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

#ifndef MDN_RESCONF_H
#define MDN_RESCONF_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <mdn/result.h>
#include <idn/resconf.h>

#define mdn_resconf_t \
	idn_resconf_t

#define mdn_resconf_initialize \
	idn_resconf_initialize
#define mdn_resconf_create \
	idn_resconf_create
#define mdn_resconf_destroy \
	idn_resconf_destroy

#define mdn_resconf_incrref \
	idn_resconf_incrref
#define mdn_resconf_loadfile \
	idn_resconf_loadfile
#define mdn_resconf_defaultfile \
	idn_resconf_defaultfile
#define mdn_resconf_getdelimitermap \
	idn_resconf_getdelimitermap
#define mdn_resconf_getidnconverter \
	idn_resconf_getidnconverter
#define mdn_resconf_getlocalconverter \
	idn_resconf_getlocalconverter
#define mdn_resconf_getlocalmapselector \
	idn_resconf_getlocalmapselector
#define mdn_resconf_getmapper \
	idn_resconf_getmapper
#define mdn_resconf_getnormalizer \
	idn_resconf_getnormalizer
#define mdn_resconf_getprohibitchecker \
	idn_resconf_getprohibitchecker
#define mdn_resconf_getunassignedchecker \
	idn_resconf_getunassignedchecker
#define mdn_resconf_setdelimitermap \
	idn_resconf_setdelimitermap
#define mdn_resconf_setidnconverter \
	idn_resconf_setidnconverter
#define mdn_resconf_setlocalconverter \
	idn_resconf_setlocalconverter
#define mdn_resconf_setlocalmapselector \
	idn_resconf_setlocalmapselector
#define mdn_resconf_setmapper \
	idn_resconf_setmapper
#define mdn_resconf_setnormalizer \
	idn_resconf_setnormalizer
#define mdn_resconf_setprohibitchecker \
	idn_resconf_setprohibitchecker
#define mdn_resconf_setunassignedchecker \
	idn_resconf_setunassignedchecker
#define mdn_resconf_setidnconvertername	\
	idn_resconf_setidnconvertername
#define mdn_resconf_addalldelimitermapucs \
	idn_resconf_addalldelimitermapucs
#define mdn_resconf_setlocalconvertername \
	idn_resconf_setlocalconvertername
#define mdn_resconf_addalllocalmapselectornames	\
	idn_resconf_addalllocalmapselectornames
#define mdn_resconf_addallmappernames \
	idn_resconf_addallmappernames
#define mdn_resconf_addallnormalizernames \
	idn_resconf_addallnormalizernames
#define mdn_resconf_addallprohibitcheckernames \
	idn_resconf_addallprohibitcheckernames
#define mdn_resconf_addallunassignedcheckernames \
	idn_resconf_addallunassignedcheckernames
#define mdn_resconf_setnameprepversion \
	idn_resconf_setnameprepversion
#define mdn_resconf_setalternateconverter \
	idn_resconf_setalternateconverter
#define mdn_resconf_setalternateconvertername \
	idn_resconf_setalternateconvertername
#define mdn_resconf_getalternateconverter \
	idn_resconf_getalternateconverter

#define mdn_resconf_localconverter \
	idn_resconf_localconverter
#define mdn_resconf_idnconverter \
	idn_resconf_idnconverter
#define mdn_resconf_alternateconverter \
	idn_resconf_alternateconverter
#define mdn_resconf_normalizer \
	idn_resconf_normalizer
#define mdn_resconf_mapper \
	idn_resconf_mapper
#define mdn_resconf_delimitermap \
	idn_resconf_delimitermap
#define mdn_resconf_localmapselector \
	idn_resconf_localmapselector
#define mdn_resconf_prohibitchecker \
	idn_resconf_prohibitchecker
#define mdn_resconf_unassignedchecker \
	idn_resconf_unassignedchecker

#ifdef __cplusplus
}
#endif

#endif /* MDN_RESCONF_H */
