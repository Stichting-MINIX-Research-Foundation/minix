/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

static char const n8_id[] = "$Id: n8_precompute.c,v 1.2 2014/10/18 08:33:28 snj Exp $";
/*****************************************************************************/
/** @file n8_precompute.c
 *  @brief Precomputes IPAD and OPAD values for SSL and TLS MACs.
 *
 * A more detailed description of the file.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/06/02 bac   Fixed Bug #842 -- user's cipher info was being modified.  Now
 *                n8_precompute_tls_ipad_opad accepts a separate parameter for
 *                returning the hashed key and passes it to
 *                n8_HMAC_MD5_Precompute and n8_HMAC_SHA1_Precompute.
 * 03/06/02 brr   Removed openssl includes.
 * 02/22/02 spm   Converted printf's to DBG's.
 * 02/15/02 brr   Modified include files to build in kernel context.
 * 01/25/02 bac   Corrected signatures to placate the compiler on all
 *                platforms. 
 * 01/23/02 dws   Changed TLS functions to return the hashed key and its
 *                length.  The length of the hashed key is the hash function's
 *                digest size.
 * 01/23/02 bac   Removed debug printfs.
 * 01/22/02 dws   Original version.
 ****************************************************************************/
/** @defgroup subsystem_name Subsystem Title (not used for a header file)
 */

#include "n8_pub_types.h"
#include "n8_common.h"
#include "n8_precomp_md5.h"
#include "n8_precompute.h"


/* SSL MAC padding strings */
static char pad_1[49] = "666666666666666666666666666666666666666666666666";
static char pad_2[49] = "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\"
"\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\";
#define SSL_MD5_Pad_Length             48
#define SSL_MD5_Secret_Length          16

/**********************/
/* SHA-1 definitions. */
/**********************/
#define SHA_1
#define SHA_LONG unsigned int
#define SHA_LBLOCK         16
#define SHA_CBLOCK         (SHA_LBLOCK*4)
#define SHA_LAST_BLOCK     (SHA_CBLOCK-8)
#define SHA_DIGEST_LENGTH  20

typedef struct
{
   SHA_LONG h0,h1,h2,h3,h4;
   SHA_LONG Nl,Nh;
   SHA_LONG data[SHA_LBLOCK];
   int num;
} SHA_CTX;

/******************************************************/
/* Including this header instantiates the SHA-1 code. */
/******************************************************/
#include "n8_precomp_sha_locl.h"


/*****************************************************************************
 * n8_HMAC_MD5_Precompute
 *****************************************************************************/
/** @ingroup substem_name
 * @brief Compute HMAC-MD5 IPAD and OPAD values.
 *
 * Computes a partial HMAC of secret key.  These values are later used
 * to compute a complete HMAC.  This code was adapted from the OpenSSL
 * HMAC_Init function. If the key is longer than the hash block size,
 * the key is hashed.  The hashed key is returned in hashedKey and its length
 * is written back to the len parameter.
 *
 * @param key       RO: Pointer to the key string.
 * @param hashedKey WO: Pointer to the key string.
 * @param len       RW: Pointer to the length of the key string.
 * @param IPAD      WO: Pointer to the IPAD result buffer.
 * @param OPAD      WO: Pointer to the OPAD result buffer.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    None.
 *****************************************************************************/
static void n8_HMAC_MD5_Precompute(const void *key,
                                   void *hashedKey,
                                   uint32_t *len,
                                   uint32_t *IPAD,
                                   uint32_t *OPAD)
{
   N8_PRECOMP_MD5_CTX ctxi, ctxo; 
   int i,j,key_len,reset=0;
   unsigned char pad[MD5_CBLOCK];
   unsigned char hkey[MD5_CBLOCK];

   if (key != NULL)
   {
      reset=1;
      j=MD5_CBLOCK;
      if (j < *len)
      {
         n8_precomp_MD5_Init(&ctxi);
         n8_precomp_MD5_Update(&ctxi,key,*len);
         n8_precomp_MD5_Final(hkey, &ctxi);
         key_len = MD5_DIGEST_LENGTH;
         memcpy(hashedKey, hkey, key_len);
         *len = key_len;
      }
      else
      {
         memcpy(hkey,key,*len);
         memcpy(hashedKey, key, *len);
         key_len = *len;
      }
      if (key_len != MD5_CBLOCK)
         memset(&hkey[key_len], 0, MD5_CBLOCK - key_len);
   }

   if (reset)
   {
      for (i=0; i<MD5_CBLOCK; i++)
      {
         pad[i]=0x36^hkey[i];
      }
      n8_precomp_MD5_Init(&ctxi);
      n8_precomp_MD5_Update(&ctxi,pad,MD5_CBLOCK);
      IPAD[0] = swap_uint32(ctxi.A); 
      IPAD[1] = swap_uint32(ctxi.B); 
      IPAD[2] = swap_uint32(ctxi.C); 
      IPAD[3] = swap_uint32(ctxi.D); 
#ifdef HMAC_DEBUG
      DBG(("IPAD={A=%08x B=%08x C=%08x D=%08x}\n", 
             ctxi.A, ctxi.B, ctxi.C, ctxi.D));
#endif

      for (i=0; i<MD5_CBLOCK; i++)
      {
         pad[i]=0x5c^hkey[i];
      }
      n8_precomp_MD5_Init(&ctxo);
      n8_precomp_MD5_Update(&ctxo,pad,MD5_CBLOCK);
      OPAD[0] = swap_uint32(ctxo.A); 
      OPAD[1] = swap_uint32(ctxo.B); 
      OPAD[2] = swap_uint32(ctxo.C); 
      OPAD[3] = swap_uint32(ctxo.D); 
#ifdef HMAC_DEBUG
      DBG(("OPAD={A=%08x B=%08x C=%08x D=%08x}\n", 
             ctxo.A, ctxo.B, ctxo.C, ctxo.D));
#endif
   }
} /* n8_HMAC_MD5_Precompute */


/*****************************************************************************
 * n8_HMAC_SHA1_Precompute
 *****************************************************************************/
/** @ingroup substem_name
 * @brief Compute HMAC-SHA1 IPAD and OPAD values.
 *
 * Computes a partial HMAC of secret key.  These values are later used
 * to compute a complete HMAC.  This code was adapted from the OpenSSL
 * HMAC_Init function. If the key is longer than the hash block size,
 * the key is hashed.  The hashed key and its length are written back
 * to the key and len parameters.
 *
 * @param key     RW: Pointer to the key string.
 * @param len     RW: Pointer to the length of the key string.
 * @param IPAD    WO: Pointer to the IPAD result buffer.
 * @param OPAD    WO: Pointer to the OPAD result buffer.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    None.
 *****************************************************************************/
static void n8_HMAC_SHA1_Precompute(const void *key,
                                    void *hashedKey,
                                    uint32_t *len,
                                    uint32_t *IPAD,
                                    uint32_t *OPAD)
{
   SHA_CTX ctxi, ctxo;
   int i,j,key_len,reset=0;
   unsigned char pad[SHA_CBLOCK];
   unsigned char hkey[SHA_CBLOCK];

   if (key != NULL)
   {
      reset=1;
      j=SHA_CBLOCK;
      if (j < *len)
      {
         n8_precomp_SHA1_Init(&ctxi);
         n8_precomp_SHA1_Update(&ctxi,key,*len);
         n8_precomp_SHA1_Final(hkey, &ctxi);
         key_len = SHA_DIGEST_LENGTH;
         memcpy(hashedKey, hkey, key_len);
         *len = key_len;
      }
      else
      {
         memcpy(hkey,key,*len);
         memcpy(hashedKey, key, *len);
         key_len = *len;
      }
      if (key_len != SHA_CBLOCK)
         memset(&hkey[key_len], 0, SHA_CBLOCK - key_len);
   }

   if (reset)
   {
      for (i=0; i<SHA_CBLOCK; i++)
      {
         pad[i]=0x36^hkey[i];
      }
      n8_precomp_SHA1_Init(&ctxi);
      n8_precomp_SHA1_Update(&ctxi,pad,SHA_CBLOCK);
      IPAD[0] = ctxi.h0; 
      IPAD[1] = ctxi.h1; 
      IPAD[2] = ctxi.h2; 
      IPAD[3] = ctxi.h3; 
      IPAD[4] = ctxi.h4; 
#ifdef HMAC_DEBUG
      DBG(("IPAD={A=%08x B=%08x C=%08x D=%08x E=%08x}\n", 
             ctxi.h0, ctxi.h1, ctxi.h2, ctxi.h3, ctxi.h4));
#endif

      for (i=0; i<SHA_CBLOCK; i++)
      {
         pad[i]=0x5c^hkey[i];
      }
      n8_precomp_SHA1_Init(&ctxo);
      n8_precomp_SHA1_Update(&ctxo,pad,SHA_CBLOCK);
      OPAD[0] = ctxo.h0; 
      OPAD[1] = ctxo.h1; 
      OPAD[2] = ctxo.h2; 
      OPAD[3] = ctxo.h3; 
      OPAD[4] = ctxo.h4; 
#ifdef HMAC_DEBUG
      DBG(("OPAD={A=%08x B=%08x C=%08x D=%08x E=%08x}\n", 
             ctxo.h0, ctxo.h1, ctxo.h2, ctxo.h3, ctxo.h4));
#endif
   }
}


/*****************************************************************************
 * n8_precompute_ssl_ipad_opad
 *****************************************************************************/
/** @ingroup substem_name
 * @brief Compute SSL IPAD and OPAD values.
 *
 * Computes a partial SSL MAC of the SSL secret. These values are later used
 * to compute the complete SSL MAC of a packet. The hash algorith for the MAC
 * is MD5. (SHA-1 SSL MACs don't us IPAD/OPAD values.)
 *
 * @param secret_p      RO: Pointer to the 16-byte MAC secret key.
 * @param ipad_p        WO: Pointer to the buffer that is to receive the 
 *                          precomputed IPAD values.
 * @param opad_p        WO: Pointer to the buffer that is to receive the 
 *                          precomputed OPAD values.
 *
 * @par Externals:
 *    None.
 *
 * @return
 *    N8_STATUS_OK - the function succeeded. 
 *
 * @par Errors:
 *    None.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    IPAD and OPAD values are returned as arrays of uint32_t. 
 *****************************************************************************/
N8_Status_t n8_precompute_ssl_ipad_opad(N8_Buffer_t *secret_p,
                                        uint32_t *ipad_p, 
                                        uint32_t *opad_p)
{
   N8_Status_t status = N8_STATUS_OK;
   N8_PRECOMP_MD5_CTX     ctx;

   /* Hash the secret and pad_1 string using the IPAD context. */
   n8_precomp_MD5_Init(&ctx);
   n8_precomp_MD5_Update(&ctx, secret_p, SSL_MD5_Secret_Length);
   n8_precomp_MD5_Update(&ctx, pad_1, SSL_MD5_Pad_Length);
   ipad_p[0] = swap_uint32(ctx.A); 
   ipad_p[1] = swap_uint32(ctx.B); 
   ipad_p[2] = swap_uint32(ctx.C); 
   ipad_p[3] = swap_uint32(ctx.D);
#ifdef HMAC_DEBUG
   DBG(("IPAD={A=%08x B=%08x C=%08x D=%08x}\n", 
          ctx.A, ctx.B, ctx.C, ctx.D));
#endif

   /* Hash the secret and pad_2 string using the OPAD context. */
   n8_precomp_MD5_Init(&ctx);
   n8_precomp_MD5_Update(&ctx, secret_p, SSL_MD5_Secret_Length);
   n8_precomp_MD5_Update(&ctx, pad_2, SSL_MD5_Pad_Length);
   opad_p[0] = swap_uint32(ctx.A); 
   opad_p[1] = swap_uint32(ctx.B); 
   opad_p[2] = swap_uint32(ctx.C); 
   opad_p[3] = swap_uint32(ctx.D); 
#ifdef HMAC_DEBUG
   DBG(("OPAD={A=%08x B=%08x C=%08x D=%08x}\n", 
          ctx.A, ctx.B, ctx.C, ctx.D));
#endif
   
   return status;
} /* n8_precompute_ssl_ipad_opad */



/*****************************************************************************
 * n8_precompute_tls_ipad_opad
 *****************************************************************************/
/** @ingroup substem_name
 * @brief Compute TLS IPAD and OPAD values.
 *
 * Computes a partial HMAC of the TLS secret.  These values are later used
 * to compute the complete TLS MAC of a packet. Note that keys longer than 
 * 64 bytes are handled correctly. (They are hashed.) If the key is hashed,
 * the new value and length are written back through the secret_p and 
 * secret_length_p parameters.
 *
 * @param hash_alg        RO: Hash algorithm - must be N8_MD5 or N8_SHA1.
 * @param secret_p        RW: Pointer to the HMAC secret key.
 * @param hashed_key_p    WO: Pointer to the computed hashed key.
 * @param secret_length_p RW: Pointer to the length in bytes of the secret.
 * @param ipad_p          WO: Pointer to the buffer that is to receive the 
 *                            precomputed IPAD values.
 * @param opad_p          WO: Pointer to the buffer that is to receive the 
 *                            precomputed OPAD values.
 *
 * @par Externals:
 *    None.
 *
 * @return
 *    N8_STATUS_OK - the function succeeded. 
 *    N8_INVALID_HASH - hash_alg is not N8_MD5 or N8_SHA1.
 *
 * @par Errors:
 *    See return.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    IPAD and OPAD values are returned as arrays of uint32_t. 
 *****************************************************************************/
N8_Status_t n8_precompute_tls_ipad_opad(const N8_HashAlgorithm_t hash_alg,
                                        const N8_Buffer_t *secret_p,
                                        N8_Buffer_t *hashed_key_p,
                                        uint32_t *secret_length_p,
                                        uint32_t *ipad_p, 
                                        uint32_t *opad_p)
{
   N8_Status_t status = N8_STATUS_OK;

   /* Check for a valid hash algorithm. */
   if ((hash_alg == N8_MD5)      ||
       (hash_alg == N8_HMAC_MD5) || 
       (hash_alg == N8_HMAC_MD5_96))
   {
      n8_HMAC_MD5_Precompute(secret_p, hashed_key_p, secret_length_p, ipad_p, opad_p);
   }
   else if ((hash_alg == N8_SHA1) || (hash_alg == N8_HMAC_SHA1) ||
            (hash_alg == N8_HMAC_SHA1_96))
   {
      n8_HMAC_SHA1_Precompute(secret_p, hashed_key_p, secret_length_p, ipad_p, opad_p);
   }
   else
   {
      /* Not a valid choice for the hash algorithm. */
      status = N8_INVALID_HASH;
   }
   return status;
}


