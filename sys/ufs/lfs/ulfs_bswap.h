/*	$NetBSD: ulfs_bswap.h,v 1.6 2013/10/18 15:15:22 christos Exp $	*/
/*  from NetBSD: ufs_bswap.h,v 1.19 2009/10/19 18:41:17 bouyer Exp  */

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _UFS_LFS_ULFS_BSWAP_H_
#define _UFS_LFS_ULFS_BSWAP_H_

#if defined(_KERNEL_OPT)
#include "opt_lfs.h"
#endif

#include <sys/bswap.h>

/* Macros to access ULFS flags */
#ifdef LFS_EI
#define	ULFS_MPNEEDSWAP(lfs)	((lfs)->um_flags & ULFS_NEEDSWAP)
#define ULFS_FSNEEDSWAP(fs)	((fs)->fs_flags & FS_SWAPPED)
#define	ULFS_IPNEEDSWAP(ip)	ULFS_MPNEEDSWAP((ip)->i_lfs)
#else
#define	ULFS_MPNEEDSWAP(ump)	(__USE(ump), 0)
#define ULFS_FSNEEDSWAP(fs)	(__USE(fs), 0)
#define	ULFS_IPNEEDSWAP(ip)	(__USE(ip), 0)
#endif

#if !defined(_KERNEL) || defined(LFS_EI)
/* inlines for access to swapped data */
static inline u_int16_t
ulfs_rw16(uint16_t a, int ns)
{
	return ((ns) ? bswap16(a) : (a));
}

static inline u_int32_t
ulfs_rw32(uint32_t a, int ns)
{
	return ((ns) ? bswap32(a) : (a));
}

static inline u_int64_t
ulfs_rw64(uint64_t a, int ns)
{
	return ((ns) ? bswap64(a) : (a));
}
#else
#define ulfs_rw16(a, ns) (__USE(ns), (uint16_t)(a))
#define ulfs_rw32(a, ns) (__USE(ns), (uint32_t)(a))
#define ulfs_rw64(a, ns) (__USE(ns), (uint64_t)(a))
#endif

#define ulfs_add16(a, b, ns) \
	(a) = ulfs_rw16(ulfs_rw16((a), (ns)) + (b), (ns))
#define ulfs_add32(a, b, ns) \
	(a) = ulfs_rw32(ulfs_rw32((a), (ns)) + (b), (ns))
#define ulfs_add64(a, b, ns) \
	(a) = ulfs_rw64(ulfs_rw64((a), (ns)) + (b), (ns))

#endif /* !_UFS_LFS_ULFS_BSWAP_H_ */
