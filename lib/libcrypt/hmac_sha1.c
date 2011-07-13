/* $NetBSD: hmac_sha1.c,v 1.1 2006/10/27 18:22:56 drochner Exp $ */

/*
 * hmac_sha1 - using HMAC from RFC 2104
 */

#include <sha1.h> /* XXX */
#include "crypt.h"

#define HMAC_HASH SHA1
#define HMAC_FUNC __hmac_sha1
#define HMAC_KAT  hmac_kat_sha1

#define HASH_LENGTH SHA1_DIGEST_LENGTH
#define HASH_CTX SHA1_CTX
#define HASH_Init SHA1Init
#define HASH_Update SHA1Update
#define HASH_Final SHA1Final

#include "hmac.c"
