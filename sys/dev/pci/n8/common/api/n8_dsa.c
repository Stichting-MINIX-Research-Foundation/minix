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

static char const n8_id[] = "$Id: n8_dsa.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_dsa.c
 *  @brief Public DSA functions.
 *
 * Implementation of all public DSA functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 04/11/03 brr   Modified initDSAPrivateKey to overwrite the generic hardware
 *                error returned by HANDLE_EVENT with INVALID_KEY. (BUG 761)
 * 09/10/02 brr   Set command complete bit on last command block.
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/15/02 brr   Reworked to remove chain request, RNG now returns random bytes.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 05/01/02 brr   Memset structures after KMALLOC.
 * 04/05/02 bac   Moved the checking of parameters for initialization to the
 *                validation routine for correct operation.  (BUG 513)
 * 04/01/02 brr   Validate modulus, divisor, and generator pointers before
 *                use in N8_DSAInitializeKey.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 03/08/02 msz   Get random bytes using a direct physical address.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/25/02 brr   Remove unused variables.
 * 02/20/02 brr   Removed references to the queue structure.
 * 01/29/02 bac   Changed DSASign to use N8_GetRandomBytes directly if we are
 *                using the underlying OS random number generator (not our
 *                hardware or behavioral model).
 * 01/31/02 brr   Eliminated the memory allocation for postProcessingData.
 * 12/11/01 mel   Fixed bug #420: There is a new requirement (to appear in rev 
 *                J of the API Spec) that states for a DSA key to be eligible 
 *                for use in the SKS the modulus length must either be an
 *                even multiple of 16 -OR- an even multiple of 16 minus 8. Added
 *                check for SKS key. 
 *                Fixed bug #412: Set SKS key length from SKS key material  
 * 12/11/01 mel   Fixed bug #380: Added extra checks for DSAInitializeKey 
 * 11/30/01 msz   Fixed bug #377 : Added check structure in N8_DSAFreeKey
 * 11/30/01 mel   Fixed bug #396 : N8_DSAInitializeKey segfaults when private 
 *                key information not needed.
 * 11/29/01 msz   Fixed bug #374 : Check that r and q are non zero.
 *                also free postProcessingData_p in freeRequest so there is
 *                no memory leak in error cases.
 * 11/28/01 mel   Fixed bug #372 : DSAInitializeKey should return
 *                N8_INVALID_KEY when x>=q.
 * 11/19/01 bac   Corrected checking of DSA constraints.  Bug #169.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/15/01 mel   Fixed bug #317 : No SKS support for DSA keys.
 * 11/08/01 mel   Added unit ID parameter to commend block calls (bug #289).
 * 11/06/01 dkm   Corrected error return for invalid key enum.
 * 11/06/01 hml   Added the structure verification.
 * 10/31/01 bac   Added initDSAPrivateSKSKey and support for it.
 * 10/30/01 dkm   Cleaned up minor error condition bugs.
 * 10/18/01 hml   Removed several redundant calls to AddMemoryStructToFree.
 * 10/11/01 bac   Rearranged some code for proper memory freeing.
 * 10/10/01 brr   Fixed memory leaks from N8_KMALLOC.
 * 10/04/01 brr   Removed warnings exposed when optimization turned on.
 * 10/02/01 bac   Changed N8_CCM_ enums to #defines as they define masks.
 * 10/01/01 hml   Added multi-unit support.
 * 09/21/01 mel   Cleaned.
 * 09/17/01 mel   Changed command block strategy. Instead of queuing every
 *                time we built command block, put all command blocks together 
 *                and queue them once.
 * 09/10/01 bac   Test for public key and modulus lengths in
 *                N8_DSAInitializeKey (BUG #129).
 * 09/14/01 mel   Added event parameter to N8_DSAInitializeKey.
 * 09/05/01 bac   Minor formatting changes.
 * 08/24/01 bac   Changed all interfaces to the cb commands to pre-allocate the
 *                command buffer space.
 * 08/10/01 bac   Fixed bug in N8_DSASign where the value of the random number N
 *                was compared to q.  This check is done in the hardware and was
 *                removed from the API code.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/30/01 bac   Use queue_p in all macro length calculations.
 * 07/20/01 bac   Changed calls to create__RequestBuffer to pass the chip id.
 * 07/12/01 bac   Deleted unused variables.
 * 07/13/01 mel   Fixed public Key initialization and added verification to
 *                key initialization.
 * 06/29/01 mel   Fixed comments.
 * 06/28/01 bac   Added back some changes lost in the merge to round up to the
 *                next word size for memory allocation.
 * 06/26/01 bac   Bug fixes for computation of 'n' random value.  Also moved
 *                copy backs to result handlers for async compatibility.
 * 06/25/01 bac   Lots of changes to affect functionality with the v1.0.1 QMgr
 * 06/22/01 bac   More kernel memory fixes.
 * 06/20/01 mel   Corrected use of kernel memory.
 * 05/22/01 mel   Original version.
 ****************************************************************************/
/** @defgroup dsa DSA Functions.
 */


#include "n8_common.h"
#include "n8_pub_errors.h"
#include "n8_dsa.h"
#include "n8_pk_common.h"
#include "n8_enqueue_common.h"
#include "n8_util.h"
#include "n8_API_Initialize.h"
#include "n8_malloc_common.h"

#include "n8_cb_dsa.h"
#include "n8_cb_rsa.h"


/*
 * local predeclarations
 */
static N8_Status_t initDSAPublicKey(N8_DSAKeyObject_t *key_p,
                                    N8_DSAKeyMaterial_t *material_p,
                                    N8_Event_t            *event_p);

static N8_Status_t initDSAPrivateKey(N8_DSAKeyObject_t *key_p,
                                     N8_DSAKeyMaterial_t *material_p,
                                     N8_Event_t            *event_p);
static N8_Status_t initDSAPrivateSKSKey(N8_DSAKeyObject_t *key_p,
                                        N8_DSAKeyMaterial_t *material_p,
                                        N8_Event_t            *event_p);

static N8_Status_t checkModulusLength(unsigned int length);

/* structure for toting around destination result buffers for copy in result
 * handler for N8_DSASign */
typedef struct 
{
   N8_Buffer_t *rFrom_p;
   N8_Buffer_t *sFrom_p;
   N8_Buffer_t *rTo_p;
   N8_Buffer_t *sTo_p;
} dsaSignPostDataStruct_t;

/* structure for toting around buffers to be compared and result destination in
  * result handler for N8_DSAVerify */
typedef struct
{
   N8_Buffer_t *computed_p;
   N8_Buffer_t *expected_p;
   int          size;
   N8_Boolean_t   *verify_p;
} dsaVerifyPostDataStruct_t;

/**********************************************************************
  * resultHandlerDSAVerify
  *
  * Description:
  * This function is called by the Public Key Request Queue handler when
  * either the request is completed successfully and all the commands
  * copied back to the command blocks allocated by the API, or when
  * the Simon has encountered an error with one of the commands such 
  * that the Simon has locked up.
  *
  * Note this function will have to be NON-BLOCKING or it will lock up the
  * queue handler!
  * 
  * jke
  *
  **********************************************************************/
static void resultHandlerDSAVerify(API_Request_t* req_p)
{
   dsaVerifyPostDataStruct_t *postData_p = NULL;

   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("resultHandlerDSAVerify call-back with success\n"));

      postData_p = (dsaVerifyPostDataStruct_t *) req_p->postProcessingData_p;
      if (memcmp(postData_p->expected_p,
                 postData_p->computed_p,
                 postData_p->size) == 0)
      {
         *(postData_p->verify_p) = N8_TRUE;
      }
   }
   else
   {
      RESULT_HANDLER_WARNING("n8_dsa::resultHandlerDSAVerify",
                             req_p);
   }

   /* do not free the request.  it is freed by the event processor. */
}

/**********************************************************************
  * resultHandlerDSASign
  *
  * Description:
  * This function is called by the Public Key Request Queue handler when
  * either the request is completed successfully and all the commands
  * copied back to the command blocks allocated by the API, or when
  * the Simon has encountered an error with one of the commands such 
  * that the Simon has locked up.
  *
  * Note this function will have to be NON-BLOCKING or it will lock up the
  * queue handler!
  * 
  * jke
  *
  **********************************************************************/
static void resultHandlerDSASign(API_Request_t* req_p)
{
   dsaSignPostDataStruct_t *postData_p = NULL;
   int skipLength;
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("resultHandlerDSASign call-back with success\n"));
      postData_p = (dsaSignPostDataStruct_t *) req_p->postProcessingData_p;
      /* Only copy back the last NUMBER_OF_BYTES_IN_VALUE as the rest are not
       * meaningful */
      skipLength = PK_DSA_R_Byte_Length - DSA_SIGN_LENGTH;
      memcpy(postData_p->rTo_p,
             postData_p->rFrom_p + skipLength,
             DSA_SIGN_LENGTH);
      skipLength = PK_DSA_S_Byte_Length - DSA_SIGN_LENGTH;
      memcpy(postData_p->sTo_p,
             postData_p->sFrom_p + skipLength,
             DSA_SIGN_LENGTH);
   }
   else
   {
      RESULT_HANDLER_WARNING("n8_dsa::resultHandlerDSASign",
                             req_p);
   }

   /* do not free the request.  it is freed by the event processor. */
} /* resultHandlerDSASign */

/*****************************************************************************
 * n8_testValueInUpperRange
 *****************************************************************************/
/** @ingroup dsa
 * @brief Test that a given value is in the upper range.
 *
 * For some cryptographic quantities, a restriction exists that the value must
 * be greater than 2**(L-1) where L is the length in bits.  Basically, this
 * means the value must greater than the mid-point of the range of values.
 * Specifically, the high-order bit must be 1 and at least one subsequent bit
 * must be 1.
 *
 *  @param p                   RO:  Pointer to the sized buffer to be checked. 
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status of comparison:  N8_STATUS_OK or N8_INVALID_KEY.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t n8_testValueInUpperRange(const N8_SizedBuffer_t *p)
{
   N8_Status_t ret = N8_INVALID_KEY;
   int i;

   do
   {
      /* test to see if the uppermost bit is set.  if so, the value will be in
       * the range (0x8 - 0xF). */
      if (p->value_p[0] < 0x8)
      {
         /* an error exists.  keep the error return code and break. */
         break;
      }
      for (i = 0; i < p->lengthBytes; i++)
      {
         if (p->value_p[i] != 0)
         {
            /* at least one bit has been found set.  return N8_STATUS_OK. */
            ret = N8_STATUS_OK;
            break;
         }
      }
   } while (FALSE);
   
   return ret;
} /* n8_testValueInUpperRange */
/*****************************************************************************
 * n8_DSAValidateKey
 *****************************************************************************/
/** @ingroup dsa
 * @brief Test to see if input in N8_DSAKeyMaterial_t is valid.
 *
 * The requirements for a valid set of values for a DSA key are checked.  If any
 * of the constraints are violated an error is returned
 *
 *  @param keyMaterial_p       RO:  Pointer to key material.
 *  @param type                RO:  Type of key
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status of validation.
 *
 * @par Errors
 *    N8_STATUS_OK, N8_INVALID_KEY, or N8_INVALID_KEY_SIZE
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t n8_DSAValidateKey(const N8_DSAKeyMaterial_t *material_p,
                              const N8_KeyType_t type)
{
   N8_Status_t ret = N8_STATUS_OK;
   int value_cmp;

   do
   {
      /* ensure we have all of the correct parameters for the key type */
      switch(type)
      {
         case N8_PUBLIC:
            CHECK_OBJECT(material_p->publicKey.value_p, ret);
            CHECK_OBJECT(material_p->p.value_p, ret);
            CHECK_OBJECT(material_p->q.value_p, ret);
            CHECK_OBJECT(material_p->g.value_p, ret);
            break;

         case N8_PRIVATE:
            CHECK_OBJECT(material_p->privateKey.value_p, ret);
            CHECK_OBJECT(material_p->p.value_p, ret);
            CHECK_OBJECT(material_p->q.value_p, ret);
            CHECK_OBJECT(material_p->g.value_p, ret);
            break;
         case N8_PRIVATE_SKS:
            break;
         default:
            ret = N8_INVALID_ENUM;
            break;
      }
      CHECK_RETURN(ret);

      /*
       * check the modulus, p
       */

      /* first check the length */
      ret = checkModulusLength(material_p->p.lengthBytes);
      CHECK_RETURN(ret);

      /* for a p of length L bits, p > 2**(L-1) */
      ret = n8_testValueInUpperRange(&material_p->p);
      CHECK_RETURN(ret);

      /*
       * check the generator, g
       */
      
      /* len(g) == len(p) */
      if (material_p->g.lengthBytes != material_p->p.lengthBytes)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }
      /* g < p */
      value_cmp = n8_sizedBufferCmp(&material_p->g, &material_p->p);
      if (value_cmp >= 0) 
      {
         ret = N8_INVALID_KEY;
         break;
      }

      /*
       * check q, a prime divisor of (p - 1)
       */

      /* len(q) == 20  */
      if (material_p->q.lengthBytes != N8_DSA_PRIVATE_KEY_LENGTH)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }
      /* for a q of length L bits, q > 2**(L-1) */
      ret = n8_testValueInUpperRange(&material_p->q);
      CHECK_RETURN(ret);

      if (type == N8_PRIVATE)
      {
         /* check the private key */
         if (material_p->privateKey.lengthBytes != N8_DSA_PRIVATE_KEY_LENGTH)
         {
            ret = N8_INVALID_KEY_SIZE;
            break;
         }
         /* private key < q */
         value_cmp = n8_sizedBufferCmp(&material_p->privateKey, &material_p->q);
         if (value_cmp >= 0)
         {
            ret = N8_INVALID_KEY;
            break;
         }
      }
      else /* the type is pre-screened and must be N8_PUBLIC at this point. */
      {
         /* check the public key */
         if (material_p->publicKey.lengthBytes != material_p->p.lengthBytes)
         {
            ret = N8_INVALID_KEY_SIZE;
            break;
         }
         /* public key < p */
         value_cmp = n8_sizedBufferCmp(&material_p->publicKey, &material_p->p);
         if (value_cmp >= 0)
         {
            ret = N8_INVALID_KEY;
            break;
         }
      }
      
   } while (FALSE);

   return ret;

} /* n8_DSAValidateKey */

/**********************************************************************
  * N8_DSAInitializeKey
  ***********************************************************************/
 /** @ingroup dsa
  * @brief Initializes the specified DSAKeyObject so that it can be used
  * in DSA sign / verify operations.
  *
  * Description:
  * The KeyMaterial object (structure) contains the appropriate key
  * (x or y), modulus p,  q and g values depending on the type of key
  * being initialized.  The enumeration parameter KeyType specifies what
  * kind of key is being set up and thus which fields in the KeyMaterial
  * object must be valid. There are 3 legal values for KeyType: "Public",
  * "Private", and "SKS Private". 
  *
  * 
  * @param key_p          RW: The caller allocated DSAKeyObject, initialized
  *                           by this call with the appropriate DSA key 
  *                           material and pre-computed DSA constants 
  *                           depending on the value of the KeyType parameter.
  * @param type           RO: An enumeration value specifying the type of
  *                           key to initialize. Valid values are: Public,
  *                           Private, and SKSPrivate.
  * @param material_p     RO: Pointer to the key material to use in initializing 
  *                           DSAKeyObject.  The valid information that must be
  *                           supplied depends on the value of of KeyType.
  *
  *
  * @return 
  *    ret - returns N8_STATUS_OK if successful or Error value.
  *
  * @par Errors:
  *          N8_INVALID_OBJECT   -   DSA key or key material object is NULL<BR>
  *          N8_INVALID_ENUM     -   The value of  KeyType  is not one of
  *                                  the values Public, Private,  or SKSPrivate.
  *          N8_UNIMPLEMENTED_FUNCTION - means that PRIVATE SKS protocol not
  *                                  implemented in this release.
  *
  * @par Assumptions:
  *      None.
  **********************************************************************/
N8_Status_t N8_DSAInitializeKey(N8_DSAKeyObject_t     *key_p,
                                N8_KeyType_t           type,
                                N8_DSAKeyMaterial_t   *material_p,
                                N8_Event_t            *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(material_p, ret);
      memset(key_p, 0x0, sizeof(N8_DSAKeyObject_t));

      /* The unit specifier is found in a different place depending on
         the requested type */
      switch (type)
      {
         case N8_PUBLIC:
         case N8_PRIVATE:
            key_p->unitID = material_p->unitID;
            break;

         case N8_PRIVATE_SKS:
            key_p->unitID = material_p->SKSKeyHandle.unitID, 

            /* Copy all of the SKS data into the key */
            memcpy((void *) &(key_p->SKSKeyHandle),
                   (const void *) &(material_p->SKSKeyHandle),
                   sizeof(N8_SKSKeyHandle_t));
            break;

         default:
            ret = N8_INVALID_ENUM;
            break;
      }

      /*
       * Verify parameters if this is not SKS call
       */
      if (type != N8_PRIVATE_SKS)
      {
         ret = n8_DSAValidateKey(material_p, type);
         CHECK_RETURN(ret);
      }

      key_p->keyType = type;
      switch(type)
      {
         case N8_PUBLIC:
            ret = initDSAPublicKey(key_p, material_p, event_p);
            break;

         case N8_PRIVATE:
            ret = initDSAPrivateKey(key_p, material_p, event_p);
            break;
         case N8_PRIVATE_SKS:
            ret = initDSAPrivateSKSKey(key_p, material_p, event_p);
            break;
         default:
            ret = N8_INVALID_ENUM;
            break;
      }
      CHECK_RETURN(ret);

   } while(FALSE);

   if (ret == N8_STATUS_OK)
   {
      /* Set the structure ID */
      key_p->structureID = N8_DSA_STRUCT_ID;
   }
   return ret;
} /* N8_DSAInitializeKey */

/**********************************************************************
  * N8_DSASign
  ***********************************************************************/
 /** @ingroup dsa
  * @brief Digitally signs the message whose SHA-1 hash value is MessageHash
  * using the Digital Signature Algorithm (DSA) as defined by the Digital
  * Signature Standard [DSS].
  *
  * Description:
  * Digitally signs the message whose SHA-1 hash value is MessageHash
  * using the Digital Signature Algorithm (DSA) as defined by the Digital
  * Signature Standard [DSS]. MessageHash is the 20 byte SHA-1 hash value
  * of the message being signed (for example, as computed by 
  * N8_HashCompleteMessage). 
  * The two DSA defined signature values, r and s, are returned in RValue
  * and SValue respectively. 
  *      r = (gN mod p)  mod q
  *      s = (N-1  * (h + x*r) ) mod q
  * where N = a random value (supplied by the system), h = the SHA-1 hash
  * value MessageHash, and x, g, p, and q are the DSA key parameters defined
  * by DSAKeyObject. DSAKeyObject must have been previously initialized via
  * an appropriate call to N8_DSAInitializeKey.
  *
  * 
  * Parameters:
  * @param key_p          RO:    The previously initialized DSAKeyObject
  *                              containing the DSA key materials to be used.
  * @param msgHash_p      RO:    The SHA-1 hash of the message being signed; 
  *                              always 20 bytes.        
  * @param rValue_p       WO:    The DSA r value.  This is always 20 bytes 
  *                              in length. 
  * @param sValue_p       WO:    The DSA s value.  This is always 20 bytes 
  *                              in length.
  * @param event_p        RW:    On input, if null the call is synchronous 
  *                              and no event is returned. The operation 
  *                              is complete when the call returns. If 
  *                              non-null, then the call is asynchronous; 
  *                              an event is returned that can be used to 
  *                              determine when the operation completes.
  *
  * @return 
  *    ret - returns N8_STATUS_OK if successful or Error value.
  *
  * @par Errors:
  *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
  *          N8_INVALID_KEY      -   The DSAKeyObject is not a valid key object
  *                                  for this operation.
  *   
  *
  * @par Assumptions:
  **********************************************************************/
N8_Status_t N8_DSASign(N8_DSAKeyObject_t *key_p,
                       N8_Buffer_t       *msgHash_p,
                       N8_Buffer_t       *rValue_p,
                       N8_Buffer_t       *sValue_p,
                       N8_Event_t        *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;
   int nBytes;
   int modulusDigits;
   N8_Buffer_t *r_p;
   N8_Buffer_t *s_p;
   N8_Buffer_t *mh_p;
   N8_Buffer_t *n_p;
   N8_Buffer_t *paramBlock_p;
   uint32_t    r_a;
   uint32_t    s_a;
   uint32_t    mh_a;
   uint32_t    n_a;
   uint32_t    paramBlock_a;
   dsaSignPostDataStruct_t *postData_p = NULL;

   do
   {

      ret = N8_preamble();
      CHECK_RETURN(ret);
      
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(msgHash_p, ret);
      CHECK_OBJECT(rValue_p, ret);
      CHECK_OBJECT(sValue_p, ret);
      CHECK_STRUCTURE(key_p->structureID, N8_DSA_STRUCT_ID, ret);

      /* sign operation can be performed only with private or SKS private key*/
      switch (key_p->keyType)
      {
         case N8_PRIVATE:
         case N8_PRIVATE_SKS:
            break;
         default:
            ret = N8_INVALID_KEY;
            break;
      }
      CHECK_RETURN(ret);

      modulusDigits = BYTES_TO_PKDIGITS(key_p->modulusLength);

         /* The parameter will be taken solely from the SKS.  All other entries
          * need to be created.
          */
      nBytes =
         (NEXT_WORD_SIZE(PK_DSA_R_Byte_Length) +   /* R */
          NEXT_WORD_SIZE(PK_DSA_S_Byte_Length) +   /* S */
          NEXT_WORD_SIZE(PK_DSA_E1_Byte_Length) +  /* hash */
          NEXT_WORD_SIZE(PK_DSA_N_Byte_Length));   /* random number */
      if (key_p->keyType != N8_PRIVATE_SKS)
      {

         /* for parameter block */
         nBytes += NEXT_WORD_SIZE(PK_DSA_Param_Byte_Length(modulusDigits));
      }

      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_DSA_SIGN_NUMCMDS(key_p), 
                                  resultHandlerDSASign, nBytes);
      CHECK_RETURN(ret);
      
      r_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      r_a =                 req_p->qr.physicalAddress + req_p->dataoffset;
      s_p =  r_p + NEXT_WORD_SIZE(PK_DSA_R_Byte_Length);
      s_a =  r_a + NEXT_WORD_SIZE(PK_DSA_R_Byte_Length);
      mh_p = s_p + NEXT_WORD_SIZE(PK_DSA_S_Byte_Length);
      mh_a = s_a + NEXT_WORD_SIZE(PK_DSA_S_Byte_Length);
      n_p = mh_p + NEXT_WORD_SIZE(PK_DSA_E1_Byte_Length);
      n_a = mh_a + NEXT_WORD_SIZE(PK_DSA_E1_Byte_Length);

      /* copy the user-space data into our kernel space buffers */
      memcpy(mh_p + (PK_DSA_E1_Byte_Length - DSA_SIGN_LENGTH),
             msgHash_p, 
             DSA_SIGN_LENGTH);

      /* get random number N.  Note that the chip will enforce the relationship
       * N < q, so we don't have to do anything here. */
      ret = N8_GetRandomBytes(DSA_SIGN_LENGTH, n_p, NULL);
      CHECK_RETURN(ret);

      if (key_p->keyType != N8_PRIVATE_SKS)
      {
         paramBlock_p = n_p
                        + NEXT_WORD_SIZE(PK_DSA_N_Byte_Length);
         paramBlock_a = n_a
                        + NEXT_WORD_SIZE(PK_DSA_N_Byte_Length);
         memcpy(paramBlock_p, 
                key_p->paramBlock, 
                PK_DSA_Param_Byte_Length(modulusDigits));
      }
      else
      {
         paramBlock_a = 0;
      }

      /* create a post-processing data object for returning the calculated R and
       * S results */
      postData_p = (dsaSignPostDataStruct_t *)&req_p->postProcessBuffer;
      postData_p->rTo_p = rValue_p;
      postData_p->sTo_p = sValue_p;
      postData_p->rFrom_p = r_p;
      postData_p->sFrom_p = s_p;

      req_p->postProcessingData_p = (void *) postData_p;
      ret = cb_dsaSign(req_p, key_p, n_a, paramBlock_a, mh_a,
                       r_a, s_a, req_p->PK_CommandBlock_ptr);

      CHECK_RETURN(ret);

      QUEUE_AND_CHECK(event_p, req_p, ret); 
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);
   DBG(("Signed\n"));

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }

   return ret;
} /* N8_DSASign */


/**********************************************************************
  * N8_DSAVerify
  ***********************************************************************/
 /**  @ingroup dsa
  * @brief Verifies the digital signature of the message whose SHA-1 hash
  * value is MessageHash using the Digital Signature Algorithm (DSA) as
  * defined by the Digital Signature Standard [DSS]
  *
  * Description:
  * MessageHash is the 20 byte SHA-1 hash value of the message being
  * signed (for example, as computed by N8_HashCompleteMessage).  The two
  * DSA defined signature values, r and s, which constitute the signature
  * to be verified, are specified in RValue and SValue respectively.  The
  * DSA key parameters to use in the verification are taken from DSAKeyObject.
  * DSAKeyObject must have been previously initialized via an appropriate
  * call to N8_DSAInitializeKey.
  * This call first checks that 0 < RValue < q and 0 < SValue < q where q
  * is as defined by DSAKeyObject. If either of these conditions is false,
  * verification fails and the Verify return flag is set to False. Otherwise,
  * the function computes the value v defined by DSA and compares v to RValue. 
  * If they are not equal, verification fails and the Verify return flag is
  * set to False. If they are equal, verification succeeds and the Verify
  * return flag is set to True.
  * The value v is computed as follows:
  *      w  = s-1  mod q
  *      u1 = h * w  mod q
  *      u2 = r * w  mod q
  *      v = (gu1  * yu2  mod p) mod q
  * where r = the provided signature value  RValue, s = the provided signature
  * value  SValue, h = the SHA-1 hash value MessageHash, and y, g, p, and q
  * are the DSA key parameters defined by DSAKeyObject
  *
  * 
  * Parameters:
  * @param key_p           RO:   The previously initialized DSAKeyObject containing the DSA key
  *                              materials to be used.
  * @param msgHash_p       RO:   The SHA-1 hash of the message being signed; always 20 bytes.    
  * @param rValue_p        RO:   The DSA r value.  This is always 20 bytes in length. 
  * @param sValue_p        RO:   The DSA s value.  This is always 20 bytes in length.
  * @param verify          WO:   The return boolean value;  True if the signature 
  *                              verifies correctly and False otherwise
  * @param event_p                 RW:   On input, if null the call is synchronous and no event
  *                              is returned. The operation is complete when the call 
  *                              returns. If non-null, then the call is asynchronous; 
  *                              an event is returned that can be used to determine when 
  *                              the operation completes.
  *
  * @return 
  *    ret - returns N8_STATUS_OK if successful or Error value.
  *
  * @par Errors:
  *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
  *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
  *          N8_INVALID_KEY      -       The DSAKeyObject is not a valid key object
  *                                  for this operation.
  *   
  *
  * @par Assumptions:
  **********************************************************************/
N8_Status_t N8_DSAVerify(const N8_DSAKeyObject_t *key_p,
                         const N8_Buffer_t       *msgHash_p,
                         const N8_Buffer_t       *rValue_p,
                         const N8_Buffer_t       *sValue_p,
                         N8_Boolean_t            *verify_p,
                         N8_Event_t              *event_p)
{
   N8_Status_t     ret = N8_STATUS_OK;
   int             modulusDigits;
   API_Request_t  *req_p = NULL;
   int             vq_cmp;
   int             nBytes;
   int             skipBytes;
   int             i;
   int             rValueNonZero = FALSE;
   int             sValueNonZero = FALSE;
   uint32_t        r_a;
   uint32_t        s_a;
   uint32_t        mh_a;
   uint32_t        result_a;
   uint32_t        q_a;
   uint32_t        cp_a; 
   uint32_t        gR_mod_p_a;
   uint32_t        p_a;
   uint32_t        publicKey_a; 
   N8_Buffer_t    *r_p = NULL;
   N8_Buffer_t    *s_p = NULL;
   N8_Buffer_t    *mh_p = NULL;
   N8_Buffer_t    *result_p = NULL;
   N8_Buffer_t    *q_p = NULL;
   N8_Buffer_t    *cp_p = NULL;
   N8_Buffer_t    *gR_mod_p_p = NULL;
   N8_Buffer_t    *p_p = NULL;
   N8_Buffer_t    *publicKey_p = NULL;
   dsaVerifyPostDataStruct_t *postData_p = NULL;
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(msgHash_p, ret);
      CHECK_OBJECT(rValue_p, ret);
      CHECK_OBJECT(sValue_p, ret);
      CHECK_OBJECT(verify_p, ret);
      CHECK_STRUCTURE(key_p->structureID, N8_DSA_STRUCT_ID, ret);
      if (key_p->keyType != N8_PUBLIC)
      {
         ret = N8_INVALID_KEY;
         break;
      }

      *verify_p = N8_FALSE;

      /* sizes of S, R, hash are the same */
      skipBytes = PK_DSA_S_Byte_Length - DSA_SIGN_LENGTH;

      /* check that r < q and s < q */
      vq_cmp = memcmp(rValue_p, key_p->q + skipBytes, DSA_SIGN_LENGTH);
      if (vq_cmp >= 0)
      {
         ret = N8_INVALID_VALUE;
         break;
      }
      vq_cmp = memcmp(sValue_p, key_p->q + skipBytes, DSA_SIGN_LENGTH);
      if (vq_cmp >= 0)
      {
         ret = N8_INVALID_VALUE;
         break;
      }

      /* check that 0 < r and 0 < s */

      /* for this test, we simply need to check that r and s are not zero
       * as they cannot be negative. */

      for ( i = 0; i < DSA_SIGN_LENGTH; i++ )
      {
         if (rValue_p[i] != 0x00)
         {
            rValueNonZero = TRUE;
         }
         if (sValue_p[i] != 0x00)
         {
            sValueNonZero = TRUE;
         }
         if ( (rValueNonZero) && (sValueNonZero) )
         {
            break;
         }
      }

      if ( (!rValueNonZero) || (!sValueNonZero) )
      {
         ret = N8_INVALID_VALUE;
         break;
      }

      modulusDigits = BYTES_TO_PKDIGITS(key_p->modulusLength);

      nBytes =
         (NEXT_WORD_SIZE(PK_DSA_R_Byte_Length) +  /* for R */
          NEXT_WORD_SIZE(PK_DSA_S_Byte_Length) +  /* for S */
          NEXT_WORD_SIZE(PK_DSA_E1_Byte_Length) + /* for hash */
          NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length) +  /* for recalculated value to verify */
          NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length) +  /* for q */
          NEXT_WORD_SIZE(PK_DSA_CP_Byte_Length) + /* for cp */
          NEXT_WORD_SIZE(PK_DSA_GR_MOD_P_Byte_Length(modulusDigits)) + /* for gR_mod_p */
          NEXT_WORD_SIZE(PK_DSA_Y_Byte_Length(modulusDigits)) +  /* for public key */
          NEXT_WORD_SIZE(PK_DSA_P_Byte_Length(modulusDigits))    /* for p */
                );

      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_DSA_VERIFY_NUMCMDS, 
                                  resultHandlerDSAVerify, nBytes);
      CHECK_RETURN(ret);

      r_a =                 req_p->qr.physicalAddress + req_p->dataoffset;
      r_p = (N8_Buffer_t *) ((int)req_p +  req_p->dataoffset);

      s_a = r_a + NEXT_WORD_SIZE(PK_DSA_R_Byte_Length);
      s_p = r_p + NEXT_WORD_SIZE(PK_DSA_R_Byte_Length);

      mh_a = s_a + NEXT_WORD_SIZE(PK_DSA_S_Byte_Length);
      mh_p = s_p + NEXT_WORD_SIZE(PK_DSA_S_Byte_Length);

      result_a = mh_a + NEXT_WORD_SIZE(PK_DSA_E1_Byte_Length);
      result_p = mh_p + NEXT_WORD_SIZE(PK_DSA_E1_Byte_Length);

      q_a = result_a + NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length);
      q_p = result_p + NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length);

      cp_a = q_a + NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length);
      cp_p = q_p + NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length);

      gR_mod_p_a = cp_a + NEXT_WORD_SIZE(PK_DSA_CP_Byte_Length);
      gR_mod_p_p = cp_p + NEXT_WORD_SIZE(PK_DSA_CP_Byte_Length);

      publicKey_p = gR_mod_p_p +
         NEXT_WORD_SIZE(PK_DSA_GR_MOD_P_Byte_Length(modulusDigits));
      publicKey_a = gR_mod_p_a +
         NEXT_WORD_SIZE(PK_DSA_GR_MOD_P_Byte_Length(modulusDigits));

      p_p = publicKey_p + 
         NEXT_WORD_SIZE(PK_DSA_Y_Byte_Length(modulusDigits));
      p_a = publicKey_a +
         NEXT_WORD_SIZE(PK_DSA_Y_Byte_Length(modulusDigits));

      /* Only the last DSA_SIGN_LENGTH are useful for the hash, R and S
       * values.
       */
      memcpy(r_p + skipBytes, rValue_p, DSA_SIGN_LENGTH);
      memcpy(s_p + skipBytes, sValue_p, DSA_SIGN_LENGTH);
      memcpy(mh_p + skipBytes, msgHash_p, DSA_SIGN_LENGTH);

      memcpy(q_p, key_p->q, PK_DSA_Q_Byte_Length);
      memcpy(cp_p, key_p->cp, PK_DSA_CP_Byte_Length);
      memcpy(gR_mod_p_p, key_p->gR_mod_p, PK_DSA_GR_MOD_P_Byte_Length(modulusDigits));
      memcpy(publicKey_p, key_p->publicKey, PK_DSA_Y_Byte_Length(modulusDigits));
      memcpy(p_p, key_p->p, PK_DSA_P_Byte_Length(modulusDigits));

      /* create a post-processing data object for comparing the computed results
       * with the expected results and set the verify flag */
      postData_p = (dsaVerifyPostDataStruct_t *)&req_p->postProcessBuffer;
      postData_p->computed_p = result_p;
      postData_p->expected_p = r_p;
      postData_p->size = PK_DSA_R_Byte_Length;
      postData_p->verify_p = verify_p;
      req_p->postProcessingData_p = (void *) postData_p;

      ret = cb_dsaVerify(req_p, 
                         key_p, 
                         q_a, 
                         cp_a, 
                         gR_mod_p_a,
                         p_a,
                         publicKey_a, 
                         mh_a, 
                         r_a, 
                         s_a, 
                         result_a,
                         req_p->PK_CommandBlock_ptr);
      CHECK_RETURN(ret);

      QUEUE_AND_CHECK(event_p, req_p, ret); 
      HANDLE_EVENT(event_p, req_p, ret);
      DBG(("Verified\n"));

   } while (FALSE);

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* N8_DSAVerify */

/**********************************************************************
  * N8_DSAFreeKey
  ***********************************************************************/
 /**  @ingroup dsa
  * @brief Frees the specified DSAKeyObject and all associated resources
  * so that they can be reused.
  *
  * Description:
  * When an application is finished using a previously initialized
  * DSAKeyObject, it should be freed so that any API or system resources
  * (including memory) can be freed.  After this call returns, DSAKeyObject
  * may no longer be used in N8_DSAEncrypt or N8_DSADecrypt calls.
  *
  * 
  * Parameters:
  * @param key_p               RO:   The previously initialized DSAKeyObject containing the DSA key
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
N8_Status_t  N8_DSAFreeKey (N8_DSAKeyObject_t *key_p)
{

   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      
      CHECK_OBJECT(key_p, ret);
      CHECK_STRUCTURE(key_p->structureID, N8_DSA_STRUCT_ID, ret);

      if (key_p->kmem_p != NULL)
      {
         N8_KFREE(key_p->kmem_p);
      }
      key_p->structureID = 0;

   }while(FALSE);
   return  ret;
} /* N8_DSAFreeKey */
/*
  * Local functions
  */
 /**********************************************************************
  * initDSAPublicKey
  ***********************************************************************/
 /** @ingroup dsa
  * @brief Initializes public key.
  *
  * 
  * @param key_p          RW: The caller allocated DSAKeyObject, initialized
  *                           by this call with the appropriate DSA key 
  *                           material and pre-computed DSA constants 
  *                           depending on the value of the KeyType parameter
  * @param material_p       RO: The key material to use in initializing DSAKeyObject.
  *                           The valid information that must be supplied
  *                           depends on the value of of KeyType
  *
  *
  * @return 
  *    ret - returns N8_STATUS_OK if successful or Error value.
  *
  * @par Errors:
  *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
  *
  * @par Assumptions:
  *      None.
  **********************************************************************/
static N8_Status_t initDSAPublicKey(N8_DSAKeyObject_t   *key_p,
                                    N8_DSAKeyMaterial_t *material_p,
                                    N8_Event_t          *event_p)
{
   unsigned int              modulusDigits;
   API_Request_t            *req_p = NULL;
   N8_Status_t               ret = N8_STATUS_OK;  
   N8_DSAKeyObjectPhysical_t pKey;
   unsigned int              nBytes;
   unsigned int              padding;

   PK_CMD_BLOCK_t    *nextCommandBlock = NULL;

   do
   {
      /* first, test the length of the public key/modulus */
      ret = checkModulusLength(material_p->p.lengthBytes);
      CHECK_RETURN(ret);
      
      /* create the request buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_COMPUTE_GRMODX_NUMCMDS  /* size for GRmodX command block */
                                + N8_CB_COMPUTE_CX_NUMCMDS,     /* size for CX command block */
                                  resultHandlerGeneric, 0);
      CHECK_RETURN(ret);

      modulusDigits = BYTES_TO_PKDIGITS(material_p->p.lengthBytes);
      key_p->modulusLength = material_p->p.lengthBytes;
      DBG(("initDSAPublicKey\n"));

      /* Allocate parameter block.  This block will contain all
       * of the parameters for an DSA operation in the correct form for
       * direct load into the Big Num Cache.  
       */
      nBytes =
         (NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length) +
          NEXT_WORD_SIZE(PK_DSA_CP_Byte_Length) +
          NEXT_WORD_SIZE(PK_DSA_GR_MOD_P_Byte_Length(modulusDigits)) +
          NEXT_WORD_SIZE(PK_DSA_P_Byte_Length(modulusDigits)) + 
          NEXT_WORD_SIZE(PK_DSA_Y_Byte_Length(modulusDigits)) + 
          NEXT_WORD_SIZE(PK_DSA_G_Byte_Length(modulusDigits))   
          );         

      key_p->kmem_p = N8_KMALLOC_PK(nBytes);
      CHECK_OBJECT(key_p->kmem_p, ret);
      memset(key_p->kmem_p->VirtualAddress, 0, nBytes);

      key_p->q = (N8_Buffer_t *) key_p->kmem_p->VirtualAddress;
      pKey.q = key_p->kmem_p->PhysicalAddress;

      key_p->cp         = key_p->q +
         NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length);
      pKey.cp           = pKey.q   +
         NEXT_WORD_SIZE(PK_DSA_Q_Byte_Length);

      key_p->gR_mod_p   = key_p->cp +
         NEXT_WORD_SIZE(PK_DSA_CP_Byte_Length);
      pKey.gR_mod_p     = pKey.cp   + 
         NEXT_WORD_SIZE(PK_DSA_CP_Byte_Length);

      key_p->p          = key_p->gR_mod_p + 
         NEXT_WORD_SIZE(PK_DSA_GR_MOD_P_Byte_Length(modulusDigits));
      pKey.p            = pKey.gR_mod_p   + 
         NEXT_WORD_SIZE(PK_DSA_GR_MOD_P_Byte_Length(modulusDigits));

      key_p->publicKey  = key_p->p + 
         NEXT_WORD_SIZE(PK_DSA_P_Byte_Length(modulusDigits));
      pKey.publicKey    = pKey.p   + 
         NEXT_WORD_SIZE(PK_DSA_P_Byte_Length(modulusDigits));

      key_p->g          = key_p->publicKey + 
         NEXT_WORD_SIZE(PK_DSA_Y_Byte_Length(modulusDigits));
      pKey.g            = pKey.publicKey   + 
         NEXT_WORD_SIZE(PK_DSA_Y_Byte_Length(modulusDigits));

      /*
       * convert the material_p from BIGNUM to binary byte buffer and copy
       * to the key's parameter block
       */

      /* q */
      padding = PK_DSA_Q_Byte_Length - material_p->q.lengthBytes;
      memcpy(&key_p->q[padding],
             material_p->q.value_p,
             material_p->q.lengthBytes);
      /* p */
      padding = PK_DSA_P_Byte_Length(modulusDigits) -
                   material_p->p.lengthBytes;
      memcpy(&key_p->p[padding],
             material_p->p.value_p,
             material_p->p.lengthBytes);
      /* publicKey */
      padding = (PK_DSA_Y_Byte_Length(modulusDigits) -
                 material_p->publicKey.lengthBytes);
      memcpy(&key_p->publicKey[padding],
             material_p->publicKey.value_p,
             material_p->publicKey.lengthBytes);
      /* g */
      padding = (PK_DSA_G_Byte_Length(modulusDigits) -
                 material_p->g.lengthBytes);
      memcpy(&key_p->g[padding],
             material_p->g.value_p,
             material_p->g.lengthBytes);
      /*  Compute gR mod p and put it into the key object. */
      ret = cb_computeGRmodX(req_p, modulusDigits, pKey.g, pKey.p,
                             pKey.gR_mod_p, 
                             req_p->PK_CommandBlock_ptr, 
                             &nextCommandBlock);
      CHECK_RETURN(ret);

      ret = cb_computeCX(req_p, pKey.p, pKey.cp, 
                         key_p->modulusLength,
                         nextCommandBlock,
                         &nextCommandBlock,
                         key_p->unitID,
			 N8_TRUE);
      CHECK_RETURN(ret);

      QUEUE_AND_CHECK(event_p, req_p, ret); 
      HANDLE_EVENT(event_p, req_p, ret);

   } while(FALSE);

   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
      if (key_p->kmem_p != NULL)
      {
         N8_KFREE(key_p->kmem_p);
         key_p->kmem_p = NULL;
      }
   }
   return ret;
} /* initDSAPublicKey */

/**********************************************************************
  * initDSAPrivateKey
  ***********************************************************************/
 /** @ingroup dsa
  * @brief Initializes private key.
  *
  * 
  * @param key_p          RW: The caller allocated DSAKeyObject, initialized
  *                           by this call with the appropriate DSA key 
  *                           material and pre-computed DSA constants 
  *                           depending on the value of the KeyType parameter
  * @param material_p       RO: The key material to use in initializing DSAKeyObject.
  *                           The valid information that must be supplied
  *                           depends on the value of of KeyType
  *
  *
  * @return 
  *    ret - returns N8_STATUS_OK if successful or Error value.
  *
  * @par Errors:
  *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
  *
  * @par Assumptions:
  *      None.
  **********************************************************************/
static N8_Status_t initDSAPrivateKey(N8_DSAKeyObject_t   *key_p,
                                     N8_DSAKeyMaterial_t *material_p,
                                     N8_Event_t          *event_p)
{
   API_Request_t     *req_p = NULL;
   int                modulusDigits;    
   N8_Status_t        ret = N8_STATUS_OK;     
   N8_DSAKeyObjectPhysical_t pKey;
   unsigned int       nBytes;
   PK_CMD_BLOCK_t    *nextCommandBlock = NULL;
   unsigned int       padding;
   int pq_cmp;                   /* result of cmp(x,q) */
   /*
    * key material must contain public key, e, modulus n, and their
    * sizes
    *
    * Tasks to be performed:
    *                   1) Put p in the parameter block.
    *                   2) Put q in the Parameter block.
    *                   3) Put privateKey in the parameter block.
    *                   4) Compute gR mod p and put it in the parameter block.
    *                   5) Compute cp = -(p[0]^-1  mod 2^128 and 
    *                      put it in the parameter block.
    */
   do
   {
      /* create the request buffer */ 
      ret = createPKRequestBuffer(&req_p, 
                                  key_p->unitID, 
                                  N8_CB_COMPUTE_GRMODX_NUMCMDS
                                + N8_CB_COMPUTE_CX_NUMCMDS, 
                                  resultHandlerGeneric, 0);
      CHECK_RETURN(ret);


      pq_cmp = n8_sizedBufferCmp(&material_p->privateKey, &material_p->q);

      if (pq_cmp >= 0)
      {
          ret = N8_INVALID_KEY;
          break;
      }
      modulusDigits = BYTES_TO_PKDIGITS(material_p->p.lengthBytes);

      key_p->modulusLength = material_p->p.lengthBytes;
      DBG(("initDSAPrivateKey\n"));
      DBG(("modulus length: %d\n", key_p->modulusLength));

      /* Allocate parameter block.  This block will contain all
       * of the parameters for an DSA operation in the correct form for
       * direct load into the Big Num Cache.  
       */
      nBytes =
         (NEXT_WORD_SIZE(PK_DSA_Param_Byte_Length(modulusDigits)) + /* paramBlock */ 
          NEXT_WORD_SIZE(PK_DSA_G_Byte_Length(modulusDigits))       /* generator g */
          );         

      key_p->kmem_p = N8_KMALLOC_PK(nBytes);
      CHECK_OBJECT(key_p->kmem_p, ret);
      memset(key_p->kmem_p->VirtualAddress, 0, nBytes);

      key_p->paramBlock = (N8_Buffer_t *) key_p->kmem_p->VirtualAddress;
      pKey.paramBlock = key_p->kmem_p->PhysicalAddress;


      /* Tasks 1-3 */

      /*
       * set the pointers in the key object to the correct address in the
       * parameter block
       */
      key_p->p          = key_p->paramBlock + PK_DSA_P_Param_Offset;
      key_p->gR_mod_p   = key_p->paramBlock + PK_DSA_GR_MOD_P_Param_Offset(modulusDigits);
      key_p->q          = key_p->paramBlock + PK_DSA_Q_Param_Offset(modulusDigits);
      key_p->privateKey = key_p->paramBlock + PK_DSA_X_Param_Offset(modulusDigits);
      key_p->cp         = key_p->paramBlock + PK_DSA_CP_Param_Offset(modulusDigits);

      pKey.p            = pKey.paramBlock   + PK_DSA_P_Param_Offset;
      pKey.gR_mod_p     = pKey.paramBlock   + PK_DSA_GR_MOD_P_Param_Offset(modulusDigits);
      pKey.q            = pKey.paramBlock   + PK_DSA_Q_Param_Offset(modulusDigits);
      pKey.privateKey   = pKey.paramBlock   + PK_DSA_X_Param_Offset(modulusDigits);
      pKey.cp           = pKey.paramBlock   + PK_DSA_CP_Param_Offset(modulusDigits);

      /*
       * convert the material from BIGNUM to binary byte buffer and copy
       * to the key's parameter block
       */

      /* q */
      padding = PK_DSA_Q_Byte_Length - material_p->q.lengthBytes;
      memcpy(&key_p->q[padding],
             material_p->q.value_p,
             material_p->q.lengthBytes);
      /* p */
      padding = PK_DSA_P_Byte_Length(modulusDigits) - material_p->p.lengthBytes;
      memcpy(&key_p->p[padding],
             material_p->p.value_p,
             material_p->p.lengthBytes);
      /* publicKey */
      padding = PK_DSA_X_Byte_Length - material_p->privateKey.lengthBytes;
      memcpy(&key_p->privateKey[padding],
             material_p->privateKey.value_p,
             material_p->privateKey.lengthBytes);
      /*
       * allocate space for the generator.  it is not a part of the parameter
       * block
       */

      key_p->g = key_p->paramBlock + NEXT_WORD_SIZE(PK_DSA_Param_Byte_Length(modulusDigits));
      pKey.g = pKey.paramBlock + NEXT_WORD_SIZE(PK_DSA_Param_Byte_Length(modulusDigits));

      /* g */
      memcpy(key_p->g, material_p->g.value_p, material_p->g.lengthBytes);

      /* Compute gR mod p and put it into the key object. */
      /* note that we already have a req_p from the initial create */
      ret = cb_computeGRmodX(req_p, modulusDigits, pKey.g, pKey.p,
                             pKey.gR_mod_p, req_p->PK_CommandBlock_ptr, &nextCommandBlock);
      CHECK_RETURN(ret);

      ret = cb_computeCX(req_p,
                         pKey.p,
                         pKey.cp, 
                         key_p->modulusLength,
                         nextCommandBlock,
                         &nextCommandBlock,
                         key_p->unitID,
			 N8_TRUE);
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
      if (key_p->kmem_p != NULL)
      {
         N8_KFREE(key_p->kmem_p);
         key_p->kmem_p = NULL;
      }

      /* The hardware detected a problem with the key, overwrite */
      /* the generic hardware error with INVALID_KEY.            */
      ret = N8_INVALID_KEY;
   }

   return ret;
} /* initDSAPrivateKey */
/**********************************************************************
  * initDSAPrivateSKS
  ***********************************************************************/
 /** @ingroup dsa
  * @brief Initializes private key.
  *
  * 
  * @param key_p          RW: The caller allocated DSAKeyObject, initialized
  *                           by this call with the appropriate DSA key 
  *                           material and pre-computed DSA constants 
  *                           depending on the value of the KeyType parameter
  * @param material_p     RO: The key material to use in initializing DSAKeyObject.
  *                           The valid information that must be supplied
  *                           depends on the value of of KeyType
  *
  *
  * @return 
  *    ret - returns N8_STATUS_OK if successful or Error value.
  *
  * @par Errors:
  *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
  *
  * @par Assumptions:
  *      None.
  **********************************************************************/
static N8_Status_t initDSAPrivateSKSKey(N8_DSAKeyObject_t   *key_p,
                                        N8_DSAKeyMaterial_t *material_p,
                                        N8_Event_t          *event_p)
{
   N8_Status_t        ret = N8_STATUS_OK;     
   unsigned int       validateSKSkeyLength;

   /*
    * From the key material we only need the SKSKeyHandle.  All information is
    * precomputed and placed in the SKS for us.
    *
    */
   do
   {
      /* verify the required parameters are not null */
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(material_p, ret);
      /* verify the SKS type is correct */
      if (material_p->SKSKeyHandle.key_type != N8_DSA_VERSION_1_KEY)
      {
         ret = N8_INCONSISTENT;
         break;
      }

      validateSKSkeyLength = PKDIGITS_TO_BYTES(material_p->SKSKeyHandle.key_length) % 16;
      if (validateSKSkeyLength != 0)
      {
          if ((validateSKSkeyLength - 8) !=0)
          {
              ret = N8_INVALID_KEY_SIZE;
              break;
          }
      }
      /* set the modulus length */
/*      key_p->modulusLength = material_p->p.lengthBytes; */
      key_p->modulusLength = PKDIGITS_TO_BYTES(material_p->SKSKeyHandle.key_length);

      DBG(("initDSAPrivateSKS\n"));
      DBG(("modulus length: %d\n", key_p->modulusLength));

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
} /* initDSAPrivateSKS */

/*****************************************************************************
 * checkModulusLength
 *****************************************************************************/
/** @ingroup dsa
 * @brief Check the modulus length to ensure it is proper.
 *
 * The modulus length must be between 64 bytes and 512 bytes and must be a
 * multiple of 8 bytes.
 *
 *  @param length              RO:  The length to be verified
 *
 * @par Externals
 *    None
 *
 * @return
 *    N8_STATUS_OK if the length is valid<br>
 *    N8_INVALID_KEY_SIZE if the length is invalid<br>
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t checkModulusLength(unsigned int length)
{
   N8_Status_t ret = N8_STATUS_OK;
   
   /* The length must be a multiple of 8 bytes between 64 and 512 */
   if (length < N8_DSA_PUBLIC_KEY_LENGTH_MIN ||
       length > N8_DSA_PUBLIC_KEY_LENGTH_MAX ||
       (length % 8 != 0))
   {
      ret = N8_INVALID_KEY_SIZE;
   }
   return ret;
} /* checkModulusLength */
