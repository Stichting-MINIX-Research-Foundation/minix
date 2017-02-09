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

static char const n8_id[] = "$Id: n8_cryptographic.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_cryptographic.c
 *  @brief Contains packet functions.
 *
 * Functions:
 *   N8_EncryptInitialize  -   Initializes the encrypt object object.
 *   N8_EncryptPartial     -   Perform a partial encryption.
 *   N8_EncryptEnd         -   Complete an in-progress partial encryption.
 *   N8_Encrypt            -   Perform a complete encryption.
 *   N8_DecryptPartial     -   Perform a partial decryption.
 *   N8_DecryptEnd         -   Complete an in-progress partial decryption.
 *   N8_Decrypt            -   Perform a complete decryption.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 06/06/03 brr   Move n8_enums to public include as  n8_pub_enums.
 * 05/27/03 brr   Removed N8_preamble call since N8_PacketInitialize must be
 *                called prior to Encrypt/Decrypt operations.
 * 05/20/03 brr   Remove include of obsolete n8_contextM.h & n8_cryptographic.h.
 * 09/23/02 bac   Fixed bug #870 by initializing residualLength to zero.
 * 06/18/02 bac   When Encrypt and EncryptEnd were adding padding, the portion
 *                added was not being set to 0x0.  (Bug #808)
 * 06/15/02 brr   Check message length ptr in N8_EncryptPartial & N8_EncryptEnd.
 * 06/11/02 bac   Check for null context in N8_EncryptInitialize when using
 *                ARC4.   Return N8_UNALLOCATED_CONTEXT when the magic number is
 *                not correct.  (Bug #776)
 * 06/11/02 bac   Check for invalid unit in N8_EncryptInitialize. (Bug #777)
 * 06/10/02 bac   Fixed N8_Decrypt and N8_DecryptEnd to return
 *                N8_INVALID_INPUT_SIZE instead of padding. (Bug #775)
 * 05/22/02 brr   Check message length ptr in N8_DecryptPartial & N8_DecryptEnd.
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/25/02 brr   Removed last references to the QMgr.
 * 01/31/02 brr   Eliminated the memory allocation for postProcessingData.
 * 12/18/01 mel   Deleted illegal operations with contextHandle
 * 11/28/01 mel   Fixed bug #381: EncryptInitialize needs to force parity before
 *                DES weakkeys check.
 * 11/27/01 bac   Removed unnecessary CHECK_RETURNs after QUEUE_AND_CHECKs.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/09/01 hml   Added structure verification code.
 * 10/02/01 bac   Added use of RESULT_HANDLER_WARNING in all result handlers.
 * 09/28/01 bac   Clean up regarding use of const in parameter lists.
 * 09/28/01 bac   Further development changes.  Fixes to work for context use.
 * 09/24/01 hml   Added unit specifer for multi-chip.
 * 09/21/01 bac   Fixed bug to prevent request from being freed twice.
 * 09/20/01 bac   Continued development.  Massive changes.
 * 09/18/01 bac   Substantial re-write to properly use kernel memory, event
 *                processing, and async support.
 * 09/14/01 mel   Added event parameter to N8_FreeContext.
 * 09/06/01 bac   Fixed doxygen groupings.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 06/07/01 mel   Original version.
 ****************************************************************************/
/** @defgroup crypto Cryptographic API Packet-Level functions
 */

#include "n8_common.h"          /* common definitions */
#include "n8_pub_errors.h"      /* Errors definition */
#include "n8_enqueue_common.h"  /* common definitions for enqueue */
#include "n8_cb_ea.h"
#include "n8_util.h"
#include "n8_key_works.h"
#include "n8_packet.h"
#include "n8_API_Initialize.h"
#include "n8_pub_enums.h"
#include <opencrypto/cryptodev.h>

#define NO_CALLBACK_FCN NULL

/*
 * Local structures used only for transfering data to the call back functions.
 */
typedef struct
{
   unsigned int       *encryptedMsgLen_p;
   N8_EncryptObject_t *encryptObject_p;
   uint32_t           *nextIV_p;
} encryptData_t;

/*
 * Local functions
 */
static N8_Status_t verifyInputLength(const unsigned int len)
{
   N8_Status_t ret = N8_STATUS_OK;
   if (len > N8_MAX_MESSAGE_LENGTH)
   {
      DBG(("Message length is out of range\n"));
      ret = N8_INVALID_INPUT_SIZE;
   }
   return ret;
}
static N8_Status_t checkCipher(N8_Cipher_t cipher)
{
   N8_Status_t ret = N8_STATUS_OK;
   switch (cipher)
   {
      case N8_CIPHER_ARC4:
      case N8_CIPHER_DES:
         break;
      default:
         ret = N8_INVALID_CIPHER;
         break;
   }
   return ret;
}

static void performCopyBack(API_Request_t* req_p)
{
   if (req_p->copyBackSize > 0 &&
       req_p->copyBackFrom_p != NULL &&
       (req_p->copyBackTo_p != NULL || req_p->copyBackTo_uio != NULL))
   {
#if N8DEBUG
      DBG(("copyBackSize = %d\n", req_p->copyBackSize));
      DBG(("results: "));
      printN8Buffer((N8_Buffer_t *) req_p->copyBackFrom_p, req_p->copyBackSize);
#endif      
      if (req_p->copyBackTo_p != NULL) {
#if N8DEBUG
	  DBG(("memcpy\n"));
#endif
	  memcpy(req_p->copyBackTo_p,
		 req_p->copyBackFrom_p,
		 req_p->copyBackSize);
      } else {
#if N8DEBUG
	  DBG(("cuio_copyback\n"));
#endif
	  cuio_copyback(req_p->copyBackTo_uio, 0, 
		  req_p->copyBackSize, 
		  req_p->copyBackFrom_p);
      }
   }
   else
   {
      DBG(("no copy back given\n"));
   }
}

static void resultHandler(API_Request_t* req_p)
{
   char title[] = "N8_cryptographic result handler";
   encryptData_t *callBackData_p = (encryptData_t *) req_p->postProcessingData_p;
   
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s called with success\n", title));
      performCopyBack(req_p);
      if (callBackData_p != NULL)
      {
         N8_EncryptObject_t *encryptObject_p = callBackData_p->encryptObject_p;
         /* update IVs for DES*/
         if (encryptObject_p->cipher == N8_CIPHER_DES &&
             callBackData_p->nextIV_p != NULL)
         {
            memcpy(encryptObject_p->cipherInfo.key.keyDES.IV,
                   callBackData_p->nextIV_p, N8_DES_BLOCK_LENGTH);
#if N8DEBUG
            DBG(("IV[ms]: "));
            printN8Buffer((N8_Buffer_t *) encryptObject_p->cipherInfo.key.keyDES.IV,
                          N8_DES_BLOCK_LENGTH);
#endif            
         }
         /* set the returned value for the encrypted message length */
         *(callBackData_p->encryptedMsgLen_p) = req_p->copyBackSize;
      }
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }
} /* resultHandler */


/*
 * External functions
 */

/*****************************************************************************
 * N8_EncryptInitialize
 *****************************************************************************/
/** @ingroup crypto
 * @brief Initializes (makes ready for use in encrypt and decrypt operations)
 * the encrypt object specified by EncryptObject. 
 *
 * An encryption object can be initialized for use with ARC4 or DES encryption /
 * decryption. The encryption algorithm is specified by the enumerated value
 * Cipher. The two permissible values for Cipher are ARC4 and DES.  If Cipher is
 * ARC4, then EncryptObject is initialized for use with ARC4 encryption /
 * decryption. ARC4 operations require context memory, so ContextHandle must
 * specify a valid context entry allocated by N8_AllocateContext that will be
 * made part of EncryptObject.  CipherInfo specifies the ARC4 key to use.  The
 * context entry denoted by ContextHandle is loaded with this key and will be
 * used in subsequent N8_Encrypt or N8_Decrypt calls made with EncryptObject.
 * If Cipher is DES, then EncryptObject is initialized for use with DES
 * encryption / decryption. DES operations do not require context memory.
 * ContextHandle may be null or may specify a valid context entry allocated by
 * N8_AllocateContext that will be made part of EncryptObject.  CipherInfo
 * specifies the DES key material to use (including the initialization
 * vector). If ContextHandle is null, then EncryptObject is initialized with the
 * key material and can be used in subsequent N8_Encrypt or N8_Decrypt calls. If
 * ContextHandle is non-null and valid, the context entry denoted by
 * ContextHandle is loaded with this key material and will be used in subsequent
 * N8_Encrypt or N8_Decrypt calls made with EncryptObject. DES keys will be
 * checked for weak and semi-weak keys and such keys will be rejected. DES keys
 * will not be checked for correct parity; the parity bits are ignored.  (Before
 * use in the hardware the parity will be set correctly so that hardware
 * generated parity errors do not occur.)
 *
 * @param encryptObject_p   WO: The encrypt object to be initialized with the
 *                              specified information. The returned object can
 *                              be used in subsequent N8_Encrypt or N8_Decrypt *
 *                              calls
 *
 * @param contextHandle_p   RO: A valid context handle as returned by
 *                              N8_AllocateContext, if Cipher = ARC4. Optional
 *                              (may be null), if Cipher = DES. The
 *                              corresponding context memory entry is loaded for
 *                              use in subsequent N8_Encrypt of Decrypt calls.
 *
 * @param cipher            RO: One of the values ARC4 or DES.
 *
 * @param cipherInfo_p      RO: The specific information to be used in the
 *                              initialization.  Its type and contents depend on
 *                              the value of ContextType, as specified above.
 *
 *
 * @return 
 *    encryptObject_p - initialized encrypt object.
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - packet object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_INVALID_CIPHER   - The value of Cipher is not one of the legal values
 *                          ARC4 or DES.<BR>
 *    N8_INVALID_VALUE    - The value of ContextIndex is not a valid context 
 *                          index, or ContextIndex is null but a context index 
 *                          is required because ARC4 is specified or the
 *                          unit value specified is invalid.<BR>
 *    N8_UNALLOCATED_CONTEXT - ContextIndex denotes a context memory index that
 *                          has not been allocated by N8_AllocateContext.<BR>
 *    N8_INVALID_KEY_SIZE - The size of the key specified in CipherInfo_p is 
 *                          outside the valid range for the encryption algorithm 
 *                          specified in Cipher, or the size of an HMAC key 
 *                          specified in CipherInfo_p is outside the valid range 
 *                          for the hash algorithm specified in HashAlgorithm.<BR> 
 *    N8_INCONSISTENT  -    The information in  CipherInfo_p and/or its type is 
 *                          different than or inconsistent with the type 
 *                          specified by Cipher or HashAlgorithm, or the 
 *                          combination of values specified by Protocol, Cipher, 
 *                          and HashAlgorithm is invalid (for example, SSL is 
 *                          specified with HMAC-MD5, or IPSec is specified with ARC4).<BR>
 *    N8_MALLOC_FAILED    - memory allocation failed
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 * Context entries (and hence ContextHandles) can only be used to hold one
 * context at a time. The context entry specified in this call should not
 * be in use with any other current  encrypt object or packet object. This
 * condition is not checked for;  incorrect / indeterminate and / or errors
 * are likely in this situation.
 *****************************************************************************/
N8_Status_t N8_EncryptInitialize(N8_EncryptObject_t *encryptObject_p,
                                 const N8_ContextHandle_t *contextHandle_p,
                                 const N8_Cipher_t cipher,
                                 N8_EncryptCipher_t *cipherInfo_p,
                                 N8_Event_t *event_p)

{
   N8_Status_t ret = N8_STATUS_OK;               /* the return status: OK or
                                                  * ERROR */ 
   int i;

   API_Request_t  *req_p = NULL;                 /* request buffer */
   key_cblock_t  key1, key2, key3;    /* keys to be checked */

   DBG(("N8_EncryptInitialize\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      
      /* verify packet object */
      CHECK_OBJECT(encryptObject_p, ret);

      /* verify cipher object */
      CHECK_OBJECT(cipherInfo_p, ret);

      /* since we are initializing the object, set the residual length to
       * zero. */
      encryptObject_p->residualLength = 0;
      
      /* if we are given a context handle */
      if (contextHandle_p != NULL)
      {
         /* Make sure the context struct is valid */
         if (contextHandle_p->structureID != N8_CONTEXT_STRUCT_ID)
         {
            ret = N8_UNALLOCATED_CONTEXT;
            break;
         }
         
         /* copy the contents into our encryption object and set the context in
          * use flag to true */
         memcpy(&encryptObject_p->contextHandle,
                contextHandle_p,
                sizeof(N8_ContextHandle_t)); 
         encryptObject_p->unitID = contextHandle_p->unitID;
         encryptObject_p->contextHandle.inUse = N8_TRUE;
      }
      else
      {
         encryptObject_p->contextHandle.inUse = N8_FALSE;
         encryptObject_p->contextHandle.index = 0xFFFF;
         encryptObject_p->unitID = cipherInfo_p->unitID;
      }

      /* validate the unit */
      if (n8_validateUnit(encryptObject_p->unitID) == N8_FALSE)
      {
         return N8_INVALID_UNIT;
         break;
      }
      
      /* verify cipher */
      switch (cipher)
      {
         case   N8_CIPHER_ARC4:

            /* verify that a valid context has been passed */
            if (contextHandle_p == NULL)
            {
               ret = N8_INVALID_OBJECT;
               break;
            }
            
            /* verify key size */
            if((cipherInfo_p->keySize < 1) ||
               (cipherInfo_p->keySize > ARC4_KEY_SIZE_BYTES_MAX))
            {
               DBG(("Key size specified for ARC4 "
                    "is outside the valid range: %d\n",
                    cipherInfo_p->keySize));
               DBG(("N8_EncryptInitialize - return Error\n"));
               ret = N8_INVALID_KEY_SIZE;
               break;
            }
             
            /* create request buffer */
            ret = createEARequestBuffer(&req_p,
                                        encryptObject_p->unitID,
                                        N8_CB_EA_LOADARC4KEYONLY_NUMCMDS,
                                        NO_CALLBACK_FCN, 
                                        sizeof(EA_SSL30_CTX));
            CHECK_RETURN(ret);

            /* Initialize for use with ARC4 encryption/decryption */
            ret = cb_ea_loadARC4keyOnly(req_p, 
                                        req_p->EA_CommandBlock_ptr,
                                        &(encryptObject_p->contextHandle),
                                        cipherInfo_p);

            break;
    
         case   N8_CIPHER_DES:
            /* verify key size */
    
            if(cipherInfo_p->keySize != DES_KEY_SIZE_BYTES)
            {
               DBG(("Key size specified for DES "
                    "is outside the valid range: %d\n",
                    cipherInfo_p->keySize)); 
               DBG(("N8_EncryptInitialize - return Error\n"));
               ret = N8_INVALID_KEY_SIZE;
               break;
            }

            /* build keys for parity verification */
            /* force key parity */
            for (i=0 ; i < sizeof(key_cblock_t); i++)
            {
               key1[i] = cipherInfo_p->key.keyDES.key1[i];
               key2[i] = cipherInfo_p->key.keyDES.key2[i];
               key3[i] = cipherInfo_p->key.keyDES.key3[i];
            }

            if (checkKeyParity(&key1) == N8_FALSE)
            {
               forceParity(&key1);
               for (i=0 ; i < sizeof(key_cblock_t); i++)
               {
                  cipherInfo_p->key.keyDES.key1[i] = key1[i];
               }
            }
            if (checkKeyParity(&key2) == N8_FALSE)
            {
               forceParity(&key2);
               for (i=0 ; i < sizeof(key_cblock_t); i++)
               {
                  cipherInfo_p->key.keyDES.key2[i] = key2[i];
               }
            }
            if (checkKeyParity(&key3) == N8_FALSE)
            {
               forceParity(&key3);
               for (i=0 ; i < sizeof(key_cblock_t); i++)
               {
                  cipherInfo_p->key.keyDES.key3[i] = key3[i];
               }
            }

            if (checkKeyForWeakness(&key1) ||
                checkKeyForWeakness(&key2) ||
                checkKeyForWeakness(&key3))
            {
               ret = N8_WEAK_KEY;
               break;
            }
            if (contextHandle_p != NULL)
            {
               /* create request buffer */
               ret = createEARequestBuffer(&req_p,
                                           encryptObject_p->unitID,
                                           N8_CB_EA_LOADDESKEYONLY_NUMCMDS,
                                           NO_CALLBACK_FCN,
                                           sizeof(EA_SSL30_CTX));
               CHECK_RETURN(ret);

               /* DES context memory */
               ret = cb_ea_loadDESkeyOnly(req_p, 
                                          req_p->EA_CommandBlock_ptr,
                                          &(encryptObject_p->contextHandle),
                                          cipherInfo_p);
            }
            break;

         default:
            /* invalid cipher */
            DBG(("Invalid cipher\n"));
            DBG(("N8_EncryptInitialize - return Error\n"));
            ret = N8_INVALID_CIPHER;
            break;
      }
      CHECK_RETURN(ret);
      /* finish initialize packet object with protocol related values */
      /* cipher info */
      memcpy(&encryptObject_p->cipherInfo, cipherInfo_p, sizeof(N8_EncryptCipher_t));
      /* encryption algorithm */
      encryptObject_p->cipher = cipher;

      if (req_p != NULL)
      {
         QUEUE_AND_CHECK(event_p, req_p, ret);
         HANDLE_EVENT(event_p, req_p, ret);
      }
      else
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
      }
   } while(FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      /* free the request */
      freeRequest(req_p);
   }

   else
   {
      /* set the structure pointer to the correct state */
      encryptObject_p->structureID = N8_ENCRYPT_STRUCT_ID;
   }

   DBG(("N8_EncryptInitialize - FINISHED\n"));
   return ret;
} /* N8_EncryptInitialize */

/******************************************************************************
 * N8_Encrypt 
 *****************************************************************************/
/** @ingroup crypto
 * @brief Encrypts message.
 *
 * Using the encryption algorithm and context (keys, etc.) contained in
 * EncryptObject, encrypt the plain text bytes in Message whose length is
 * MessageLength bytes,  returning the resulting cipher text bytes (including
 * any pad bytes added during encryption) in EncryptedMessage.  The length of
 * EncryptedMessage will depend on the encryption algorithm and the size of Message. 
 *
 * @param encryptObject_p   RW: The  encryption object to use, defining the 
 *                              encryption algorithm (DES or ARC4), the keys, etc.
 *                              The state in EncryptObject will be updated
 *                              appropriately as part of the call.<BR>
 * @param message_p         RO: The bytes to be encrypted<BR>
 * @param messageLength     RO  Length of Message in bytes, from 0 to 18 KBytes  
 *                              inclusive. A length of 0 is legal, but no bytes 
 *                              will actually be encrypted or returned in
 *                              EncryptedMessage<BR>
 * @param encryptedMessage_p WO: The result of encrypting the given Message 
 *                              with the specified encryption algorithm and keys 
 *                              etc in EncryptObject. This includes any pad bytes
 *                              added by the encryption algorithm. (DES will pad 
 *                              as necessary, ARC4 never will.)<BR>
 * @param event_p           RW: On input, if null the call is synchronous and no 
 *                              event is returned. The operation is complete when 
 *                              the call returns. If non-null, then the call is 
 *                              asynchronous; an event is returned that can be used 
 *                              to determine when the operation completes.<BR>
 *
 * @return 
 *    encryptObject_p - The state in PacketObject will be updated if necessary as 
 *                     part of the call.
 *    encryptedMessage_p - The encrypted data.
 *    ret   - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - object is zero, couldn't write/read unspecified
 *                          address<BR>
 *    N8_INVALID_INPUT_SIZE - The value of message length is less than 0 or bigger 
 *                            then 18 KBytes.
 *    N8_UNIMPLEMENTED_FUNCTION - not supported protocol configuration requested
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 * The caller must ensure that EncryptedMessage is of the required size.
 * (This will never be more than MessageLength + 7 for 
 * DES, and will always be exactly MessageLength for ARC4.)
 *****************************************************************************/
N8_Status_t N8_Encrypt(N8_EncryptObject_t *encryptObject_p,
                       const N8_Buffer_t  *message_p,
                       const unsigned int  messageLength,  
                       N8_Buffer_t        *encryptedMessage_p,
                       N8_Event_t         *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
     
   API_Request_t   *req_p = NULL;  /* request buffer */
   N8_Buffer_t     *plain_p = NULL;
   uint32_t         plain_a;
   N8_Buffer_t     *res_p = NULL;
   uint32_t         res_a;
   unsigned int     paddedMessageLength;
   unsigned int     nBytes;

   DBG(("N8_Encrypt\n"));
   do
   {
      /* verify data length */
      ret = verifyInputLength(messageLength);
      CHECK_RETURN(ret);

      /* verify objects */
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(message_p, ret);
      CHECK_OBJECT(encryptedMessage_p, ret);
      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      paddedMessageLength = messageLength;
  
      if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         unsigned int remainder;
         /* pad message if necessary.  the message length must be a multiple of
          * N8_DES_BLOCK_LENGTH */
         if ((remainder = messageLength % N8_DES_BLOCK_LENGTH))
         { 
            paddedMessageLength += N8_DES_BLOCK_LENGTH - remainder;
         }
      }
      nBytes = 2 * NEXT_WORD_SIZE(paddedMessageLength);

      /* create request buffer */
      ret = createEARequestBuffer(&req_p, encryptObject_p->unitID,
                                  N8_CB_EA_ENCRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      res_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      plain_a = res_a + NEXT_WORD_SIZE(paddedMessageLength);
      plain_p = res_p + NEXT_WORD_SIZE(paddedMessageLength);

      /* copy the user message into the kernel memory buffer */
      memcpy(plain_p, message_p, messageLength);
      /* set the padding bytes to 0x0 */
      memset(&plain_p[messageLength], 0x0, paddedMessageLength - messageLength);

      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p = encryptedMessage_p;
      req_p->copyBackSize = paddedMessageLength;
         
      ret = cb_ea_encrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          plain_a, 
                          res_a, 
                          paddedMessageLength);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      /* free the request */
      freeRequest(req_p);
   }
   DBG(("N8_Encrypt - FINISHED\n"));
   return ret;
} /* N8_Encrypt */
 

/* same as above, but for uio iovec i/o */
N8_Status_t N8_Encrypt_uio(N8_EncryptObject_t *encryptObject_p,
                       struct uio   *message_p,
                       const unsigned int messageLength,  
                       struct uio         *encryptedMessage_p,
                       N8_Event_t         *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
     
   API_Request_t   *req_p = NULL;  /* request buffer */
   N8_Buffer_t     *plain_p = NULL;
   uint32_t         plain_a;
   N8_Buffer_t     *res_p = NULL;
   uint32_t         res_a;
   unsigned int     paddedMessageLength;
   unsigned int     nBytes;

   DBG(("N8_Encrypt\n"));
   do
   {
      /* verify data length */
      ret = verifyInputLength(messageLength);
      CHECK_RETURN(ret);

      /* verify objects */
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(message_p, ret);
      CHECK_OBJECT(encryptedMessage_p, ret);
      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      paddedMessageLength = messageLength;
  
      if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         unsigned int remainder;
         /* pad message if necessary.  the message length must be a multiple of
          * N8_DES_BLOCK_LENGTH */
         if ((remainder = messageLength % N8_DES_BLOCK_LENGTH))
         { 
            paddedMessageLength += N8_DES_BLOCK_LENGTH - remainder;
         }
      }
      nBytes = 2 * NEXT_WORD_SIZE(paddedMessageLength);

      /* create request buffer */
      ret = createEARequestBuffer(&req_p, encryptObject_p->unitID,
                                  N8_CB_EA_ENCRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      res_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      plain_a = res_a + NEXT_WORD_SIZE(paddedMessageLength);
      plain_p = res_p + NEXT_WORD_SIZE(paddedMessageLength);

      /* copy the user message into the kernel memory buffer */
      cuio_copydata(message_p, 0, messageLength, plain_p);
      /* set the padding bytes to 0x0 */
      memset(&plain_p[messageLength], 0x0, paddedMessageLength - messageLength);

      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p = NULL;
      req_p->copyBackTo_uio = encryptedMessage_p;
      req_p->copyBackSize = paddedMessageLength;
         
      ret = cb_ea_encrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          plain_a, 
                          res_a, 
                          paddedMessageLength);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      /* free the request */
      freeRequest(req_p);
   }
   DBG(("N8_Encrypt - FINISHED\n"));
   return ret;
} /* N8_Encrypt */
/******************************************************************************
 * N8_EncryptPartial
 *****************************************************************************/
/** @ingroup crypto
 * @brief Encrypts message.
 *
 * Using the encryption algorithm and context (keys, etc.) contained in
 * EncryptObject, encrypt the plain text bytes in Message whose length is
 * MessageLength bytes,  returning the resulting cipher text bytes (including
 * any pad bytes added during encryption) in EncryptedMessage.  The length of
 * EncryptedMessage will depend on the encryption algorithm and the size of Message. 
 *
 * @param encryptObject_p   WO: The  encryption object to use, defining the 
 *                              encryption algorithm (DES or ARC4), the keys, etc.
 *                              The state in EncryptObject will be updated
 *                              appropriately as part of the call.<BR>
 * @param message_p         RO: The bytes to be encrypted<BR>
 * @param messageLength     RO  Length of Message in bytes, from 0 to 18 KBytes  
 *                              inclusive. A length of 0 is legal, but no bytes 
 *                              will actually be encrypted or returned in
 *                              EncryptedMessage<BR>
 * @param encryptedMessage_p WO: The result of encrypting the given Message 
 *                              with the specified encryption algorithm and keys 
 *                              etc in EncryptObject. This includes any pad bytes
 *                              added by the encryption algorithm. (DES will pad 
 *                              as necessary, ARC4 never will.)<BR>
 * @param encryptedMsgLen_p WO: The length of the encrypted message.
 * @param event_p           RW: On input, if null the call is synchronous and no 
 *                              event is returned. The operation is complete when 
 *                              the call returns. If non-null, then the call is 
 *                              asynchronous; an event is returned that can be used 
 *                              to determine when the operation completes.<BR>
 *
 * @return 
 *    packetObject_p - The state in PacketObject will be updated if necessary as 
 *                     part of the call.
 *    encryptedMessage_p - The encrypted data.
 *    ret   - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - object is NULL, couldn't write/read unspecified
 *                          address<BR>
 *    N8_INVALID_INPUT_SIZE - The value of messageLength is less than 0 or bigger 
 *                            then 18 KBytes.
 *    N8_UNIMPLEMENTED_FUNCTION - not supported protocol configuration requested
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 * The caller must ensure that EncryptedMessage is of the required size.
 * (This will never be more than MessageLength + 7 for 
 * DES, and will always be exactly MessageLength for ARC4.)
 *****************************************************************************/
N8_Status_t N8_EncryptPartial(N8_EncryptObject_t *encryptObject_p,
                              const N8_Buffer_t  *message_p,
                              const unsigned int  messageLength,  
                              N8_Buffer_t        *encryptedMessage_p,
                              unsigned int       *encryptedMsgLen_p,
                              N8_Event_t         *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;         /* the return status: OK or ERROR */
     
   API_Request_t  *req_p = NULL;           /* request buffer */
   N8_Buffer_t    *plain_p = NULL;         /* virtual address of message to be
                                            * encrypted this time. */
   N8_Buffer_t    *res_p = NULL;           /* virtual address of encrypted
                                            * message. */
   uint32_t        plain_a;                /* physical address of message to be
                                            * encrypted.  */
   uint32_t        res_a;                  /* physical address of encrypted
                                            * message.  */
   unsigned int totalLength = 0;           /* total length of this message plus
                                            * any residual from previous
                                            * calls. */
   unsigned int msgLen = 0;                /* the length of the message to be
                                            * encrypted in this call. */
   unsigned int nextResidualLength = 0;   /* amount leftover from this
                                            * call. */ 
   unsigned int fromThisMessageLength = 0; /* amount of taken from this
                                            * message */
   unsigned int nBytes;
   encryptData_t  *callBackData_p = NULL;
   
   DBG(("N8_EncryptPartial\n"));
   do
   {
      /* verify data length */
      ret = verifyInputLength(messageLength);
      CHECK_RETURN(ret);

      /* verify objects */
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(message_p, ret);
      CHECK_OBJECT(encryptedMessage_p, ret);
      CHECK_OBJECT(encryptedMsgLen_p, ret);
      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      /* sanity check the residual length */
      if (encryptObject_p->residualLength >= N8_DES_BLOCK_LENGTH)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      
      /* set the value for the returned encrypted message length to zero.  it
       * will be set finally in the callback at the completion of the call (sync
       * or async). */
      *encryptedMsgLen_p = 0;
      if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         totalLength = encryptObject_p->residualLength + messageLength;
         nextResidualLength = totalLength % N8_DES_BLOCK_LENGTH;
         msgLen = totalLength - nextResidualLength;
         fromThisMessageLength = messageLength - nextResidualLength;
      }
      else if (encryptObject_p->cipher == N8_CIPHER_ARC4)
      {
         fromThisMessageLength = msgLen = messageLength;
      }
      else
      {
         ret = N8_INVALID_CIPHER;
         break;
      }

      /* check to ensure we actually have some message to encrypt this go
       * round. */
      if (msgLen == 0)
      {
         /* copy the input to the end of the residual */
         memcpy(&encryptObject_p->residual_p[encryptObject_p->residualLength],
                message_p,
                messageLength);
         encryptObject_p->residualLength += messageLength;

         /* if this was an asynchronous call, set the status to finished */
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }

      nBytes = 2 * NEXT_WORD_SIZE(msgLen);

      /* create request buffer */
      ret = createEARequestBuffer(&req_p,
                                  encryptObject_p->unitID,
                                  N8_CB_EA_ENCRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      plain_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      plain_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = plain_p + NEXT_WORD_SIZE(msgLen);
      res_a = plain_a + NEXT_WORD_SIZE(msgLen);

      /* copy any previous residual to the m-t-b-e buffer */
      if (encryptObject_p->residualLength > 0)
      {
         memcpy(plain_p,
                encryptObject_p->residual_p,
                encryptObject_p->residualLength);
         /* advance the message pointer */
         plain_p += encryptObject_p->residualLength;
      }

      /* copy the new message portion to the end of the m-t-b-e buffer */
      if (fromThisMessageLength > 0)
      {
         memcpy(plain_p,
                message_p,
                fromThisMessageLength);
      }
      
      /* copy leftovers to the residual */
      if (nextResidualLength > 0)
      {
         /* not necessary, but let's zero out the remaining message buffer */
         memset(encryptObject_p->residual_p, 0x0, 
                N8_DES_BLOCK_LENGTH);
         memcpy(encryptObject_p->residual_p,
                &message_p[fromThisMessageLength],
                nextResidualLength);
      }
      encryptObject_p->residualLength = nextResidualLength;

      /* set the copy back parameters for returning the results to the user.
       * the main data is represented by the copy back structures in the request
       * and the extra data is in the post-processing data. */
      callBackData_p = (encryptData_t *)&req_p->postProcessBuffer;
      req_p->postProcessingData_p = (void *) callBackData_p;

      callBackData_p->encryptedMsgLen_p = encryptedMsgLen_p;
      callBackData_p->encryptObject_p = encryptObject_p;
      if (encryptObject_p->contextHandle.inUse == N8_FALSE)
      {
         callBackData_p->nextIV_p = (uint32_t *) (res_p + msgLen -
                                                  N8_DES_BLOCK_LENGTH);
      }
      else
      {
         callBackData_p->nextIV_p = NULL;
      }
      req_p->copyBackSize   = msgLen;
      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p   = encryptedMessage_p;
      ret = cb_ea_encrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          plain_a,
                          res_a, 
                          msgLen);
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_EncryptPartial - FINISHED\n"));
   return ret;
} /* N8_EncryptPartial */
 
/******************************************************************************
 * N8_EncryptEnd
 *****************************************************************************/
/** @ingroup crypto
 * @brief Encrypts message.
 *
 * Using the encryption algorithm and context (keys, etc.) contained in
 * EncryptObject, encrypt the plain text bytes in Message whose length is
 * MessageLength bytes,  returning the resulting cipher text bytes (including
 * any pad bytes added during encryption) in EncryptedMessage.  The length of
 * EncryptedMessage will depend on the encryption algorithm and the size of Message. 
 *
 * @param encryptObject_p   WO: The  encryption object to use, defining the 
 *                              encryption algorithm (DES or ARC4), the keys, etc.
 *                              The state in EncryptObject will be updated
 *                              appropriately as part of the call.<BR>
 * @param message_p         RO: The bytes to be encrypted<BR>
 * @param messageLength     RO  Length of Message in bytes, from 0 to 18 KBytes  
 *                              inclusive. A length of 0 is legal, but no bytes 
 *                              will actually be encrypted or returned in
 *                              EncryptedMessage<BR>
 * @param encryptedMessage_p WO: The result of encrypting the given Message 
 *                              with the specified encryption algorithm and keys 
 *                              etc in EncryptObject. This includes any pad bytes
 *                              added by the encryption algorithm. (DES will pad 
 *                              as necessary, ARC4 never will.)<BR>
 * @param event_p        RW:    On input, if null the call is synchronous and no 
 *                              event is returned. The operation is complete when 
 *                              the call returns. If non-null, then the call is 
 *                              asynchronous; an event is returned that can be used 
 *                              to determine when the operation completes.<BR>
 *
 *
 * @return 
 *    packetObject_p - The state in PacketObject will be updated if necessary as 
 *                     part of the call.
 *    encryptedMessage_p - The encrypted data.
 *    ret   - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - object is zero, couldn't write/read unspecified
 *                          address<BR>
 *    N8_UNIMPLEMENTED_FUNCTION - not supported protocol configuration requested
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 * The caller must ensure that EncryptedMessage is of the required size.
 * (This will never be more than MessageLength + 7 for 
 * DES, and will always be exactly MessageLength for ARC4.)
 *****************************************************************************/
N8_Status_t N8_EncryptEnd(N8_EncryptObject_t *encryptObject_p,
                          N8_Buffer_t        *encryptedMessage_p,
                          unsigned int       *encryptedMsgLen_p,  
                          N8_Event_t         *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
     
   API_Request_t   *req_p = NULL;       /* request buffer */
   N8_Buffer_t     *plain_p = NULL;
   uint32_t         plain_a;
   N8_Buffer_t     *res_p = NULL;
   uint32_t         res_a;
   unsigned int     paddedMessageLength = 0;
   unsigned int     nBytes;
   encryptData_t  *callBackData_p = NULL;

   DBG(("N8_EncryptEnd\n"));
   do
   {
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(encryptedMessage_p, ret);
      CHECK_OBJECT(encryptedMsgLen_p, ret);

      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      /* sanity check the residual length */
      if (encryptObject_p->residualLength >= N8_DES_BLOCK_LENGTH)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      
      *encryptedMsgLen_p = 0;
      /* check to see if there is no work to be done. */
      if (encryptObject_p->residualLength == 0)
      { 
         /* if this was an asynchronous call, set the status to finished */
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }

      paddedMessageLength = encryptObject_p->residualLength;

      if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         /* pad message */
         paddedMessageLength =
            CEIL(encryptObject_p->residualLength, N8_DES_BLOCK_LENGTH) *
            N8_DES_BLOCK_LENGTH; 
      }

      nBytes = 2 * NEXT_WORD_SIZE(paddedMessageLength);

      /* create request buffer */
      ret = createEARequestBuffer(&req_p,
                                  encryptObject_p->unitID,
                                  N8_CB_EA_ENCRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      res_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      plain_a = res_a + NEXT_WORD_SIZE(paddedMessageLength);
      plain_p = res_p + NEXT_WORD_SIZE(paddedMessageLength);

      /* copy the residual message into the kernel memory buffer */
      memcpy(plain_p,
             encryptObject_p->residual_p,
             encryptObject_p->residualLength);
      /* set the padding bytes to 0x0 */
      memset(&plain_p[encryptObject_p->residualLength],
             0x0,
             paddedMessageLength - encryptObject_p->residualLength);

      /* set the copy back parameters for returning the results to the user.
       * the main data is represented by the copy back structures in the request
       * and the extra data is in the post-processing data. */
      callBackData_p = (encryptData_t *)&req_p->postProcessBuffer;
      req_p->postProcessingData_p = (void *) callBackData_p;
      callBackData_p->encryptedMsgLen_p = encryptedMsgLen_p;
      callBackData_p->encryptObject_p = encryptObject_p;
      callBackData_p->nextIV_p = NULL;
      req_p->copyBackSize   = paddedMessageLength;
      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p   = encryptedMessage_p;
      encryptObject_p->residualLength = 0;

      ret = cb_ea_encrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          plain_a,
                          res_a,
                          paddedMessageLength);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_EncryptEnd - FINISHED\n"));
   return ret;

} /* N8_EncryptEnd */
 
/******************************************************************************
 * N8_Decrypt
 *****************************************************************************/
/** @ingroup crypto
 * @brief Decrypts message.
 *
 * Using the algorithm and context (keys, etc.) contained in EncryptObject, 
 * decrypt the cipher text bytes in EncryptedMessage whose length is
 * EncryptedMessageLength bytes,  returning the resulting plain text bytes
 * (including any pad bytes added during encryption) in EncryptedMessage. 
 * The length of Message will always be EncryptedMessageLength.  Either DES
 * or ARC4 encryption can be specified in EncryptObject. DES encryption pads
 * if necessary so Message may contain pad bytes; ARC4 decrypted messages will
 * never contain pad bytes. 
 *
 * @param encryptObject_p        WR: The  encryption object to use, defining 
 *                                   the encryption algorithm (DES or
 *                                   ARC4), the keys, etc. The state in 
 *                                   EncryptObject will be updated
 *                                   appropriately as part of the call.  For 
 *                                   example, an ARC4 object would 
 *                                   have its key information updated.<BR> 
 * @param encryptedMessage_p     RO: The bytes to be decrypted.<BR>
 * @param encryptedMsgLen_p RO: Length of EncryptedMessage in bytes, 
 *                                   from 0 to 18 KBytes  inclusive.  A 
 *                                               length of 0 is legal, but no bytes will 
 *                                   actually be decrypted or returned in
 *                                   EncryptedMessage. For DES, 
 *                                   EncryptedMessageLength must be a 
 *                                   multiple of 8, the DES block size. 
 * @param message_p              WO: The result of decrypting the given 
 *                                   EncryptedMessage with the specified 
 *                                               algorithm and keys etc in EncryptObject. 
 *                                   This includes any pad bytes
 *                                   added by the encryption algorithm. 
 *                                   (DES will pad as necessary, ARC4 never will.)  
 *                                   Thus Message is always of size MessageLength, 
 *                                   the caller must ensure that Message is of at 
 *                                   least this size.<BR>
 * @param event_p        RW:    On input, if null the call is synchronous and no 
 *                              event is returned. The operation is complete when 
 *                              the call returns. If non-null, then the call is 
 *                              asynchronous; an event is returned that can be used 
 *                              to determine when the operation completes.
 *
 *
 * @return 
 *    encryptObject_p - The state in PacketObject will be updated if necessary as 
 *                     part of the call.
 *    message_p       - The encrypted data.
 *    ret   - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - encrypt object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_INVALID_INPUT_SIZE - The value of message length is less than 0 or bigger 
 *                            then 18 KBytes.
 *    N8_HARDWARE_ERROR   - couldn't decrypt
 *   
 *    N8_MALLOC_FAILED    - memory allocation failed
 *
 * @par Assumptions:
 *****************************************************************************/
N8_Status_t N8_Decrypt(N8_EncryptObject_t *encryptObject_p,
                       const N8_Buffer_t  *encryptedMessage_p,
                       const unsigned int  encryptedMsgLen,  
                       N8_Buffer_t        *message_p,
                       N8_Event_t         *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
   API_Request_t  *req_p = NULL;       /* request buffer */
   N8_Buffer_t     *enc_p = NULL;
   uint32_t         enc_a;
   N8_Buffer_t     *res_p = NULL;
   uint32_t         res_a;
   unsigned int     nBytes;

   DBG(("N8_Decrypt\n"));
   do {
      /* verify data length */
      ret = verifyInputLength(encryptedMsgLen);
      CHECK_RETURN(ret);

      /* verify objects */
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(message_p, ret);
      CHECK_OBJECT(encryptedMessage_p, ret);
      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         /* Check to ensure the message length is a multiple of
          * N8_DES_BLOCK_LENGTH.  Return an error if not.
          */
         if (encryptedMsgLen % N8_DES_BLOCK_LENGTH)
         { 
            ret = N8_INVALID_INPUT_SIZE;
            break;
         }
      }

      nBytes = 2 * NEXT_WORD_SIZE(encryptedMsgLen);

      /* create request buffer */
      ret = createEARequestBuffer(&req_p,
                                  encryptObject_p->unitID,
                                  N8_CB_EA_DECRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      res_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      enc_a = res_a + NEXT_WORD_SIZE(encryptedMsgLen);
      enc_p = res_p + NEXT_WORD_SIZE(encryptedMsgLen);
      
      /* copy the encrypted message into the kernel memory buffer */
      memcpy(enc_p, encryptedMessage_p, encryptedMsgLen);

      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p = message_p;
      req_p->copyBackSize = encryptedMsgLen;

      ret = cb_ea_decrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          enc_a,
                          res_a,
                          encryptedMsgLen);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      /* free the request */
      freeRequest(req_p);
   }
   DBG(("N8_Decrypt - FINISHED\n"));
   return ret;
} /* N8_Decrypt */


/* same as above, but for uio iovec i/o */
N8_Status_t N8_Decrypt_uio(N8_EncryptObject_t *encryptObject_p,
                       struct uio         *encryptedMessage_p,
                       const unsigned int  encryptedMsgLen,  
                       struct uio         *message_p,
                       N8_Event_t         *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
   API_Request_t  *req_p = NULL;       /* request buffer */
   N8_Buffer_t     *enc_p = NULL;
   uint32_t         enc_a;
   N8_Buffer_t     *res_p = NULL;
   uint32_t         res_a;
   unsigned int     nBytes;

   DBG(("N8_Decrypt\n"));
   do {
      /* verify data length */
      ret = verifyInputLength(encryptedMsgLen);
      CHECK_RETURN(ret);

      /* verify objects */
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(message_p, ret);
      CHECK_OBJECT(encryptedMessage_p, ret);
      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         /* Check to ensure the message length is a multiple of
          * N8_DES_BLOCK_LENGTH.  Return an error if not.
          */
         if (encryptedMsgLen % N8_DES_BLOCK_LENGTH)
         { 
            ret = N8_INVALID_INPUT_SIZE;
            break;
         }
      }

      nBytes = 2 * NEXT_WORD_SIZE(encryptedMsgLen);

      /* create request buffer */
      ret = createEARequestBuffer(&req_p,
                                  encryptObject_p->unitID,
                                  N8_CB_EA_DECRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      res_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      enc_a = res_a + NEXT_WORD_SIZE(encryptedMsgLen);
      enc_p = res_p + NEXT_WORD_SIZE(encryptedMsgLen);
      
      /* copy the encrypted message into the kernel memory buffer */
      cuio_copydata(encryptedMessage_p, 0, encryptedMsgLen, enc_p);

      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p = NULL;
      req_p->copyBackTo_uio = message_p;
      req_p->copyBackSize = encryptedMsgLen;

      ret = cb_ea_decrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          enc_a,
                          res_a,
                          encryptedMsgLen);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      /* free the request */
      freeRequest(req_p);
   }
   DBG(("N8_Decrypt - FINISHED\n"));
   return ret;
} /* N8_Decrypt */
 
/******************************************************************************
 * N8_DecryptPartial
 *****************************************************************************/
/** @ingroup crypto
 * @brief Decrypts partial message.
 * For stream decryption algorithms (ARC4), this function is identical to 
 * N8_Decrypt called with the same parameters, with the additional output 
 * parameter MessageLength always set equal to EncryptedMessageLength on return.
 *
 * For block decryption algorithms (DES), this function starts or continues,
 * but does not complete, decrypting the bytes given in EncryptedMessage. 
 * The partial message in EncryptedMessage can be any number of bytes from 0
 * to 18 KBytes inclusive, its size is specified in EncryptedMessageLength. 
 * The EncryptObject specifies the decryption algorithm to use, and must have
 * been initialized by a call to N8_EncryptInitialize prior to the first call
 * to N8_DecryptPartial. If this is the first call to this routine since the
 * EncryptObject was initialized, then the call starts the decryption of a message. 
 * Subsequent calls to N8_DecryptPartial with the same EncryptObject with additional
 * bytes in EncryptedMessage allow the decryption to be continued, and a final
 * call to N8_DecryptEnd finishes the computation and returns the final decrypted
 * result.  Note that for block decryption algorithms, only whole message blocks
 * will be decrypted and returned to the caller on each call to N8_DecryptPartial.
 * Any message bytes left over (i.e., not forming a complete decryption block)
 * will be accumulated in the EncryptObject and added to the message bytes in
 * the next N8_DecryptPartial call.  For this reason, the number of bytes
 * decrypted and returned in Message may be different than the number of input
 * bytes specified in EncryptedMessageLength. The actual number of bytes
 * decrypted will always be an integral multiple of the block size of the
 * encryption algorithm (8 bytes for DES) and is returned in MessageLength.
 * Note in particular that this value can be 0, and it can be larger than the
 * input length by as much as one less than the block size (8 - 1 = 7 for DES).
 * That is, the maximum result length is the next multiple of the block size
 * that is larger than EncryptedMessageLength-1
 * (maximum length = ((EncryptedMessageLength / block size) + 1) * block size).
 * The caller is responsible for ensuring that Message can hold a message
 * of the required size. Note that while this call does not require that
 * EncryptedMessageLength be a multiple of the block size, if the length of
 * the entire message to be decrypted over multiple calls is not a multiple
 * of the block size, an error will occur eventually
 *
 * @param encryptObject_p      RW: The  encryption object to use, defining
 *                                 the encryption algorithm (DES or
 *                                 ARC4), the keys, etc. The state in
 *                                 EncryptObject will be updated
 *                                 appropriately as part of the call. 
 *                                 For example, an ARC4 object would have
 *                                 its key information updated.
 * @param encryptedMessage_p   RO: The bytes to be decrypted.
 * @param encryptedMsgLen_p    RO: Length of EncryptedMessage in bytes,
 *                                 from 0 to 18 KBytes  inclusive.  A 
 *                                 length of  0 is legal, but no bytes will
 *                                 actually be decrypted or returned in
 *                                 message.
 * @param message_p            WO: The partial result of decrypting the
 *                                 given EncryptedMessage with the 
 *                                 specified encryption algorithm and keys
 *                                 etc in EncryptObject. The caller must
 *                                 ensure that Message is of the required
 *                                 size. (This will never be more than
 *                                 EncryptedMessageLength + 7 for DES.
 * @param messageLength        WO: The number of bytes encrypted by this call and
 *                                 returned in Message.  Always a multiple of
 *                                 the block size (8 for DES).  This value may
 *                                 be 0 and may be as large as the next multiple
 *                                 of the block size that is greater than
 *                                 EncryptedMessageLength. For stream algorithms
 *                                 this will always equal MessageLength and will
 *                                 always be exactly MessageLength for * ARC4.)

 * @param event_p              RW: On input, if null the call is synchronous and
 *                                 no event is returned. The operation is
 *                                 complete when the call returns. If
 *                                 non-null, then the call is asynchronous; an
 *                                 event is returned that can be used to
 *                                 determine when the operation completes.<BR>
 *
 * @return
 * Message               WO:    The partial result of decrypting the
 *                              given EncryptedMessage with the 
 *                              specified encryption algorithm and keys
 *                              etc in EncryptObject. 
 * MessageLength         WO:     The number of bytes encrypted by this call
 *                           and returned in Message.
 * @par Errors:
 *    N8_INVALID_OBJECT   - encrypt object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_INVALID_INPUT_SIZE - The value of message length is less than 0 or bigger 
 *                            then 18 KBytes.
 *    N8_HARDWARE_ERROR   - couldn't decrypt
 *    N8_MALLOC_FAILED    - memory allocation failed
 *
 * @par Assumptions:
 *****************************************************************************/
N8_Status_t N8_DecryptPartial(N8_EncryptObject_t *encryptObject_p,
                              const N8_Buffer_t  *encryptedMessage_p,
                              const unsigned int  encryptedMsgLen,  
                              N8_Buffer_t        *message_p,
                              unsigned int       *messageLength_p,  
                              N8_Event_t         *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
     
   API_Request_t  *req_p = NULL;       /* request buffer */
   N8_Buffer_t    *enc_p = NULL;
   N8_Buffer_t    *cipherText_p = NULL;
   uint32_t        enc_a;
   N8_Buffer_t    *res_p = NULL;
   uint32_t        res_a;

   unsigned int totalLength = 0;           /* total length of this message plus
                                            * any residual from previous
                                            * calls. */
   unsigned int msgLen = 0;                /* the length of the message to be
                                            * encrypted in this call. */
   unsigned int nextResidualLength = 0;   /* amount leftover from this
                                            * call. */ 
   unsigned int fromThisMessageLength = 0; /* amount of taken from this
                                            * message */ 
   encryptData_t  *callBackData_p = NULL;
   unsigned int    nBytes;

   DBG(("N8_DecryptPartial\n"));
   do
   {
      /* verify data length */
      ret = verifyInputLength(encryptedMsgLen);
      CHECK_RETURN(ret);

      /* verify objects */
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(message_p, ret);
      CHECK_OBJECT(messageLength_p, ret);
      CHECK_OBJECT(encryptedMessage_p, ret);
      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      /* sanity check the residual length */
      if (encryptObject_p->residualLength >= N8_DES_BLOCK_LENGTH)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      
      /* set the value for the returned decrypted message length to zero.  it
       * will be set finally in the callback at the completion of the call (sync
       * or async). */
      *messageLength_p = 0;
      switch (encryptObject_p->cipher)
      {
         case N8_CIPHER_DES:
         {
            totalLength = encryptObject_p->residualLength + encryptedMsgLen;
            nextResidualLength = totalLength % N8_DES_BLOCK_LENGTH;
            msgLen = totalLength - nextResidualLength;
            fromThisMessageLength = encryptedMsgLen - nextResidualLength;
            break;
         }
         case N8_CIPHER_ARC4:
         {
            fromThisMessageLength = msgLen = encryptedMsgLen;
            break;
         }
      }

      /* check to ensure we actually have some message to encrypt this go
       * round. */
      if (msgLen == 0)
      {
         /* copy the input to the end of the residual */
         memcpy(&encryptObject_p->residual_p[encryptObject_p->residualLength],
                encryptedMessage_p,
                encryptedMsgLen);
         encryptObject_p->residualLength += encryptedMsgLen;

         /* if this was an asynchronous call, set the status to finished */
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }

      nBytes = 2 * NEXT_WORD_SIZE(msgLen);
      ret = createEARequestBuffer(&req_p,
                                  encryptObject_p->unitID,
                                  N8_CB_EA_DECRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      enc_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      enc_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = enc_p + NEXT_WORD_SIZE(msgLen);
      res_a = enc_a + NEXT_WORD_SIZE(msgLen);

      cipherText_p = enc_p;

      /* copy any previous residual to the m-t-b-e buffer */
      if (encryptObject_p->residualLength > 0)
      {
         memcpy(cipherText_p,
                encryptObject_p->residual_p,
                encryptObject_p->residualLength);
         /* advance the message pointer */
         cipherText_p += encryptObject_p->residualLength;
      }

      /* copy the new message portion to the end of the m-t-b-e buffer */
      if (fromThisMessageLength > 0)
      {
         memcpy(cipherText_p,
                encryptedMessage_p,
                fromThisMessageLength);
      }
      
      /* copy leftovers to the residual */
      if (nextResidualLength > 0)
      {
         /* not necessary, but let's zero out the remaining message buffer */
         memset(encryptObject_p->residual_p, 0x0, 
                N8_DES_BLOCK_LENGTH);
         memcpy(encryptObject_p->residual_p,
                &encryptedMessage_p[fromThisMessageLength],
                nextResidualLength);
      }
      
      encryptObject_p->residualLength = nextResidualLength;

      /* set the copy back parameters for returning the results to the user.
       * the main data is represented by the copy back structures in the request
       * and the extra data is in the post-processing data. */
      callBackData_p = (encryptData_t *)&req_p->postProcessBuffer;
      req_p->postProcessingData_p = (void *) callBackData_p;

      callBackData_p->encryptedMsgLen_p = messageLength_p;
      callBackData_p->encryptObject_p = encryptObject_p;
      if (encryptObject_p->contextHandle.inUse == N8_FALSE)
      {
         callBackData_p->nextIV_p = (uint32_t *) (enc_p + msgLen -
                                                  N8_DES_BLOCK_LENGTH); 
      }
      else
      {
         callBackData_p->nextIV_p = NULL;
      }
      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p   = message_p;
      req_p->copyBackSize   = msgLen;
      ret = cb_ea_decrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          enc_a,
                          res_a,
                          msgLen);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      /* free the request */
      freeRequest(req_p);
   }
   DBG(("N8_DecryptPartial - FINISHED\n"));
   return ret;
} /* N8_DecryptPartial */
 
/******************************************************************************
 * N8_DecryptEnd
 *****************************************************************************/
/** @ingroup crypto
 * @brief Decrypts message.
 *
 * Completes a sequence of partial decrypt operations by encrypting any residual
 * message bytes in EncryptObject, returning the resulting MessageLength bytes
 * in Message.  
 * For stream encryption algorithms (ARC4), this function does nothing as there
 * will never be residual message bytes in EncryptObject after an
 * N8_DecryptPartial operation. MessageLength will be 0 on return and no
 * decrypted bytes will be returned in Message. 
 * For block encryption algorithms (DES), this completes decrypting any message
 * bytes remaining in EncryptObject from previous N8_DecryptPartial calls. 
 * The EncryptObject specifies the decryption algorithm to use, and must have
 * been initialized by a call to N8_EncryptInitialize prior to the first call
 * to N8_DecryptPartial; it may have been used in one or more N8_DecryptPartial
 * calls to process previous parts of the message. If this is the first call made
 * with EncryptObject since it was initialized, then the call does nothing; 0 is
 * returned in MessageLength and no bytes are written to Message. If previous
 * calls to N8_DecryptPartial with EncryptObject were made, then this final
 * call to N8_DecryptEnd finishes the computation and returns the final decrypted
 * result.  Note that for block decryption algorithms, this final decryption must
 * result in a total length over all N8_DecryptPartial & N8_DecryptEnd calls that
 * is a multiple of the block size. This means that if there are any residual
 * message bytes accumulated in the EncryptObject as a result of previous
 * N8_DecryptPartial calls that the total length is not a multiple of the block
 * size, and an error will result.  Thus, a successful call will never result in
 * any bytes being decrypted. The actual number of bytes decrypted will always be 0.
 * The caller is responsible for ensuring that Message can hold a message of this
 * required size.
 *
 * @param encryptObject_p      WR: The encryption object to use, defining 
 *                                 the encryption algorithm (DES or
 *                                 ARC4), the keys, etc. The state in 
 *                                 EncryptObject will be updated
 *                                 appropriately as part of the call.  For 
 *                                 example, an ARC4 object would 
 *                                 have its key information updated.<BR>
 * @param MessageLength        RO: The number of bytes decrypted by this call and
 *                                 returned in Message.  Should always a
 *                                 multiple of the block size (8 for DES).
 *                                 This value may be 0 and may be as large as
 *                                 (EncryptedMessageLength + block size - 1)
 *                                 mod (block size).  For stream algorithms
 *                                 this will always equal MessageLength
 * @param message_p            WO: The result of decrypting the given 
 *                                 EncryptedMessage with the specified 
 *                                 algorithm and keys etc in EncryptObject. 
 *                                 This includes any pad bytes
 *                                 added by the encryption algorithm. 
 *                                 (DES will pad as necessary, ARC4 never will.)  
 *                                 Thus Message is always of size MessageLength, 
 *                                 the caller must ensure that Message is of at 
 *                                 least this size.<BR>
 * @param event_p              RW: On input, if null the call is synchronous and no 
 *                                 event is returned. The operation is complete when 
 *                                 the call returns. If non-null, then the call is 
 *                                 asynchronous; an event is returned that can be used 
 *                                 to determine when the operation completes.
 *
 *
 * @return 
 *    encryptObject_p - The state in encryptObject_p will be updated if necessary as 
 *                     part of the call.
 *    message_p       - The decrypted data.
 *    ret   - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - packet object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_MALLOC_FAILED    - memory allocation failed
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 *      packetObject_p was initialized and all parameters checked.<BR>
 *****************************************************************************/
N8_Status_t N8_DecryptEnd(N8_EncryptObject_t *encryptObject_p,
                          N8_Buffer_t        *message_p,
                          unsigned int       *messageLength_p,  
                          N8_Event_t         *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
     
   API_Request_t   *req_p = NULL;       /* request buffer */
   N8_Buffer_t     *enc_p = NULL;
   uint32_t         enc_a;
   N8_Buffer_t     *res_p = NULL;
   uint32_t         res_a;
   unsigned int     nBytes;
   encryptData_t   *callBackData_p = NULL;

   DBG(("N8_DecryptEnd\n"));
   do
   {
      /* verify objects */
      CHECK_OBJECT(encryptObject_p, ret);
      CHECK_OBJECT(message_p, ret);
      CHECK_OBJECT(messageLength_p, ret);

      CHECK_STRUCTURE(encryptObject_p->structureID, N8_ENCRYPT_STRUCT_ID, ret);

      ret = checkCipher(encryptObject_p->cipher);
      CHECK_RETURN(ret);

      /* sanity check the residual length */
      if (encryptObject_p->residualLength >= N8_DES_BLOCK_LENGTH)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      
      *messageLength_p = 0;
      /* check to see if there is no work to be done. */
      if (encryptObject_p->residualLength == 0)
      { 
         /* if this was an asynchronous call, set the status to finished */
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }

      if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         if (encryptObject_p->residualLength % N8_DES_BLOCK_LENGTH)
         {
            ret = N8_INVALID_INPUT_SIZE;
            break;
         }
         
      }

      /* create request buffer */
      nBytes = 2 * NEXT_WORD_SIZE(encryptObject_p->residualLength);
      ret = createEARequestBuffer(&req_p,
                                  encryptObject_p->unitID,
                                  N8_CB_EA_DECRYPT_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      res_a = req_p->qr.physicalAddress + req_p->dataoffset;
      res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      enc_a = res_a + NEXT_WORD_SIZE(encryptObject_p->residualLength);
      enc_p = res_p + NEXT_WORD_SIZE(encryptObject_p->residualLength);

      /* copy the residual message into the kernel memory buffer */
      memcpy(enc_p,
             encryptObject_p->residual_p,
             encryptObject_p->residualLength);

      /* set the copy back parameters for returning the results to the user.
       * the main data is represented by the copy back structures in the request
       * and the extra data is in the post-processing data. */
      callBackData_p = (encryptData_t *)&req_p->postProcessBuffer;
      req_p->postProcessingData_p = (void *) callBackData_p;
      callBackData_p->encryptedMsgLen_p = messageLength_p;
      callBackData_p->encryptObject_p = encryptObject_p;
      callBackData_p->nextIV_p = NULL;
      req_p->copyBackSize   = encryptObject_p->residualLength;
      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p   = message_p;
      encryptObject_p->residualLength = 0;

      ret = cb_ea_decrypt(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          encryptObject_p, 
                          enc_a,
                          res_a,
                          encryptObject_p->residualLength);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_DecryptEnd - FINISHED\n"));
   return ret;
} /* N8_DecryptEnd */
 
