/*	$NetBSD: citrus_ctype_local.h,v 1.3 2008/02/09 14:56:20 junyoung Exp $	*/

/*-
 * Copyright (c)2002 Citrus Project,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _CITRUS_CTYPE_LOCAL_H_
#define _CITRUS_CTYPE_LOCAL_H_

#define _CITRUS_CTYPE_GETOPS_FUNC_BASE(_n_)				\
int _n_(_citrus_ctype_ops_rec_t *, size_t, uint32_t)
#define _CITRUS_CTYPE_GETOPS_FUNC(_n_)					\
_CITRUS_CTYPE_GETOPS_FUNC_BASE(_citrus_##_n_##_ctype_getops)

#define _CITRUS_CTYPE_DECLS(_e_)					      \
static int	_citrus_##_e_##_ctype_init				      \
	(void ** __restrict, void * __restrict, size_t, size_t);	      \
static void	_citrus_##_e_##_ctype_uninit(void *);			      \
static unsigned	_citrus_##_e_##_ctype_get_mb_cur_max(void *);		      \
static int	_citrus_##_e_##_ctype_mblen(void * __restrict,		      \
				     const char * __restrict,		      \
				     size_t, int * __restrict);		      \
static int	_citrus_##_e_##_ctype_mbrlen(void * __restrict,		      \
				      const char * __restrict,		      \
				      size_t, void * __restrict,	      \
				      size_t * __restrict);		      \
static int	_citrus_##_e_##_ctype_mbrtowc(void * __restrict,	      \
				       wchar_t * __restrict,		      \
				       const char * __restrict, size_t,	      \
				       void * __restrict,		      \
				       size_t * __restrict);		      \
static int	_citrus_##_e_##_ctype_mbsinit(void * __restrict,	      \
				       void const * __restrict,		      \
				       int * __restrict);		      \
static int	_citrus_##_e_##_ctype_mbsrtowcs(void * __restrict,	      \
					 wchar_t * __restrict,		      \
					 const char ** __restrict,	      \
					 size_t, void * __restrict,	      \
					 size_t * __restrict);		      \
static int	_citrus_##_e_##_ctype_mbstowcs(void * __restrict,	      \
					wchar_t * __restrict,		      \
					const char * __restrict,	      \
					size_t, size_t * __restrict);	      \
static int	_citrus_##_e_##_ctype_mbtowc(void * __restrict,		      \
				      wchar_t * __restrict,		      \
				      const char * __restrict,		      \
				      size_t, int * __restrict);	      \
static int	_citrus_##_e_##_ctype_wcrtomb(void * __restrict,	      \
				       char * __restrict, wchar_t,	      \
				       void * __restrict,		      \
				       size_t * __restrict);		      \
static int	_citrus_##_e_##_ctype_wcsrtombs(void * __restrict,	      \
					 char * __restrict,		      \
					 const wchar_t ** __restrict,	      \
					 size_t, void * __restrict,	      \
					 size_t * __restrict);		      \
static int	_citrus_##_e_##_ctype_wcstombs(void * __restrict,	      \
					char * __restrict,		      \
					const wchar_t * __restrict,	      \
					size_t, size_t * __restrict);	      \
static int	_citrus_##_e_##_ctype_wctomb(void * __restrict,		      \
				      char * __restrict,		      \
				      wchar_t, int * __restrict);	      \
static int	_citrus_##_e_##_ctype_btowc(_citrus_ctype_rec_t * __restrict, \
				      int, wint_t * __restrict);	      \
static int	_citrus_##_e_##_ctype_wctob(_citrus_ctype_rec_t * __restrict, \
				      wint_t, int * __restrict)

#define _CITRUS_CTYPE_DEF_OPS(_e_)					\
_citrus_ctype_ops_rec_t _citrus_##_e_##_ctype_ops = {			\
	/* co_abi_version */	_CITRUS_CTYPE_ABI_VERSION,		\
	/* co_init */		&_citrus_##_e_##_ctype_init,		\
	/* co_uninit */		&_citrus_##_e_##_ctype_uninit,		\
	/* co_get_mb_cur_max */	&_citrus_##_e_##_ctype_get_mb_cur_max,	\
	/* co_mblen */		&_citrus_##_e_##_ctype_mblen,		\
	/* co_mbrlen */		&_citrus_##_e_##_ctype_mbrlen,		\
	/* co_mbrtowc */	&_citrus_##_e_##_ctype_mbrtowc,		\
	/* co_mbsinit */	&_citrus_##_e_##_ctype_mbsinit,		\
	/* co_mbsrtowcs */	&_citrus_##_e_##_ctype_mbsrtowcs,	\
	/* co_mbstowcs */	&_citrus_##_e_##_ctype_mbstowcs,	\
	/* co_mbtowc */		&_citrus_##_e_##_ctype_mbtowc,		\
	/* co_wcrtomb */	&_citrus_##_e_##_ctype_wcrtomb,		\
	/* co_wcsrtombs */	&_citrus_##_e_##_ctype_wcsrtombs,	\
	/* co_wcstombs */	&_citrus_##_e_##_ctype_wcstombs,	\
	/* co_wctomb */		&_citrus_##_e_##_ctype_wctomb,		\
	/* co_btowc */		&_citrus_##_e_##_ctype_btowc,		\
	/* co_wctob */		&_citrus_##_e_##_ctype_wctob		\
}

typedef struct _citrus_ctype_ops_rec	_citrus_ctype_ops_rec_t;
typedef struct _citrus_ctype_rec	_citrus_ctype_rec_t;

typedef int	(*_citrus_ctype_init_t)
	(void ** __restrict, void * __restrict, size_t, size_t);
typedef void	(*_citrus_ctype_uninit_t)(void *);
typedef unsigned (*_citrus_ctype_get_mb_cur_max_t)(void *);
typedef int	(*_citrus_ctype_mblen_t)
	(void * __restrict, const char * __restrict, size_t, int * __restrict);
typedef int	(*_citrus_ctype_mbrlen_t)
	(void * __restrict, const char * __restrict, size_t,
	 void * __restrict, size_t * __restrict);
typedef int	(*_citrus_ctype_mbrtowc_t)
	(void * __restrict, wchar_t * __restrict, const char * __restrict,
	 size_t, void * __restrict, size_t * __restrict);
typedef int	(*_citrus_ctype_mbsinit_t)
	(void * __restrict, const void * __restrict, int * __restrict);
typedef int	(*_citrus_ctype_mbsrtowcs_t)
	(void * __restrict, wchar_t * __restrict, const char ** __restrict,
	 size_t, void * __restrict,
	 size_t * __restrict);
typedef int	(*_citrus_ctype_mbstowcs_t)
	(void * __restrict, wchar_t * __restrict, const char * __restrict,
	 size_t, size_t * __restrict);
typedef int	(*_citrus_ctype_mbtowc_t)
	(void * __restrict, wchar_t * __restrict, const char * __restrict,
	 size_t, int * __restrict);
typedef int	(*_citrus_ctype_wcrtomb_t)
	(void * __restrict, char * __restrict, wchar_t, void * __restrict,
	 size_t * __restrict);
typedef int	(*_citrus_ctype_wcsrtombs_t)
	(void * __restrict, char * __restrict, const wchar_t ** __restrict,
	 size_t, void * __restrict, size_t * __restrict);
typedef int	(*_citrus_ctype_wcstombs_t)
	(void * __restrict, char * __restrict, const wchar_t * __restrict,
	 size_t, size_t * __restrict);
typedef int	(*_citrus_ctype_wctomb_t)
	(void * __restrict, char * __restrict, wchar_t, int * __restrict);
typedef int	(*_citrus_ctype_btowc_t)
	(_citrus_ctype_rec_t * __restrict, int, wint_t * __restrict);
typedef int	(*_citrus_ctype_wctob_t)
	(_citrus_ctype_rec_t * __restrict, wint_t, int * __restrict);

/*
 * ABI Version change log:
 *   0x00000001
 *     initial version
 *   0x00000002
 *     ops record:	btowc and wctob are added.
 *     ctype record:	unchanged.
 */
#define _CITRUS_CTYPE_ABI_VERSION	0x00000002
struct _citrus_ctype_ops_rec {
	uint32_t			co_abi_version;
	/* version 0x00000001 */
	_citrus_ctype_init_t		co_init;
	_citrus_ctype_uninit_t		co_uninit;
	_citrus_ctype_get_mb_cur_max_t	co_get_mb_cur_max;
	_citrus_ctype_mblen_t		co_mblen;
	_citrus_ctype_mbrlen_t		co_mbrlen;
	_citrus_ctype_mbrtowc_t		co_mbrtowc;
	_citrus_ctype_mbsinit_t		co_mbsinit;
	_citrus_ctype_mbsrtowcs_t	co_mbsrtowcs;
	_citrus_ctype_mbstowcs_t	co_mbstowcs;
	_citrus_ctype_mbtowc_t		co_mbtowc;
	_citrus_ctype_wcrtomb_t		co_wcrtomb;
	_citrus_ctype_wcsrtombs_t	co_wcsrtombs;
	_citrus_ctype_wcstombs_t	co_wcstombs;
	_citrus_ctype_wctomb_t		co_wctomb;
	/* version 0x00000002 */
	_citrus_ctype_btowc_t		co_btowc;
	_citrus_ctype_wctob_t		co_wctob;
};

#define _CITRUS_DEFAULT_CTYPE_NAME	"NONE"
#define _CITRUS_DEFAULT_CTYPE_OPS	_citrus_NONE_ctype_ops
#define _CITRUS_DEFAULT_CTYPE_HEADER	"citrus_none.h"

typedef _CITRUS_CTYPE_GETOPS_FUNC_BASE((*_citrus_ctype_getops_t));
struct _citrus_ctype_rec {
	_citrus_ctype_ops_rec_t	*cc_ops;
	void			*cc_closure;
	_citrus_module_t	cc_module;
};

#endif
