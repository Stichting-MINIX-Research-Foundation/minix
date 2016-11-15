/*	$NetBSD: ocryptodev.h,v 1.3 2015/09/06 06:01:02 dholland Exp $ */
/*	$FreeBSD: src/sys/opencrypto/cryptodev.h,v 1.2.2.6 2003/07/02 17:04:50 sam Exp $	*/
/*	$OpenBSD: cryptodev.h,v 1.33 2002/07/17 23:52:39 art Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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

/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 *
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#ifndef _CRYPTO_OCRYPTODEV_H_
#define _CRYPTO_OCRYPTODEV_H_

#include <sys/ioccom.h>

struct osession_op {	/* backwards compatible */
	u_int32_t	cipher;		/* ie. CRYPTO_DES_CBC */
	u_int32_t	mac;		/* ie. CRYPTO_MD5_HMAC */
	u_int32_t	keylen;		/* cipher key */
	void *		key;
	int		mackeylen;	/* mac key */
	void *		mackey;

  	u_int32_t	ses;		/* returns: session # */
};

struct osession_n_op {
	u_int32_t	cipher;		/* ie. CRYPTO_DES_CBC */
	u_int32_t	mac;		/* ie. CRYPTO_MD5_HMAC */

	u_int32_t	keylen;		/* cipher key */
	void *		key;
	int		mackeylen;	/* mac key */
	void *		mackey;

	u_int32_t	ses;		/* returns: session # */
	int		status;
};

struct ocrypt_op {
	u_int32_t	ses;
	u_int16_t	op;		/* i.e. COP_ENCRYPT */
	u_int16_t	flags;
	u_int		len;
	void *		src, *dst;	/* become iov[] inside kernel */
	void *		mac;		/* must be big enough for chosen MAC */
	void *		iv;
};

/* to support multiple session creation */
/*
 *
 * The reqid field is filled when the operation has 
 * been accepted and started, and can be used to later retrieve
 * the operation results via CIOCNCRYPTRET or identify the 
 * request in the completion list returned by CIOCNCRYPTRETM.
 *
 * The opaque pointer can be set arbitrarily by the user
 * and it is passed back in the crypt_result structure
 * when the request completes.  This field can be used for example
 * to track context for the request and avoid lookups in the
 * user application.
 */

struct ocrypt_n_op {
	u_int32_t	ses;
	u_int16_t	op;		/* i.e. COP_ENCRYPT */
	u_int16_t	flags;
	u_int		len;		/* src & dst len */

	u_int32_t	reqid;		/* request id */
	int		status;		/* status of request -accepted or not */	
	void		*opaque;	/* opaque pointer returned to user */
	u_int32_t	keylen;		/* cipher key - optional */
	void *		key;
	u_int32_t	mackeylen;	/* also optional */
	void *		mackey;

	void *		src, *dst;	/* become iov[] inside kernel */
	void *		mac;		/* must be big enough for chosen MAC */
	void *		iv;
};

struct ocrypt_sgop {
	size_t		count;
	struct osession_n_op * sessions;
};

struct ocrypt_mop {
	size_t 		count;		/* how many */
	struct ocrypt_n_op *	reqs;	/* where to get them */
};

#define	OCIOCGSESSION	_IOWR('c', 101, struct osession_op)
#define	OCIOCNGSESSION	_IOWR('c', 106, struct ocrypt_sgop)
#define OCIOCCRYPT	_IOWR('c', 103, struct ocrypt_op)
#define OCIOCNCRYPTM	_IOWR('c', 107, struct ocrypt_mop)

int ocryptof_ioctl(struct file *, u_long, void *);

#endif /* _CRYPTO_OCRYPTODEV_H_ */
