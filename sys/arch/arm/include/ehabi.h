/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
/* $NetBSD: ehabi.h,v 1.1 2013/08/12 23:22:12 matt Exp $ */

#ifndef _ARM_EHABI_H_
#define	_ARM_EHABI_H_

#if defined(_KERNEL) || defined(_STANDALONE)
#include <sys/types.h>
#else
#include <inttypes.h>
#endif

typedef enum {
	_URC_OK = 0,			/* operation complete */
	_URC_FOREIGN_EXCEPTION_CAUGHT = 1,
	_URC_HANDLER_FOUND = 6,
	_URC_INSTALL_CONTEXT = 7,
	_URC_CONTINUE_UNWIND = 8,
	_URC_FAILURE = 9,		/* unspecified failure */
} _Unwind_Reason_Code;

typedef enum {
	_UVRSC_CORE = 0,	/* integer register */
	_UVRSC_VFP = 1,		/* vfp */
	_UVRSC_WMMXD = 3,	/* Intel WMMX data register */
	_UVRSC_WMMXC = 4	/* Intel WMMX control register */
} _Unwind_VRS_RegClass;
typedef enum {
	_UVRSD_UINT32 = 0,
	_UVRSD_VFPX = 1,
	_UVRSD_UINT64 = 3,
	_UVRSD_FLOAT = 4,
	_UVRSD_DOUBLE = 5
} _Unwind_VRS_DataRepresentation;
typedef enum {
	_UVRSR_OK = 0,
	_UVRSR_NOT_IMPLEMENTED = 1,
	_UVRSR_FAILED = 2
} _Unwind_VRS_Result;

typedef uint32_t _Unwind_State;
static const _Unwind_State _US_VIRTUAL_UNWIND_FRAME  = 0;
static const _Unwind_State _US_UNWIND_FRAME_STARTING = 1;
static const _Unwind_State _US_UNWIND_FRAME_RESUME   = 2;

typedef struct _Unwind_Control_Block _Unwind_Control_Block;
typedef struct _Unwind_Context _Unwind_Context;
typedef uint32_t _Unwind_EHT_Header;

struct _Unwind_Control_Block {
	char exception_class[8];
	void (*exception_cleanup)(_Unwind_Reason_Code, _Unwind_Control_Block *);
	/* Unwinder cache, private fields for the unwinder's use */
	struct {
		uint32_t reserved1;
		uint32_t reserved2;
		uint32_t reserved3;
		uint32_t reserved4;
		uint32_t reserved5;
		/* init reserved1 to 0, then don't touch */
	} unwinder_cache;
	/* Propagation barrier cache (valid after phase 1): */
	struct {
		uint32_t sp;
		  uint32_t bitpattern[5];
	} barrier_cache;
	/* Cleanup cache (preserved over cleanup): */
	struct {
		uint32_t bitpattern[4];
	} cleanup_cache;
	/* Pr cache (for pr's benefit): */
	struct {
		uint32_t fnstart;		/* function start address */
		_Unwind_EHT_Header *ehtp; /* ptr to EHT entry header word */
		uint32_t additional;		/* additional data */
		uint32_t reserved1;
	} pr_cache;
	uint64_t : 0; /* Force alignment of next item to 8-byte boundary */
};

__BEGIN_DECLS

/* Unwinding functions */
void			_Unwind_Resume(_Unwind_Control_Block *);
void			_Unwind_Complete(_Unwind_Control_Block *);
void			_Unwind_DeleteException(_Unwind_Control_Block *);
_Unwind_Reason_Code	_Unwind_RaiseException(_Unwind_Control_Block *);

_Unwind_VRS_Result	_Unwind_VRS_Set(_Unwind_Context *, _Unwind_VRS_RegClass,
			    uint32_t, _Unwind_VRS_DataRepresentation, void *);
_Unwind_VRS_Result	_Unwind_VRS_Get(_Unwind_Context *, _Unwind_VRS_RegClass,
			    uint32_t, _Unwind_VRS_DataRepresentation, void *);

_Unwind_VRS_Result	_Unwind_VRS_Pop(_Unwind_Context *, _Unwind_VRS_RegClass,
			    uint32_t, _Unwind_VRS_DataRepresentation);

_Unwind_Reason_Code	__aeabi_unwind_cpp_pr0(_Unwind_State,
			    _Unwind_Control_Block *, _Unwind_Context *);
_Unwind_Reason_Code	__aeabi_unwind_cpp_pr1(_Unwind_State ,
			    _Unwind_Control_Block *, _Unwind_Context *);
_Unwind_Reason_Code	__aeabi_unwind_cpp_pr2(_Unwind_State ,
			    _Unwind_Control_Block *, _Unwind_Context *);

__END_DECLS

#endif /* _ARM_EHABI_H_ */
