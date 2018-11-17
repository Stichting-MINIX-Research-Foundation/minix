/*	$NetBSD: md5.h,v 1.2 2016/06/14 20:47:08 agc Exp $	*/

/*
 * This file is derived from the RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm and has been modified by Jason R. Thorpe <thorpej@NetBSD.org>
 * for portability and formatting.
 */

/*
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

#ifndef _SYS_MD5_H_
#define _SYS_MD5_H_

#include <sys/types.h>

#include <inttypes.h>

#define MD5_DIGEST_LENGTH		16
#define	MD5_DIGEST_STRING_LENGTH	33

#ifndef __BEGIN_DECLS
#  if defined(__cplusplus)
#  define __BEGIN_DECLS           extern "C" {
#  define __END_DECLS             }
#  else
#  define __BEGIN_DECLS
#  define __END_DECLS
#  endif
#endif

/* MD5 context. */
typedef struct MD5Context {
	uint32_t state[4];	/* state (ABCD) */
	uint32_t count[2];	/* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64]; /* input buffer */
} NETPGPV_MD5_CTX;

__BEGIN_DECLS
void	netpgpv_MD5Init(NETPGPV_MD5_CTX *);
void	netpgpv_MD5Update(NETPGPV_MD5_CTX *, const unsigned char *, unsigned int);
void	netpgpv_MD5Final(unsigned char[MD5_DIGEST_LENGTH], NETPGPV_MD5_CTX *);
#ifndef _KERNEL
char	*netpgpv_MD5End(NETPGPV_MD5_CTX *, char *);
char	*netpgpv_MD5File(const char *, char *);
char	*netpgpv_MD5Data(const unsigned char *, unsigned int, char *);
#endif /* _KERNEL */
__END_DECLS

#endif /* _SYS_MD5_H_ */
