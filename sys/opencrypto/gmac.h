/* $NetBSD: gmac.h,v 1.2 2011/06/09 14:47:42 drochner Exp $ */
/* OpenBSD: gmac.h,v 1.1 2010/09/22 11:54:23 mikeb Exp */

/*
 * Copyright (c) 2010 Mike Belopuhov <mike@vantronix.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _GMAC_H_
#define _GMAC_H_

#include <crypto/rijndael/rijndael.h>

#define GMAC_BLOCK_LEN		16
#define GMAC_DIGEST_LEN		16

#ifdef _LP64
#define GMAC_INT uint64_t
#define GMAC_INTLEN 8
#else
#define GMAC_INT uint32_t
#define GMAC_INTLEN 4
#endif

typedef struct _GHASH_CTX {
	GMAC_INT	H[GMAC_BLOCK_LEN/GMAC_INTLEN];	/* hash subkey */
	GMAC_INT	S[GMAC_BLOCK_LEN/GMAC_INTLEN];	/* state */
	GMAC_INT	Z[GMAC_BLOCK_LEN/GMAC_INTLEN];	/* initial state */
} GHASH_CTX;

typedef struct _AES_GMAC_CTX {
	GHASH_CTX	ghash;
	uint32_t	K[4*(RIJNDAEL_MAXNR + 1)];
	uint8_t		J[GMAC_BLOCK_LEN];		/* counter block */
	int		rounds;
} AES_GMAC_CTX;

#include <sys/cdefs.h>

__BEGIN_DECLS
void	AES_GMAC_Init(AES_GMAC_CTX *);
void	AES_GMAC_Setkey(AES_GMAC_CTX *, const uint8_t *, uint16_t);
void	AES_GMAC_Reinit(AES_GMAC_CTX *, const uint8_t *, uint16_t);
int	AES_GMAC_Update(AES_GMAC_CTX *, const uint8_t *, uint16_t);
void	AES_GMAC_Final(uint8_t [GMAC_DIGEST_LEN], AES_GMAC_CTX *);
__END_DECLS

#endif /* _GMAC_H_ */
