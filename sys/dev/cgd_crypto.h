/* $NetBSD: cgd_crypto.h,v 1.8 2015/04/25 12:55:04 riastradh Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell.
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

#ifndef _DEV_CGD_CRYPTO_H_
#define	_DEV_CGD_CRYPTO_H_

#ifdef _KERNEL
#define CGD_CIPHER_DECRYPT	1
#define CGD_CIPHER_ENCRYPT	2

typedef void *(cfunc_init)(size_t, const void *, size_t *);
typedef void  (cfunc_destroy)(void *);
typedef void  (cfunc_cipher)(void *, struct uio *, struct uio *, const void *,
				int);

struct cryptfuncs {
	const char	 *cf_name;	/* cipher name */
	cfunc_init	 *cf_init;	/* Initialisation function */
	cfunc_destroy	 *cf_destroy;	/* destruction function */
	cfunc_cipher	 *cf_cipher;	/* the cipher itself */
};

const struct cryptfuncs	*cryptfuncs_find(const char *);
#endif /* _KERNEL */

#endif /* _DEV_CGD_CRYPTO_H_ */
