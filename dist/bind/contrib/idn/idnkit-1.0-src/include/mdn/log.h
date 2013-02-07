/* $Id: log.h,v 1.1.1.1 2003-06-04 00:25:45 marka Exp $ */
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

#ifndef MDN_LOG_H
#define MDN_LOG_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <idn/log.h>

#define mdn_log_proc_t \
	idn_log_proc_t

#define mdn_log_level_fatal \
	idn_log_level_fatal
#define mdn_log_level_error \
	idn_log_level_error
#define mdn_log_level_warning \
	idn_log_level_warning
#define mdn_log_level_info \
	idn_log_level_info
#define mdn_log_level_trace \
	idn_log_level_trace
#define mdn_log_level_dump \
	idn_log_level_dump

#define mdn_log_fatal \
	idn_log_fatal
#define mdn_log_error \
	idn_log_error
#define mdn_log_warning \
	idn_log_warning
#define mdn_log_info \
	idn_log_info
#define mdn_log_trace \
	idn_log_trace
#define mdn_log_dump \
	idn_log_dump
#define mdn_log_setlevel \
	idn_log_setlevel
#define mdn_log_getlevel \
	idn_log_getlevel
#define mdn_log_setproc \
	idn_log_setproc

#ifdef __cplusplus
}
#endif

#endif /* MDN_LOG_H */
