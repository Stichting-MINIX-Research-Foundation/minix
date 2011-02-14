#ifndef __crypto_headers_h__
#define __crypto_headers_h__
#ifdef KRB5
#include <krb5-types.h>
#endif
#ifndef OPENSSL_DES_LIBDES_COMPATIBILITY
#define OPENSSL_DES_LIBDES_COMPATIBILITY
#endif
#include <openssl/evp.h>
#include <openssl/des.h>
#include <openssl/md2.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/rc4.h>
#include <openssl/rc2.h>
#include <openssl/aes.h>
#include <openssl/ui.h>
#include <openssl/rand.h>
#include <openssl/engine.h>
#include <openssl/pkcs12.h>
#include <openssl/hmac.h>
#ifndef BN_is_negative
#define BN_set_negative(bn, flag) ((bn)->neg=(flag)?1:0)
#define BN_is_negative(bn) ((bn)->neg != 0)
#endif
#endif /* __crypto_headers_h__ */
