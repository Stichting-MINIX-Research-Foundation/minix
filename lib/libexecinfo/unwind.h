/*	$NetBSD: unwind.h,v 1.3 2014/10/22 16:30:21 christos Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _UNWIND_H_
#define _UNWIND_H_

#include <sys/cdefs.h>

__BEGIN_DECLS
struct _Unwind_Context;
struct _Unwind_Exception;
typedef int _Unwind_Reason_Code;
typedef void *_Unwind_Ptr;
typedef long _Unwind_Word;

#define	_URC_NO_REASON			0
#define	_URC_FOREIGN_EXCEPTION_CAUGHT	1
#define	_URC_FATAL_PHASE2_ERROR		2
#define	_URC_FATAL_PHASE1_ERROR		3
#define	_URC_NORMAL_STOP		4
#define	_URC_END_OF_STACK		5
#define	_URC_HANDLER_FOUND		6
#define	_URC_INSTALL_CONTEXT		7
#define	_URC_CONTINUE_UNWIND		8

typedef _Unwind_Reason_Code
    (*_Unwind_Trace_Fn)(struct _Unwind_Context *, void *);
#ifdef notyet
typedef _Unwind_Reason_Code
    (*_Unwind_Stop_Fn)(struct _Unwind_Context *, void *);
#endif

_Unwind_Reason_Code	 _Unwind_Backtrace(_Unwind_Trace_Fn, void *);
void 			 _Unwind_DeleteException(struct _Unwind_Exception *);
void 	       		*_Unwind_FindEnclosingFunction(void *);
#ifdef notyet
_Unwind_Reason_Code 	 _Unwind_ForcedUnwind(struct _Unwind_Exception *,
    _Unwind_Stop_fn, void *);
#endif
_Unwind_Word		 _Unwind_GetCFA(struct _Unwind_Context *);
_Unwind_Ptr		 _Unwind_GetDataRelBase(struct _Unwind_Context *);
_Unwind_Word 		 _Unwind_GetGR(struct _Unwind_Context *, int);
_Unwind_Ptr		 _Unwind_GetIP(struct _Unwind_Context *);
_Unwind_Ptr		 _Unwind_GetIPInfo(struct _Unwind_Context *, int *);
_Unwind_Ptr		 _Unwind_GetLanguageSpecificData(
    struct _Unwind_Context *);
_Unwind_Ptr		 _Unwind_GetRegionStart(struct _Unwind_Context *);
_Unwind_Ptr		 _Unwind_GetTextRelBase(struct _Unwind_Context *);
_Unwind_Reason_Code	 _Unwind_RaiseException(struct _Unwind_Exception *);
void			 _Unwind_Resume(struct _Unwind_Exception *);
_Unwind_Reason_Code	 _Unwind_Resume_or_Rethrow(struct _Unwind_Exception *);
void			 _Unwind_SetGR(struct _Unwind_Context *, int,
    _Unwind_Ptr);
void			 _Unwind_SetIP(struct _Unwind_Context *, _Unwind_Ptr);
__END_DECLS
#endif /* _UNWIND_H_ */
