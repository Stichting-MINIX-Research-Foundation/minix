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

static char const n8_id[] = "$Id: n8_rsa.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_rsa.c
 *  @brief Public RSA functions.
 *
 * Implementation of all public RSA functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * DON'T FORGET TO MAKE YOUR CHANGES OPTIMAL!
 * 12/17/03 bac   Added N8_RSAPublicDecrypt (as written by JPW).
 * 04/07/03 bac   Removed redundant CHECK_RETURN.
 * 09/10/02 brr   Set command complete bit on last command block.
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 05/01/02 brr   Memset data segment after return from N8_KMALLOC.
 * 04/08/02 bac   Corrected a padding bug that produced incorrect results
 *                for decryption with keys with leading zeroes.  (BUG 671).
 * 04/05/02 bac   Removed overly restrictive input tests for initialization.
 *                (BUG #517)
 * 04/01/02 brr   Validate modulus, p, and q pointers before
 *                use in N8_RSAInitializeKey.
 * 03/27/02 hml   N8_RSAInitializeKey now calls n8_validateUnit (BUG 653)
 *                and N8_RSAFreeKey returns N8_INVALID_OBJECT with a NULL
 *                pointer (BUG 576).
 * 03/26/02 hml   Changed N8_INVALID_UNIT to N8_UNINITIALIZED unit.
 * 03/25/02 spm   Removed test for N8_PRIVATE_SKS key type in N8_RSAEncrypt
 *                (public key routine).  Fixed SKS Round Robin unit binding
 *                so that a branch is not necessary when this feature is
 *                disabled.  (Bug 645)
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 03/18/02 bac   Added code to support SKS Round Robin.
 * 03/07/02 brr   Fix display functions, Don't use genericResultHandler when
 *                none is needed.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/25/02 brr   Removed last references to QMgr.
 * 02/22/02 spm   Converted printk's to DBG's.
 * 02/20/02 brr   Removed references to the queue structure.
 * 01/17/02 hml   Fix so previous delayed binding fix works with OpenSSL.
 * 01/15/02 bac   Re-arranged the late chip binding code to avoid problems in
 *                failure modes.
 * 01/14/02 hml   RSA fixes to delay chip binding fro encrypt/decrypt.
 * 12/11/01 mel   Fixed bug #380: Added extra checks for RSAInitializeKey 
 *                Fixed bug #412: Set SKS key length from SKS key material  
 * 12/05/01 mel   Fixed bug #405: Changed check for private key length to 
 *                modulus length check.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/27/01 bac   For public key initialization, get the private key length from
 *                the modulus length in the key material.
 * 11/26/01 bac   Fixed bug #338 by creating n8_RSAValidateKey to do key bounds
 *                checking. 
 * 11/16/01 mel   Fixed bug #333 : OpenSSL test rsa_test fails some padding
 *                checks. 
 * 11/11/01 bac   Removed BIGNUMS from key material.
 * 11/09/01 mel   Fixed bug #304 : N8_RSAInitializeKey does not check key
 *                lengths. 
 * 11/08/01 mel   Added unit ID parameter to commend block calls (bug #289).
 * 11/06/01 dkm   Corrected error returns for invalid key values.
 * 11/06/01 hml   Added some error checking and the structure verification.
 * 11/02/01 bac   Cleaned up printf/scanf formats to silence compiler warnings.
 * 10/31/01 bac   Modified interface to initPrivateSKSKey.
 * 10/19/01 bac   Added support for RSA types N8_PRIVATE and N8_PRIVATE_SKS.
 * 10/11/01 bac   More correctons for BUG #180.
 * 10/08/01 bac   Changes for the case where len(p) != len(q) (BUG #180).
 * 10/02/01 bac   Added use of RESULT_HANDLER_WARNING in all result handlers.
 * 10/01/01 hml   Added multi-chip functionality.
 * 09/05/01 bac   Globally changed to use 'req_p' for consistency.
 * 08/24/01 bac   Changed all interfaces to the cb commands to pre-allocate the
 *                command buffer space.
 * 08/21/01 bac   Fixes to allow the use of odd key lengths (Bug #174).
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/30/01 bac   Use queue_p in all macro length calculations.
 * 07/20/01 bac   Changed calls to create__RequestBuffer to pass the chip id.
 * 07/02/01 mel   Fixed comments.
 * 06/28/01 bac   Changed use of cb_rsaEncrypt and cb_rsaDecrypt to take
 *                physical addresses not N8_MemoryHandle_t-s, convert final
 *                routines to use single KMALLOC, fixed final methods that
 *                weren't async-ready.
 * 06/25/01 bac   More mem mgt changes and lots of cleanup.
 * 06/19/01 bac   Correct use of kernel memory.
 * 05/30/01 bac   Doxygenation plus standardization.
 * 05/30/01 bac   Changes to support message lengths that are not multiples of
 *                16 bytes for bug #16.  Removed reference to data in
 *                QUEUE_AND_CHECK that is previously freed in the result
 *                handler (bug #27).
 * 05/21/01 bac   Converted to use N8_ContextHandle_t and N8_Packet_t
 *                with integrated cipher and hash packet.
 * 05/21/01 bac   Return unimplemented function for initialization with SKS
 *                Private. 
 * 05/18/01 bac   Changed to use memory mgmt macros.  Removed unnecessary global
 *                variable. 
 * 04/30/01 bac   Changed BN display functions to N8_display*.
 * 04/26/01 bac   Changed interface to createRequestBuffer to remove
 *                num_commands as it was not used and confusing.
 * 04/26/01 bac   Added 'static' to resultHandler to avoid name clashes.
 * 04/10/01 bac   Extensive re-write to bring up to standards.  Fixed
 * 04/24/01 bac   Extensive re-write to bring up to standards.  Fixed
 *                various memory leaks.
 * 04/05/01 bac   Added error checking to all cb_* calls.
 * 04/05/01 bac   Changed all key length info to be in bytes, not digits.
 * 04/04/01 bac   Added ordering of p and q so that p < q and added
 *                calculation of u.
 * 03/01/01 bac   Original version.
 ****************************************************************************/
/** @defgroup n8_rsa RSA Functions.
 */

#include "n8_common.h"
#include "n8_pub_errors.h"
#include "n8_rsa.h"
#include "n8_pk_common.h"
#include "n8_enqueue_common.h"
#include "n8_util.h"
#include "n8_API_Initialize.h"

#include "n8_cb_rsa.h"

#ifdef N8DEBUG
#define DBG_PARAM(__M, __P, __L) \
    DBG((__M)); \
    N8_print_rsa_parameters((__P), (__L));
#else
#define DBG_PARAM(M,P,L)
#endif

/*
 * local predeclarations
 */
static N8_Status_t initPublicKey(N8_RSAKeyObject_t   *key_p,
                                 N8_RSAKeyMaterial_t *material_p,
                                 N8_Event_t          *event_p);
static N8_Status_t initPrivateKey(N8_RSAKeyObject_t  *key_p,
                                  N8_RSAKeyMaterial_t *material_p,
                                  N8_Event_t          *event_p);
static N8_Status_t initPrivateKeyCRT(N8_RSAKeyObject_t  *key_p,
                                     N8_RSAKeyMaterial_t *material_p,
                                     N8_Event_t          *event_p);
static N8_Status_t initPrivateSKSKey(N8_RSAKeyObject_t   *key_p,
                                     N8_RSAKeyMaterial_t *material_p,
                                     N8_Event_t          *event_p);

/*****************************************************************************
 * n8_RSAValidateKey
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Minimal bounds checking for RSA keys.
 *
 * Based upon the key type, test the key lengths for basic sanity.
 *
 *  @param material_p          RO:  Pointer to RSA key material
 *  @param type                RO:  Key type
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status of comparison.  N8_STATUS_OK if fine.
 *
 * @par Errors
 *    N8_INVALID_KEY_SIZE if invalid.<br>
 *    N8_INVALID_ENUM if type is invalid.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t n8_RSAValidateKey(const N8_RSAKeyMaterial_t *material_p,
                                     const N8_KeyType_t type)
{
   N8_Status_t ret = N8_STATUS_OK;
   /* perform bounds checking based upon type. */
   switch (type)
   {
      case N8_PUBLIC:
         /* ensure the required elements of the key material are present.  for
          * PUBLIC: publicKey and n */
         CHECK_OBJECT(material_p->publicKey.value_p, ret);
         CHECK_OBJECT(material_p->n.value_p, ret);

         /* for a public key, bounds check modulus length and public key
          * length from material. */
         if ((material_p->n.lengthBytes < N8_RSA_KEY_LENGTH_MIN) ||
             (material_p->n.lengthBytes > N8_RSA_KEY_LENGTH_MAX) ||
             (material_p->publicKey.lengthBytes  < N8_RSA_KEY_LENGTH_MIN) ||
             (material_p->publicKey.lengthBytes  > N8_RSA_KEY_LENGTH_MAX))
         {
            ret = N8_INVALID_KEY_SIZE;
         }
         break;
      case N8_PRIVATE_CRT:
         /* ensure the required elements of the key material are present.  for
          * PRIVATE CRT: p and q in addition to those for PRIVATE */
         CHECK_OBJECT(material_p->p.value_p, ret);
         CHECK_OBJECT(material_p->q.value_p, ret);
         /* NOTE case statement fall through */
      case N8_PRIVATE:
         /* ensure the required elements of the key material are present.  for
          * PRIVATE: privateKey and n */
         CHECK_OBJECT(material_p->privateKey.value_p, ret);
         CHECK_OBJECT(material_p->n.value_p, ret);
         /* for private, non-SKS, bounds check the private key length
          * only. */ 
         if ((material_p->privateKey.lengthBytes < N8_RSA_KEY_LENGTH_MIN) ||
             (material_p->privateKey.lengthBytes > N8_RSA_KEY_LENGTH_MAX))
         {
            ret = N8_INVALID_KEY_SIZE;
         }
         break;
      case N8_PRIVATE_SKS:
         /* for SKS, bounds check the key length in the SKS Key Handle
          * only. */
         if (material_p->SKSKeyHandle.key_length < N8_RSA_SKS_KEY_LENGTH_DIGITS_MIN ||
             material_p->SKSKeyHandle.key_length > N8_RSA_SKS_KEY_LENGTH_DIGITS_MAX)
         {
            ret = N8_INVALID_KEY_SIZE;
         }
         break;
      default:
         ret = N8_INVALID_ENUM;
         break;
   } /* switch */

   return ret;
} /* n8_RSAValidateKey */

#ifdef N8DEBUG
/*****************************************************************************
 * N8_print_rsa_parameters
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Print RSA parameters.
 *
 *
 *  @param p_block           RO:  pointer to the parameter's block to be
 *                                printed.
 *  @param key_p             RO:  pointer to key object
 *  @param key_length_bytes  RO:  key length
 *
 * @return
 *    None.
 *
 *****************************************************************************/
void N8_print_rsa_parameters(unsigned char *p_block,
                             N8_RSAKeyObject_t *key_p)
{
   unsigned char  *ptr;
   int          i, j;
   uint32_t     key_length_bytes;
   uint32_t     key_length;
   unsigned int pbLength;
   int paramByteLength = PK_RSA_Param_Byte_Length(key_p);

   if (p_block == NULL || key_p == NULL)
   {
      return;
   }
   
   key_length_bytes = key_p->privateKeyLength;
   key_length = BYTES_TO_PKDIGITS(key_length_bytes);
   N8_PRINT("\nRSA decrypt/sign parameters\n");
   N8_PRINT("key length=%d (0x%08x)\n", key_length, key_length);
   /* print p */
   ptr = p_block + PK_RSA_P_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr,
                    PK_RSA_P_Byte_Length(key_p),
                    "p");
   /* print q */
   ptr = p_block + PK_RSA_Q_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_Q_Byte_Length(key_p), "q");

   /* print dp */
   ptr = p_block + PK_RSA_DP_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_DP_Byte_Length(key_p), "dp");

   /* print dq */
   ptr = p_block + PK_RSA_DQ_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_DQ_Byte_Length(key_p), "dq");

   /* print R mod p */
   ptr = p_block + PK_RSA_R_MOD_P_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_R_MOD_P_Byte_Length(key_p), "R mod p");

   /* print R mod q */
   ptr = p_block + PK_RSA_R_MOD_Q_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_R_MOD_Q_Byte_Length(key_p), "R mod q");

   /* print N */
   ptr = p_block + PK_RSA_N_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_N_Byte_Length(key_p), "N");

   /* print u */
   ptr = p_block + PK_RSA_U_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_U_Byte_Length(key_p), "u");

   /* print cp */
   ptr = p_block + PK_RSA_CP_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_CP_Byte_Length, "cp");

   /* print cq */
   ptr = p_block + PK_RSA_CQ_Param_Byte_Offset(key_p);
   n8_displayBuffer(ptr, PK_RSA_CQ_Byte_Length, "cq");

   N8_PRINT("---------\n");
   ptr = p_block;
   pbLength = BYTES_TO_PKDIGITS(paramByteLength);
   for (j = 0; j < pbLength; j++)
   {
      for (i = 0; i < PK_Bytes_Per_BigNum_Digit; i++)
      {
         N8_PRINT("%02x", (*ptr++) & 0xff);
      }
      N8_PRINT("\n");
   }
   N8_PRINT("\n");
   N8_PRINT("---------\n");
} /* N8_print_rsa_parameters */
#endif


/*****************************************************************************
 * N8_RSAInitializeKey
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Initialize an RSA key object.
 *
 * Initializes the specified key object so that it can be used in
 * subsequent RSA encrypt/decrypt operation.  The key type specifies
 * the manner in which the key object is to be interpreted.
 *
 *  @param key_p               RW:  pointer to the key to be initialized
 *  @param type                RO:  key type
 *  @param material_p          RO:  pointer to key material with initialization
 *                                  values. 
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error code if encountered.
 *
 * @par Errors
 *    N8_UNIMPLEMENTED_FUNCTION - type of N8_PRIVATE or N8_PRIVATE_SKS used.<br>
 *    N8_INVALID_KEY - type passed is not recognized.<br>
 *     
 *
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_RSAInitializeKey(N8_RSAKeyObject_t     *key_p,
                                N8_KeyType_t           type,
                                N8_RSAKeyMaterial_t   *material_p,
                                N8_Event_t            *event_p)
{
   N8_Status_t     ret = N8_STATUS_OK;
   N8_Unit_t       unit = N8_UNINITIALIZED_UNIT;
   N8_Boolean_t    unitValid;
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(material_p, ret);

      /* bounds test key lengths. */
      ret = n8_RSAValidateKey(material_p, type);
      CHECK_RETURN(ret);

      /* clear the contents of the key. */
      memset(key_p, 0, sizeof(N8_RSAKeyObject_t));

      /* The unit specifier is found in a different place depending on
         the requested type */
      switch (type)
      {
         case N8_PUBLIC:
         case N8_PRIVATE_CRT:
         case N8_PRIVATE:
            unit = material_p->unitID;
            break;

         case N8_PRIVATE_SKS:
            unit = material_p->SKSKeyHandle.unitID;
            /* Copy all of the SKS data into the key */
            memcpy((void *) &(key_p->SKSKeyHandle),
                   (const void *) &(material_p->SKSKeyHandle),
                   sizeof(N8_SKSKeyHandle_t));
            break;

          default:
            /* value has already been checked. */
            break;
      }

      /* Make sure the unit is valid */
      unitValid = n8_validateUnit(unit);
      if (!unitValid)
      {
         ret = N8_INVALID_UNIT;
         break;
      }
      key_p->unitID = unit;
      
      key_p->keyType = type;
      switch(type)
      {
         case N8_PUBLIC:
            /*
             * key material must contain public key e, modulus n, and their
             * sizes 
             */
            CHECK_OBJECT(material_p->publicKey.value_p, ret);
            ret = initPublicKey(key_p, material_p, event_p);
            break;

         case N8_PRIVATE:
            /*
             * key material must contain private key d, modulus n, and their
             * sizes
             */
            CHECK_OBJECT(material_p->privateKey.value_p, ret);
            CHECK_OBJECT(material_p->n.value_p, ret);
            ret = initPrivateKey(key_p, material_p, event_p);
            break;
         case N8_PRIVATE_CRT:
            /* used to perform private encryption with Chinese Remainder
             * Theorem.  key material must contain private key d, modulus n,
             * and their sizes.  also, p and q, factors of n, (i.e. n = p*q)
             * shall be included.  (the relation is assumed but not
             * verified.)
             */
            CHECK_OBJECT(material_p->privateKey.value_p, ret);
            CHECK_OBJECT(material_p->n.value_p, ret);
            CHECK_OBJECT(material_p->p.value_p, ret);
            CHECK_OBJECT(material_p->q.value_p, ret);
            ret = initPrivateKeyCRT(key_p, material_p, event_p);
            break;
         case N8_PRIVATE_SKS:
            /*
             * used to perform private encryption but gets the key material
             * from the Secure Key Storage
             */
            ret = initPrivateSKSKey(key_p, material_p, event_p);
            break;
         default:
            ret = N8_INVALID_KEY;
            break;
      }
   }
   while (FALSE);

   if (ret == N8_STATUS_OK)
   {
      /* Set the structure ID */
      key_p->structureID = N8_RSA_STRUCT_ID;

      /* Special code for RSA key object late-binding to an execution unit */
      if (type == N8_PUBLIC || type == N8_PRIVATE_CRT || type == N8_PRIVATE)
      {
         /* Reset the unit to the unit from the material. The RSA Decrypt
            will have to validate the unit again.  This allows all of the
            decrypt operations to be performed on different units. */
         key_p->unitID = material_p->unitID;
      }

   }
   return ret;
} /* N8_RSAInitializeKey */

/*****************************************************************************
 * N8_RSAEncrypt
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Perform an RSA encrypt on the message.  This is also used to
 * perform the verify function for authentication.
 *
 *
 *  @param key_p               RW:  pointer to the key to be initialized
 *  @param msgIn               RO:  incoming message to be encrypted
 *  @param msgLength           RO:  incoming message length
 *  @param msgOut              RW:  resulting encrypted message
 *  @param event_p             RW:  On input, if null the call is synchronous 
 *                                  and no event is returned. The operation 
 *                                  is complete when the call returns. If 
 *                                  non-null, then the call is asynchronous; 
 *                                  an event is returned that can be used to 
 *                                  determine when the operation completes.
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error code if encountered.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *          N8_INVALID_OBJECT   -   object is NULL<BR>
 *
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_RSAEncrypt(N8_RSAKeyObject_t *key_p,
                          N8_Buffer_t       *msgIn,
                          uint32_t           msgLength,
                          N8_Buffer_t       *msgOut,
                          N8_Event_t        *event_p)
{
   N8_Status_t    ret = N8_STATUS_OK;
   int            nBytes;
   
   N8_Buffer_t   *kmsgIn_p = NULL;
   N8_Buffer_t   *kmsgOut_p = NULL;
   uint32_t       kmsgIn_a;
   uint32_t       kmsgOut_a;
   API_Request_t *req_p = NULL;
   char          *p;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(msgIn, ret);
      CHECK_OBJECT(msgOut, ret);                                 
      CHECK_STRUCTURE(key_p->structureID, N8_RSA_STRUCT_ID, ret);

      if (key_p == NULL || key_p->keyType != N8_PUBLIC)
      {
         ret = N8_INVALID_KEY;
         break;
      }

      if (msgLength > key_p->privateKeyLength)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      nBytes = NEXT_WORD_SIZE(key_p->privateKeyLength) * 2;

      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_RSA_ENCRYPT_NUMCMDS, 
                                  resultHandlerGeneric, nBytes);
      CHECK_RETURN(ret);

      kmsgIn_p  = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      kmsgIn_a  =                 req_p->qr.physicalAddress + req_p->dataoffset;
      kmsgOut_p = kmsgIn_p + NEXT_WORD_SIZE(key_p->privateKeyLength);
      kmsgOut_a = kmsgIn_a + NEXT_WORD_SIZE(key_p->privateKeyLength);

      p = kmsgIn_p + key_p->privateKeyLength - msgLength;
      memcpy(p, msgIn, msgLength);

      req_p->copyBackTo_p = msgOut;
      req_p->copyBackFrom_p = kmsgOut_p + key_p->privateKeyLength - msgLength;
      req_p->copyBackSize = msgLength;
      ret = cb_rsaEncrypt(req_p, key_p, kmsgIn_a, kmsgOut_a,
                          req_p->PK_CommandBlock_ptr, 
                          key_p->unitID);
      CHECK_RETURN(ret);

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);
   DBG(("encrypt\n"));

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
  
   return ret;
} /* N8_RSAEncrypt */


/*****************************************************************************
 * N8_RSADecrypt
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Perform an RSA decrypt on the message.  This is also used to
 * perform the sign function for authentication.
 *
 *
 *  @param key                 RW:  pointer to the key to be initialized
 *  @param msgIn               RO:  incoming message to be encrypted
 *  @param msgLength           RO:  incoming message length
 *  @param msgOut              RW:  resulting encrypted message
 *  @param event_p             RW:  On input, if null the call is synchronous 
 *                                  and no event is returned. The operation 
 *                                  is complete when the call returns. If 
 *                                  non-null, then the call is asynchronous; 
 *                                  an event is returned that can be used to 
 *                                  determine when the operation completes.
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error code if encountered.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *          N8_INVALID_OBJECT   -   object is NULL<BR>
 *
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_RSADecrypt(N8_RSAKeyObject_t *key_p,
                          N8_Buffer_t *msgIn,
                          uint32_t msgLength,
                          N8_Buffer_t *msgOut,
                          N8_Event_t *event_p)
{
   N8_Status_t     ret = N8_STATUS_OK;
   N8_Buffer_t    *kmsgIn_p = NULL;
   N8_Buffer_t    *kmsgOut_p = NULL;
   uint32_t        kmsgIn_a;
   uint32_t        kmsgOut_a;
   API_Request_t  *req_p = NULL;
   char           *p;
   int             nBytes;
   N8_Unit_t       unit = N8_UNINITIALIZED_UNIT;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(msgIn, ret);
      CHECK_OBJECT(msgOut, ret);
      CHECK_STRUCTURE(key_p->structureID, N8_RSA_STRUCT_ID, ret);

      if (key_p == NULL ||
          (key_p->keyType != N8_PRIVATE &&
           key_p->keyType != N8_PRIVATE_CRT &&
           key_p->keyType != N8_PRIVATE_SKS))
      {
         ret = N8_INVALID_KEY;
         break;
      }
    
#if N8_SKS_ROUND_ROBIN
      if (key_p->keyType == N8_PRIVATE_SKS)
      {
         unit = key_p->SKSKeyHandle.unitID;
      }
      else
      {
         unit = key_p->unitID;
      }
#else
      unit = key_p->unitID;
#endif

      if (msgLength > key_p->privateKeyLength)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      nBytes = NEXT_WORD_SIZE(key_p->privateKeyLength) * 2;

      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  unit, 
                                  N8_CB_RSA_DECRYPT_NUMCMDS(key_p), 
                                  resultHandlerGeneric, nBytes);
      CHECK_RETURN(ret);

      kmsgIn_p  = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      kmsgIn_a  =                 req_p->qr.physicalAddress + req_p->dataoffset;
      kmsgOut_p = (N8_Buffer_t *) kmsgIn_p + NEXT_WORD_SIZE(key_p->privateKeyLength);
      kmsgOut_a =                 kmsgIn_a + NEXT_WORD_SIZE(key_p->privateKeyLength);

      p = kmsgIn_p + key_p->privateKeyLength - msgLength;
      memcpy(p, msgIn, msgLength);


      req_p->copyBackTo_p   = msgOut;
      req_p->copyBackFrom_p = kmsgOut_p + key_p->privateKeyLength - msgLength;
      req_p->copyBackSize   = msgLength;
      
      ret = cb_rsaDecrypt(req_p, key_p, kmsgIn_a, kmsgOut_a, 
                          req_p->PK_CommandBlock_ptr, unit);
      CHECK_RETURN(ret);
           
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }

   return ret;
} /* N8_RSADecrypt */

/*****************************************************************************
 * N8_RSAPublicDecrypt
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Perform an RSA public decrypt on the message.  This is also used to
 * perform the verify function for authentication.
 *
 *
 *  @param key_p               RW:  pointer to the key to be initialized
 *  @param msgIn               RO:  incoming message to be encrypted
 *  @param msgLength           RO:  incoming message length
 *  @param msgOut              RW:  resulting encrypted message
 *  @param event_p             RW:  On input, if null the call is synchronous 
 *                                  and no event is returned. The operation 
 *                                  is complete when the call returns. If 
 *                                  non-null, then the call is asynchronous; 
 *                                  an event is returned that can be used to 
 *                                  determine when the operation completes.
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error code if encountered.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *          N8_INVALID_OBJECT   -   object is NULL<BR>
 *
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_RSAPublicDecrypt(N8_RSAKeyObject_t   *key_p,
                                const N8_RSAKeyMaterial_t *material_p,
                                const N8_Buffer_t         *msgIn,
                                uint32_t             msgLength,
                                N8_Buffer_t         *msgOut,
                                N8_Event_t          *event_p)
{
   N8_Status_t    ret = N8_STATUS_OK;
   int            nBytes;
   
   N8_Buffer_t   *kmsgIn_p = NULL;
   N8_Buffer_t   *kmsgOut_p = NULL;
   uint32_t       kmsgIn_a;
   uint32_t       kmsgOut_a;
   API_Request_t *req_p = NULL;
   char          *p;
   uint32_t	      unit;
   uint32_t	      unitValid;

   unsigned int   pkDigitSize = SIMON_BITS_PER_DIGIT / 8;
   unsigned int   padding;
   unsigned long  pAddr;
   char          *vAddr;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(msgIn, ret);
      CHECK_OBJECT(msgOut, ret);                                 

      /* From initPublicKey */

      /* bounds test key lengths. */
      ret = n8_RSAValidateKey(material_p, N8_PUBLIC);
      /* clear the contents of the key. */
      memset(key_p, 0, sizeof(N8_RSAKeyObject_t));

      unit = material_p->unitID;
      /* Make sure the unit is valid */
      unitValid = n8_validateUnit(unit);
      if (!unitValid)
      {
         ret = N8_INVALID_UNIT;
         break;
      }
      key_p->unitID = unit;

      padding = (pkDigitSize -
                 (material_p->publicKey.lengthBytes % pkDigitSize));
      key_p->publicKeyLength =
         material_p->publicKey.lengthBytes + padding;
      /* the private key length is a bit of a misnomer.  it is also the modulus
       * length.  for a public key we don't have the privateKey information
       * filled in, so we get the key length from the modulus. */
      key_p->privateKeyLength = material_p->n.lengthBytes;
      key_p->publicKeyDigits = BYTES_TO_PKDIGITS(key_p->publicKeyLength);
      key_p->privateKeyDigits = BYTES_TO_PKDIGITS(key_p->privateKeyLength);
      /* unintuitively, reassign the private key length to ensure it is rounded
       * up if necessary. */
      key_p->privateKeyLength = PKDIGITS_TO_BYTES(key_p->privateKeyDigits);

      DBG(("initPublicKey\n"));
      DBG(("public key length: %d\n", key_p->publicKeyLength));
      DBG(("private key length: %d\n", key_p->privateKeyLength));

      /* pre-allocate all of the kernel memory we need at once */
      nBytes = (NEXT_WORD_SIZE(key_p->publicKeyLength) + /* public key */
                NEXT_WORD_SIZE(PK_RSA_N_Byte_Length(key_p)) + /* modulus */
                2 * NEXT_WORD_SIZE(key_p->privateKeyLength)); /* msg in and out */

      /* allocate PK Command block buffer in Kernel Space */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_RSA_PUBLICDECRYPT_NUMCMDS,
                                  resultHandlerGeneric, nBytes); 
      CHECK_RETURN(ret);

      /* there is no persistent kernel memory in the key object for this
         operation. */
      kmsgIn_p  = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      kmsgIn_a  = req_p->qr.physicalAddress + req_p->dataoffset;
      kmsgOut_p = kmsgIn_p + NEXT_WORD_SIZE(key_p->privateKeyLength);
      kmsgOut_a = kmsgIn_a + NEXT_WORD_SIZE(key_p->privateKeyLength);

      vAddr = kmsgOut_p + NEXT_WORD_SIZE(key_p->privateKeyLength);
      pAddr = kmsgOut_a + NEXT_WORD_SIZE(key_p->privateKeyLength);

      memset(kmsgIn_p, 0, nBytes);

      /* Tasks 1 */

      /*
       * allocate space for the key.  it is not a part of the parameter
       * block
       */
      key_p->key = pAddr;
      /* copy the public key into the key object.
       * this must be zero-filled to the left in order to fill an integral
       * number of Big Number Cache digits.
       *
       * padding = digit_size - (pub_key_len % digit_size)
       */
      memcpy(&vAddr[padding],
             material_p->publicKey.value_p,
             material_p->publicKey.lengthBytes);

      pAddr += NEXT_WORD_SIZE(key_p->publicKeyLength);
      vAddr += NEXT_WORD_SIZE(key_p->publicKeyLength);

      /* Tasks 2 */

      /*
       * allocate space for the modulus.  it is not a part of the parameter
       * block for a private key object.
       */
      key_p->n = pAddr;
      padding = (PK_RSA_N_Byte_Length(key_p) - material_p->n.lengthBytes);
      memcpy(&vAddr[padding], material_p->n.value_p, material_p->n.lengthBytes);
      pAddr += NEXT_WORD_SIZE(PK_RSA_N_Byte_Length(key_p));
      vAddr += NEXT_WORD_SIZE(PK_RSA_N_Byte_Length(key_p));

      /* End From initPublicKey */
      
      /* Magic tag to note key has been initialized */
      key_p->structureID = N8_RSA_STRUCT_ID;

      p = kmsgIn_p + key_p->privateKeyLength - msgLength;
      memcpy(p, msgIn, msgLength);

      req_p->copyBackTo_p = msgOut;
      req_p->copyBackFrom_p = kmsgOut_p + key_p->privateKeyLength - msgLength;
      req_p->copyBackSize = msgLength;
      ret = cb_rsaPublicDecrypt(req_p,
                                key_p->n,                /* modulus */
                                key_p->privateKeyLength, /* modulus length */
                                kmsgIn_a,
                                kmsgOut_a,
                                key_p->key,              /* public key */
                                key_p->publicKeyLength,  /* public key length */
                                req_p->PK_CommandBlock_ptr,
                                key_p->unitID);
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);
   DBG(("N8_RSAPublic_Decrypt\n"));

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
  
   return ret;
} /* N8_RSAPublic_Decrypt */

/**********************************************************************
 * N8_RSAFreeKey
 ***********************************************************************/
/**  @ingroup dsa
 * @brief Frees the specified RSAKeyObject_p and all associated resources
 * so that they can be reused.
 *
 * Description:
 * When an application is finished using a previously initialized
 * RSAKeyObject_p, it should be freed so that any API or system resources
 * (including memory) can be freed.  After this call returns, RSAKeyObject_p
 * may no longer be used in N8_RSAEncrypt or N8_RSADecrypt calls.
 *
 * 
 * Parameters:
 * @param RSAKeyObject_p  RO:   The previously initialized RSAKeyObject containing the RSA key
 *                                  materials to be used.
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_KEY      -       The DSAKeyObject is not a valid key object
 *                                  for this operation.
 *   
 *
 * @par Assumptions:
 **********************************************************************/
N8_Status_t  N8_RSAFreeKey (N8_RSAKeyObject_t *key_p)
{

   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      if (key_p == NULL)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      CHECK_STRUCTURE(key_p->structureID, N8_RSA_STRUCT_ID, ret);
      if (key_p->kmem_p != NULL)
      {
         N8_KFREE(key_p->kmem_p);
      }
      key_p->structureID = 0;

   } while(FALSE);
   return  ret;
} /* N8_RSAFreeKey */
/*
 * Local functions
 */

/*****************************************************************************
 * initPublicKey
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Initialize an RSA public key object.
 *
 *
 *  @param key                 RW:  pointer to the key to be initialized
 *  @param material            RO:  pointer to key material with initialization
 *                                  values.
 *  @param paramBlock_pp       WO:  pointer parameter block pointer
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error code if encountered.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
static N8_Status_t initPublicKey(N8_RSAKeyObject_t    *key_p,
                                 N8_RSAKeyMaterial_t  *material_p,
                                 N8_Event_t           *event_p)
{

   /*
    * Tasks to be performed:
    * 1) Put public key, e, into the key object
    * 2) Put modulus, n, into the key object
    * 3) Compute cn and put into the key object
    * 4) Compute R mod n and put into the key object
    */

   API_Request_t  *req_p = NULL;
   unsigned int    nBytes;
   unsigned int    pkDigitSize = SIMON_BITS_PER_DIGIT / 8;
   unsigned int    padding;
   unsigned long   pAddr;
   char           *vAddr;
   N8_Status_t     ret = N8_STATUS_OK;
   PK_CMD_BLOCK_t *nextCommandBlock = NULL;

   do
   {
      padding = (pkDigitSize -
                 (material_p->publicKey.lengthBytes % pkDigitSize));
      key_p->publicKeyLength =
         material_p->publicKey.lengthBytes + padding;
      /* the private key length is a bit of a misnomer.  it is also the modulus
       * length.  for a public key we don't have the privateKey information
       * filled in, so we get the key length from the modulus. */
      key_p->privateKeyLength = material_p->n.lengthBytes; 
      key_p->publicKeyDigits = BYTES_TO_PKDIGITS(key_p->publicKeyLength); 
      key_p->privateKeyDigits = BYTES_TO_PKDIGITS(key_p->privateKeyLength);
      /* unintuitively, reassign the private key length to ensure it is rounded
       * up if necessary. */
      key_p->privateKeyLength = PKDIGITS_TO_BYTES(key_p->privateKeyDigits);

      DBG(("initPublicKey\n"));
      DBG(("public key length: %d\n", key_p->publicKeyLength));
      DBG(("private key length: %d\n", key_p->privateKeyLength));

      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_COMPUTE_CX_NUMCMDS +
                                  N8_CB_COMPUTE_RMODX_NUMCMDS, 
                                  NULL, 0);
      CHECK_RETURN(ret);

      /* pre-allocate all of the kernel memory we need at once */
      nBytes = (NEXT_WORD_SIZE(key_p->publicKeyLength) +
                NEXT_WORD_SIZE(PK_RSA_N_Byte_Length(key_p)) +
                NEXT_WORD_SIZE(PK_RSA_CN_Byte_Length) +
                NEXT_WORD_SIZE(PK_RSA_R_MOD_N_Byte_Length(key_p)));

      /* the kernel memory allocated here is freed in the call to
       * N8_RSAFreeKey.  DO NOT add it to the free list here. */
      key_p->kmem_p = N8_KMALLOC_PK(nBytes);
      CHECK_OBJECT(key_p->kmem_p, ret);

      pAddr = key_p->kmem_p->PhysicalAddress;
      vAddr = (N8_Buffer_t *) key_p->kmem_p->VirtualAddress;
      memset(vAddr, 0, nBytes);
      /* Tasks 1 */

      /*
       * allocate space for the key.  it is not a part of the parameter
       * block
       */
      key_p->key = pAddr;

      /* copy the public key into the key object.
       * this must be zero-filled to the left in order to fill an integral
       * number of Big Number Cache digits.
       *
       * padding = digit_size - (pub_key_len % digit_size)
       */
      memset(vAddr, 0x0, padding);
      memcpy(&vAddr[padding],
             material_p->publicKey.value_p,
             material_p->publicKey.lengthBytes);
      
      pAddr += NEXT_WORD_SIZE(key_p->publicKeyLength);
      vAddr += NEXT_WORD_SIZE(key_p->publicKeyLength);

      /* Tasks 2 */

      /*
       * allocate space for the modulus.  it is not a part of the parameter
       * block for a private key object.
       */
      key_p->n = pAddr;
      padding = (PK_RSA_N_Byte_Length(key_p) - material_p->n.lengthBytes);
      memcpy(&vAddr[padding], material_p->n.value_p, material_p->n.lengthBytes);
      pAddr += NEXT_WORD_SIZE(PK_RSA_N_Byte_Length(key_p));
      vAddr += NEXT_WORD_SIZE(PK_RSA_N_Byte_Length(key_p));

      /*  Task 3: Compute cn = -(n[0]^-1 mod 2^128) mod 2^128 and  */
      /*      put it in the key object. */
      key_p->cn = pAddr;

      ret = cb_computeCX(req_p, key_p->n, key_p->cn, PK_RSA_N_Byte_Length(key_p), 
                         req_p->PK_CommandBlock_ptr, &nextCommandBlock,
                         key_p->unitID, N8_FALSE);
      pAddr += NEXT_WORD_SIZE(PK_RSA_CN_Byte_Length);
      vAddr += NEXT_WORD_SIZE(PK_RSA_CN_Byte_Length);

      /*  Task 4: Compute R mod n and put it in the key object. */
      key_p->R_mod_n = pAddr;
      ret = cb_computeRmodX(req_p, key_p->n, key_p->R_mod_n,
                            PK_RSA_N_Byte_Length(key_p),
                            nextCommandBlock, &nextCommandBlock, N8_TRUE);
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret)
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);
   
   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }

   return ret;
} /* initPublicKey */

static N8_Status_t initPrivateKey(N8_RSAKeyObject_t   *key_p,
                                  N8_RSAKeyMaterial_t *material_p,
                                  N8_Event_t          *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   unsigned int nBytes;
   N8_Buffer_t  *mod_p;
   N8_Buffer_t  *privateKey_p;
   unsigned int  padding;
   do
   {
      /* to initialize a key object for private non-CRT use only requires the
       * copying of the values for the key, the modulus and their sizes.  No
       * precomputations are required.
       */
      /* the length of the private key must be less than the length of the
       * modulus but we want to pad it out so they appear to be the same.  thus,
       * figure out the length of the number of digits required for the modulus
       * and use that for the private key.
       */
      key_p->privateKeyDigits = BYTES_TO_PKDIGITS(material_p->n.lengthBytes);
      key_p->publicKeyDigits = BYTES_TO_PKDIGITS(material_p->publicKey.lengthBytes);

      /* use the macros to recompute the lengths to use the rounding.  a mere
       * assignment from material_p lengths is not sufficient. */
      key_p->privateKeyLength = PKDIGITS_TO_BYTES(key_p->privateKeyDigits);
      key_p->publicKeyLength = PKDIGITS_TO_BYTES(key_p->publicKeyDigits);

      DBG(("initPrivateKey\n"));
      DBG(("public key length: %d\n", key_p->publicKeyLength));
      DBG(("private key length: %d\n", key_p->privateKeyLength));

      /* allocate kernel memory space for n and the private key.  */
      nBytes = 2 * NEXT_WORD_SIZE(key_p->privateKeyLength);

      /* the kernel memory allocated here is freed in the call to
       * N8_RSAFreeKey.  DO NOT add it to the free list here. */
      key_p->kmem_p = N8_KMALLOC_PK(nBytes);
      CHECK_OBJECT(key_p->kmem_p, ret);

      mod_p = (N8_Buffer_t *) key_p->kmem_p->VirtualAddress;
      memset(mod_p, 0, nBytes);
      privateKey_p = mod_p + NEXT_WORD_SIZE(key_p->privateKeyLength);
      key_p->n = key_p->kmem_p->PhysicalAddress;
      key_p->key = key_p->n + NEXT_WORD_SIZE(key_p->privateKeyLength);
      /* copy the value of the modulus to the key structure */
      padding = (PK_RSA_N_Byte_Length(key_p) - material_p->n.lengthBytes);
      memcpy(&mod_p[padding],
             material_p->n.value_p,
             material_p->n.lengthBytes);
      padding = (PK_RSA_FULL_LENGTH(key_p->privateKeyDigits) - material_p->privateKey.lengthBytes);
      memcpy(&privateKey_p[padding],
             material_p->privateKey.value_p,
             material_p->privateKey.lengthBytes);
      /* initializing for non-CRT private key requires no further processing.
       * set the event status to finished if called asynchronously. */
      if (event_p != NULL)
      {
         N8_SET_EVENT_FINISHED(event_p, N8_PKP);
      }
   } while(FALSE);

   return ret;
   
} /* initPrivateKey */
/*****************************************************************************
 * initPrivateKeyCRT
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Initialize an RSA private key object for Chinese Remainder Theorem.
 *
 * Use of the CRT requires several values to be pre-computed and requires the
 * knowledge of the values p and q.  Performing RSA Decrypt with CRT is much
 * faster than using the standard exponentiation method.
 *
 *
 *  @param key                 RW:  pointer to the key to be initialized
 *  @param material            RO:  pointer to key material with initialization
 *                                  values. 
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error code if encountered.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
static N8_Status_t initPrivateKeyCRT(N8_RSAKeyObject_t   *key_p,
                                     N8_RSAKeyMaterial_t *material_p,
                                     N8_Event_t          *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Status_t ret = N8_STATUS_OK;  
   int pq_cmp;                   /* result of cmp(p,q) */
   unsigned int nBytes;
   unsigned int padding;
   unsigned long  pAddr;
   char          *vAddr;
   N8_RSAKeyObjectVirtual_t vKey;
   PK_CMD_BLOCK_t    *nextCommandBlock_p = NULL;
   N8_Buffer_t *pb = NULL;
   /*
    * key material must contain public key e, modulus n, and their
    * sizes
    *
    * Tasks to be performed:
    *  0) Allocate the parameter block where the data will live
    *  0.5) p = min(p',q')
    *  0.6) q = max(p',q')
    *  1) Put p into the key object.
    *  2) Put q into the key object.
    *  2a)Put the private key into the key object
    *  3) Put N into the key object.
    *  4) Compute u and put into the key object.
    *  5) Compute dp = d mod ((p-1) mod p) and put it in 
    *     the key object.
    *  6) Compute dq = d mod ((q-1) mod q) and put it in 
    *     the key object.
    *  7) Compute cp = -(p[0]^-1 mod 2^128) mod 2^128 and 
    *     put it into the key object.
    *  8) Compute cq = -(q[0]^-1 mod 2^128) mod 2^128 and 
    *     put it into the key object.
    *  9) Compute R mod p and put it into the key object.
    * 10) Compute R mod q and put it into the key object.
    */

   do
   {
      key_p->privateKeyDigits =
         BYTES_TO_PKDIGITS(material_p->privateKey.lengthBytes);
      key_p->publicKeyDigits =
         BYTES_TO_PKDIGITS(material_p->publicKey.lengthBytes);

      /* use the macros to recompute the lengths to use the rounding.  a mere
       * assignment from material_p lengths is not sufficient. */
      key_p->publicKeyLength = PKDIGITS_TO_BYTES(key_p->publicKeyDigits);
      key_p->privateKeyLength = PKDIGITS_TO_BYTES(key_p->privateKeyDigits);

      DBG(("initPrivateKeyCRT\n"));
      DBG(("public key length: %d\n", key_p->publicKeyLength));
      DBG(("private key length: %d\n", key_p->privateKeyLength));

      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_COMPUTE_U_NUMCMDS +
                                  N8_CB_COMPUTE_DX_NUMCMDS +
                                  N8_CB_COMPUTE_DX_NUMCMDS +
                                  N8_CB_COMPUTE_CX_NUMCMDS +
                                  N8_CB_COMPUTE_CX_NUMCMDS +
                                  N8_CB_COMPUTE_RMODX_NUMCMDS +
                                  N8_CB_COMPUTE_RMODX_NUMCMDS, 
                                  NULL, 0);
      CHECK_RETURN(ret);

      /* if p < q, use them in order */
      pq_cmp = n8_sizedBufferCmp(&material_p->p, &material_p->q);
      if (pq_cmp < 0)
      {
         /* p < q.  use them in order. */
         key_p->pLength = material_p->p.lengthBytes;
         key_p->qLength = material_p->q.lengthBytes;
      }
      else if (pq_cmp > 0)
      {
         /* p > q.  swap them. */
         key_p->pLength = material_p->q.lengthBytes; /* swapped! */
         key_p->qLength = material_p->p.lengthBytes; /* swapped! */
      }
      else
      {
         /* p == q.  error! */
         ret = N8_INVALID_PARAMETER;
         break;
      }
      key_p->pDigits = BYTES_TO_PKDIGITS(key_p->pLength);
      key_p->qDigits = BYTES_TO_PKDIGITS(key_p->qLength);

      /* calculate the left padding for p and q */
      key_p->pPad = PKDIGITS_TO_BYTES(key_p->pDigits) - key_p->pLength;
      key_p->qPad = PKDIGITS_TO_BYTES(key_p->qDigits) - key_p->qLength;

      padding = PK_RSA_N_Byte_Length(key_p) - material_p->n.lengthBytes;

      /* Task 0:  Allocate parameter block.  This block will contain all
       * of the parameters for an RSA operation in the correct form for
       * direct load into the Big Num Cache.  
       */
      nBytes = (NEXT_WORD_SIZE(PK_RSA_Param_Byte_Length(key_p)) + /* parameter block */
                NEXT_WORD_SIZE(PKDIGITS_TO_BYTES(key_p->privateKeyDigits))); /* private Key */

      /* the kernel memory allocated here is freed in the call to
       * N8_RSAFreeKey.  DO NOT add it to the free list here. */
      key_p->kmem_p = N8_KMALLOC_PK(nBytes);
      CHECK_OBJECT(key_p->kmem_p, ret);

      pAddr = key_p->kmem_p->PhysicalAddress;
      vAddr = (N8_Buffer_t *) key_p->kmem_p->VirtualAddress;
      memset(vAddr, 0, nBytes);
      key_p->paramBlock = pAddr;
      vKey.paramBlock = (N8_Buffer_t *) vAddr;

      pAddr += NEXT_WORD_SIZE(PK_RSA_Param_Byte_Length(key_p));
      vAddr += NEXT_WORD_SIZE(PK_RSA_Param_Byte_Length(key_p));
      /* Tasks 1-3 */

      /*
       * set the pointers in the key object to the correct address in the
       * parameter block
       */

      key_p->p        = key_p->paramBlock + PK_RSA_P_Param_Byte_Offset(key_p);
      key_p->q        = key_p->paramBlock + PK_RSA_Q_Param_Byte_Offset(key_p);
      key_p->n        = key_p->paramBlock + PK_RSA_N_Param_Byte_Offset(key_p);
      key_p->u        = key_p->paramBlock + PK_RSA_U_Param_Byte_Offset(key_p); 
      key_p->dp       = key_p->paramBlock + PK_RSA_DP_Param_Byte_Offset(key_p);
      key_p->dq       = key_p->paramBlock + PK_RSA_DQ_Param_Byte_Offset(key_p);
      key_p->R_mod_p  = key_p->paramBlock + PK_RSA_R_MOD_P_Param_Byte_Offset(key_p);
      key_p->R_mod_q  = key_p->paramBlock + PK_RSA_R_MOD_Q_Param_Byte_Offset(key_p);
      key_p->cp       = key_p->paramBlock + PK_RSA_CP_Param_Byte_Offset(key_p);
      key_p->cq       = key_p->paramBlock + PK_RSA_CQ_Param_Byte_Offset(key_p);

      pb = vKey.paramBlock;
      vKey.p       = pb + PK_RSA_P_Param_Byte_Offset(key_p) + key_p->pPad;
      vKey.q       = pb + PK_RSA_Q_Param_Byte_Offset(key_p) + key_p->qPad;
      vKey.n       = pb + PK_RSA_N_Param_Byte_Offset(key_p) + padding;
      vKey.u       = pb + PK_RSA_U_Param_Byte_Offset(key_p) + key_p->qPad; 
      vKey.dp      = pb + PK_RSA_DP_Param_Byte_Offset(key_p) + key_p->pPad;
      vKey.dq      = pb + PK_RSA_DQ_Param_Byte_Offset(key_p) + key_p->qPad;
      vKey.R_mod_p = pb + PK_RSA_R_MOD_P_Param_Byte_Offset(key_p) + key_p->pPad;
      vKey.R_mod_q = pb + PK_RSA_R_MOD_Q_Param_Byte_Offset(key_p) + key_p->qPad;
      vKey.cp      = pb + PK_RSA_CP_Param_Byte_Offset(key_p);
      vKey.cq      = pb + PK_RSA_CQ_Param_Byte_Offset(key_p);

      /*
       * convert the material from BIGNUM to binary byte buffer and copy
       * to the key's parameter block
       */

      /* if p < q, use them in order */
      if (pq_cmp < 0)
      {
         /* p < q.  use them in order. */
         memcpy(vKey.p, material_p->p.value_p, key_p->pLength);
         memcpy(vKey.q, material_p->q.value_p, key_p->qLength);
      }
      else if (pq_cmp > 0)
      {
         /* p > q.  swap them. */
         DBG(("p > q:  swapping\n"));
         memcpy(vKey.p, material_p->q.value_p, key_p->pLength); /* swapped */
         memcpy(vKey.q, material_p->p.value_p, key_p->qLength); /* swapped */
      }
      else
      {
         /* p == q.  error! */
         ret = N8_INVALID_PARAMETER;
         break;
      }
      memcpy(vKey.n, material_p->n.value_p, material_p->n.lengthBytes);

      DBG_PARAM("input only\n", vKey.paramBlock, key_p);
      /*
       * allocate space for the key.  it is not a part of the parameter
       * block
       */

      /* we need to round up to an integral number of digits */
      /* recompute amount of padding for the private key */
      padding = PK_RSA_N_Byte_Length(key_p) - material_p->privateKey.lengthBytes;
      key_p->key = pAddr;
      memcpy(vAddr + padding, material_p->privateKey.value_p,
             material_p->privateKey.lengthBytes);

      /* Task 4: compute u = (p^-1) mod q  */
      ret = cb_computeU(req_p, key_p->p, key_p->q, key_p->u,
                        PK_RSA_P_Byte_Length(key_p),
                        PK_RSA_Q_Byte_Length(key_p),
                        req_p->PK_CommandBlock_ptr, &nextCommandBlock_p);
      CHECK_RETURN(ret);

      /*  Task 5: compute dp = mod((p-1) mod p) */
      ret = cb_computeDX(req_p, key_p->p, key_p->key, key_p->dp,
                         key_p->privateKeyLength,
                         PK_RSA_P_Byte_Length(key_p), 
                         nextCommandBlock_p, &nextCommandBlock_p,
                         key_p->unitID);
      CHECK_RETURN(ret);

      /*  Task 6: compute dq = mod((q-1) mod q) */
      ret = cb_computeDX(req_p, key_p->q, key_p->key, key_p->dq,
                         key_p->privateKeyLength,
                         PK_RSA_Q_Byte_Length(key_p), 
                         nextCommandBlock_p, &nextCommandBlock_p,
                         key_p->unitID);
      CHECK_RETURN(ret);

      /*  Task 7: Compute cp = -(p[0]^-1 mod 2^128) mod 2^128 and  */
      /*      put it into the key object. */
      ret = cb_computeCX(req_p, key_p->p, key_p->cp,
                         PK_RSA_P_Byte_Length(key_p), 
                         nextCommandBlock_p, &nextCommandBlock_p,
                         key_p->unitID,
                         N8_FALSE);
      CHECK_RETURN(ret);

      /*  Task 8: Compute cq = -(q[0]^-1 mod 2^128) mod 2^128 and  */
      /*      put it into the key object. */
      ret = cb_computeCX(req_p, key_p->q, key_p->cq,
                         PK_RSA_Q_Byte_Length(key_p), 
                         nextCommandBlock_p, &nextCommandBlock_p,
                         key_p->unitID,
                         N8_FALSE);
      CHECK_RETURN(ret);

      /*  Task 9: Compute R mod p and put it into the key object. */
      ret = cb_computeRmodX(req_p, key_p->p, key_p->R_mod_p, 
                            PK_RSA_P_Byte_Length(key_p),
                            nextCommandBlock_p, &nextCommandBlock_p, FALSE);
      CHECK_RETURN(ret);

      /*  Task 10: Compute R mod q and put it into the key object. */
      ret = cb_computeRmodX(req_p, key_p->q, key_p->R_mod_q,
                            PK_RSA_Q_Byte_Length(key_p), 
                            nextCommandBlock_p, &nextCommandBlock_p, TRUE);
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

      if ((N8_Buffer_t*) vKey.paramBlock != NULL)
      {
#ifdef N8DEBUG
         n8_displayBuffer(material_p->privateKey.value_p,
                          key_p->privateKeyLength, "d -- private key");
#endif         
         DBG_PARAM("Computed Parameter Block:\n", vKey.paramBlock, key_p);
      }
   } while (FALSE);

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }

   return ret;
} /* initPrivateKeyCRT */

#if 0
/* Open Crypto key setup for MOD EXP CRT (RSA MOD EXP) */
/* dP, dQ, and qinv are supplied.,
 * but the public and private keys are not...
 *
 * The N8 claims to require N!!! (the public key, = pq).
 * That throws a bit of a wrench in the works...
 */
static N8_Status_t initPrivateKeyCRT_OC(N8_RSAKeyObject_t *key_p,
                                     N8_RSAKeyMaterial_t  *material_p,
				     N8_SizedBuffer_t     *dp_p;             
				     N8_SizedBuffer_t     *dq_p;
                                     N8_Event_t           *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Status_t ret = N8_STATUS_OK;  
   int pq_cmp;                   /* result of cmp(p,q) */
   unsigned int nBytes;
   unsigned int padding;
   unsigned long  pAddr;
   char          *vAddr;
   N8_RSAKeyObjectVirtual_t vKey;
   PK_CMD_BLOCK_t    *nextCommandBlock_p = NULL;
   N8_Buffer_t *pb = NULL;
   /*
    * key material must contain public key e, modulus n, and their
    * sizes
    *
    * Tasks to be performed:
    *  0) Allocate the parameter block where the data will live
    *  0.5) p = min(p',q')
    *  0.6) q = max(p',q')
    *  1) Put p into the key object.
    *  2) Put q into the key object.
    *  3) Compute u and put into the key object.
    *  4) Put dp (=d mod ((p-1) mod p)) in the key object.
    *  5) Put dq (= d mod ((q-1) mod q)) the key object.
    *  6) Compute cp = -(p[0]^-1 mod 2^128) mod 2^128 and 
    *     put it into the key object.
    *  7) Compute cq = -(q[0]^-1 mod 2^128) mod 2^128 and 
    *     put it into the key object.
    *  8) Compute R mod p and put it into the key object.
    *  9) Compute R mod q and put it into the key object.
    */

   do
   {
      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_COMPUTE_U_NUMCMDS +
                                  N8_CB_COMPUTE_CX_NUMCMDS +
                                  N8_CB_COMPUTE_CX_NUMCMDS +
                                  N8_CB_COMPUTE_RMODX_NUMCMDS +
                                  N8_CB_COMPUTE_RMODX_NUMCMDS, 
                                  NULL, 0);
      CHECK_RETURN(ret);

      /* if p < q, use them in order */
      pq_cmp = n8_sizedBufferCmp(&material_p->p, &material_p->q);
      if (pq_cmp < 0)
      {
         /* p < q.  use them in order. */
         key_p->pLength = material_p->p.lengthBytes;
         key_p->qLength = material_p->q.lengthBytes;
      }
      else if (pq_cmp > 0)
      {
         /* p > q.  swap them. */
         key_p->pLength = material_p->q.lengthBytes; /* swapped! */
         key_p->qLength = material_p->p.lengthBytes; /* swapped! */
      }
      else
      {
         /* p == q.  error! */
         ret = N8_INVALID_PARAMETER;
         break;
      }
      key_p->pDigits = BYTES_TO_PKDIGITS(key_p->pLength);
      key_p->qDigits = BYTES_TO_PKDIGITS(key_p->qLength);

      /* calculate the left padding for p and q */
      key_p->pPad = PKDIGITS_TO_BYTES(key_p->pDigits) - key_p->pLength;
      key_p->qPad = PKDIGITS_TO_BYTES(key_p->qDigits) - key_p->qLength;

      padding = PK_RSA_N_Byte_Length(key_p) - material_p->n.lengthBytes;

      /* Task 0:  Allocate parameter block.  This block will contain all
       * of the parameters for an RSA operation in the correct form for
       * direct load into the Big Num Cache.  
       */
      nBytes = NEXT_WORD_SIZE(PK_RSA_Param_Byte_Length(key_p)); /* parameter block */

      /* the kernel memory allocated here is freed in the call to
       * N8_RSAFreeKey.  DO NOT add it to the free list here. */
      key_p->kmem_p = N8_KMALLOC_PK(nBytes);
      CHECK_OBJECT(key_p->kmem_p, ret);

      pAddr = key_p->kmem_p->PhysicalAddress;
      vAddr = (N8_Buffer_t *) key_p->kmem_p->VirtualAddress;
      memset(vAddr, 0, nBytes);
      key_p->paramBlock = pAddr;
      vKey.paramBlock = (N8_Buffer_t *) vAddr;

      pAddr += NEXT_WORD_SIZE(PK_RSA_Param_Byte_Length(key_p));
      vAddr += NEXT_WORD_SIZE(PK_RSA_Param_Byte_Length(key_p));
      /* Tasks 1-2 */

      /*
       * set the pointers in the key object to the correct address in the
       * parameter block
       */

      key_p->p        = key_p->paramBlock + PK_RSA_P_Param_Byte_Offset(key_p);
      key_p->q        = key_p->paramBlock + PK_RSA_Q_Param_Byte_Offset(key_p);
      key_p->n        = key_p->paramBlock + PK_RSA_N_Param_Byte_Offset(key_p);
      key_p->u        = key_p->paramBlock + PK_RSA_U_Param_Byte_Offset(key_p); 
      key_p->dp       = key_p->paramBlock + PK_RSA_DP_Param_Byte_Offset(key_p);
      key_p->dq       = key_p->paramBlock + PK_RSA_DQ_Param_Byte_Offset(key_p);
      key_p->R_mod_p  = key_p->paramBlock + PK_RSA_R_MOD_P_Param_Byte_Offset(key_p);
      key_p->R_mod_q  = key_p->paramBlock + PK_RSA_R_MOD_Q_Param_Byte_Offset(key_p);
      key_p->cp       = key_p->paramBlock + PK_RSA_CP_Param_Byte_Offset(key_p);
      key_p->cq       = key_p->paramBlock + PK_RSA_CQ_Param_Byte_Offset(key_p);

      pb = vKey.paramBlock;
      vKey.p       = pb + PK_RSA_P_Param_Byte_Offset(key_p) + key_p->pPad;
      vKey.q       = pb + PK_RSA_Q_Param_Byte_Offset(key_p) + key_p->qPad;
      vKey.n       = pb + PK_RSA_N_Param_Byte_Offset(key_p) + padding;
      vKey.u       = pb + PK_RSA_U_Param_Byte_Offset(key_p) + key_p->qPad; 
      vKey.dp      = pb + PK_RSA_DP_Param_Byte_Offset(key_p) + key_p->pPad;
      vKey.dq      = pb + PK_RSA_DQ_Param_Byte_Offset(key_p) + key_p->qPad;
      vKey.R_mod_p = pb + PK_RSA_R_MOD_P_Param_Byte_Offset(key_p) + key_p->pPad;
      vKey.R_mod_q = pb + PK_RSA_R_MOD_Q_Param_Byte_Offset(key_p) + key_p->qPad;
      vKey.cp      = pb + PK_RSA_CP_Param_Byte_Offset(key_p);
      vKey.cq      = pb + PK_RSA_CQ_Param_Byte_Offset(key_p);

      /*
       * convert the material from BIGNUM to binary byte buffer and copy
       * to the key's parameter block
       */

      /* if p < q, use them in order */
      if (pq_cmp < 0)
      {
         /* p < q.  use them in order. */
         memcpy(vKey.p, material_p->p.value_p, key_p->pLength);
         memcpy(vKey.q, material_p->q.value_p, key_p->qLength);
      }
      else if (pq_cmp > 0)
      {
         /* p > q.  swap them. */
         DBG(("p > q:  swapping\n"));
         memcpy(vKey.p, material_p->q.value_p, key_p->pLength); /* swapped */
         memcpy(vKey.q, material_p->p.value_p, key_p->qLength); /* swapped */
      }
      else
      {
         /* p == q.  error! */
         ret = N8_INVALID_PARAMETER;
         break;
      }

      DBG_PARAM("input only\n", vKey.paramBlock, key_p);

      /* Task 3: compute u = (p^-1) mod q  */
      ret = cb_computeU(req_p, key_p->p, key_p->q, key_p->u,
                        PK_RSA_P_Byte_Length(key_p),
                        PK_RSA_Q_Byte_Length(key_p),
                        req_p->PK_CommandBlock_ptr, &nextCommandBlock_p);
      CHECK_RETURN(ret);

      /*  Task 4: compute dp = mod((p-1) mod p) */
      memcpy(vKey.dp, dp_p->value_p, dp_p->lengthBytes);

      /*  Task 5: compute dq = mod((q-1) mod q) */
      memcpy(vKey.dq, dq_p->value_p, dq_p->lengthBytes);

      /*  Task 6: Compute cp = -(p[0]^-1 mod 2^128) mod 2^128 and  */
      /*      put it into the key object. */
      ret = cb_computeCX(req_p, key_p->p, key_p->cp,
                         PK_RSA_P_Byte_Length(key_p), 
                         nextCommandBlock_p, &nextCommandBlock_p,
                         key_p->unitID,
                         N8_FALSE);
      CHECK_RETURN(ret);

      /*  Task 7: Compute cq = -(q[0]^-1 mod 2^128) mod 2^128 and  */
      /*      put it into the key object. */
      ret = cb_computeCX(req_p, key_p->q, key_p->cq,
                         PK_RSA_Q_Byte_Length(key_p), 
                         nextCommandBlock_p, &nextCommandBlock_p,
                         key_p->unitID,
                         N8_FALSE);
      CHECK_RETURN(ret);

      /*  Task 8: Compute R mod p and put it into the key object. */
      ret = cb_computeRmodX(req_p, key_p->p, key_p->R_mod_p, 
                            PK_RSA_P_Byte_Length(key_p),
                            nextCommandBlock_p, &nextCommandBlock_p, FALSE);
      CHECK_RETURN(ret);

      /*  Task 9: Compute R mod q and put it into the key object. */
      ret = cb_computeRmodX(req_p, key_p->q, key_p->R_mod_q,
                            PK_RSA_Q_Byte_Length(key_p), 
                            nextCommandBlock_p, &nextCommandBlock_p, TRUE);
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

      if ((N8_Buffer_t*) vKey.paramBlock != NULL)
      {
         DBG_PARAM("Computed Parameter Block:\n", vKey.paramBlock, key_p);
      }
   } while (FALSE);

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }

   return ret;
} /* initPrivateKeyCRT_OC */
#endif

/*****************************************************************************
 * initPrivateSKSKey
 *****************************************************************************/
/** @ingroup n8_rsa
 * @brief Initialize an RSA private key object.
 *
 * <More detailed description of the function including any unusual algorithms
 * or suprising details.>
 *
 *  @param key_p               RW:  pointer to the key to be initialized
 *  @param material_p          RW:  pointer to key material with initialization
 *                                  values
 *  @param event_p             RW:  pointer to event structure
 *
 * @par Externals
 *    None
 *
 * @return
 *    Statue.  Error code if encountered.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t initPrivateSKSKey(N8_RSAKeyObject_t   *key_p,
                                     N8_RSAKeyMaterial_t *material_p,
                                     N8_Event_t          *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      /* verify the required parameters are not null */
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(material_p, ret);
      /* verify the SKS type is correct */
      if (material_p->SKSKeyHandle.key_type != N8_RSA_VERSION_1_KEY)
      {
         ret = N8_INCONSISTENT;
         break;
      }
      key_p->publicKeyLength = PKDIGITS_TO_BYTES(material_p->SKSKeyHandle.key_length);
      key_p->privateKeyLength = PKDIGITS_TO_BYTES(material_p->SKSKeyHandle.key_length);
/*      key_p->publicKeyLength = material_p->publicKey.lengthBytes;
      key_p->privateKeyLength = material_p->privateKey.lengthBytes;*/
      /* copy the SKS key handle from the key material to the key object */
      memcpy(&key_p->SKSKeyHandle,
             &material_p->SKSKeyHandle,
             sizeof(N8_SKSKeyHandle_t));
      /* initializing for SKS requires no further processing.  set the event
       * status to finished if called asynchronously. */
      if (event_p != NULL)
      {
         N8_SET_EVENT_FINISHED(event_p, N8_PKP);
      }
   } while (FALSE);
   return ret;
} /* initPrivateSKSKey */
