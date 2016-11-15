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

static char const n8_id[] = "$Id: n8_hash.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_hash.c
 *  @brief Hashing functionality.
 *
 * Implementation of all public functions dealing with management of
 * hashs.
 *
 * References:
 * MD5: RFC1321 "The MD5 Message-Digest Algorithm", R. Rivest, 4/92
 * SHA-1: FIPS Pub 180-1,"Secure Hash Standard", US Dept. of Commerce,
 *    4/17/95
 * HMAC: RFC2104 "HMAC: Keyed-Hashing for Message Authentication", H. Krawczyk,
 *    2/97. 
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:    
 * 07/28/03 brr   Removed obsolete #ifdefs. (Bug 918)
 * 07/01/03 brr   Added option to use no hashing algorithm.
 * 05/19/03 brr   Use post processing buffer in API request instead of
 *                performing a malloc for it.
 * 09/10/02 brr   Set command complete bit on last command block.
 * 08/13/02 brr   Correctly size hashedKey array.
 * 08/06/02 bac   Fixed Bug #842 -- user's cipher info was being modified.
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/15/02 brr   Removed call to n8_addSubRequest.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 05/01/02 brr   Memset data segment after return from createEARequestBuffer.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/25/02 brr   Removed last of the QMgr references.
 * 02/22/02 spm   Converted printf's to DBG's.  Added include of n8_OS_intf.h.
 * 02/21/02 brr   Removed references to QMgr_get_valid_unit_num.
 * 01/23/02 bac   Correctly initialize for HMAC by setting the Nl and opad_Nl
 *                values and by setting the hashedHMACKey.
 * 01/22/02 bac   Added code to use software to do SSL and HMAC precomputes.
 * 01/21/02 bac   Changes to support new kernel allocation scheme.
 * 01/18/02 bac   Fixed a problem with kernel memory allocation sizing.
 * 01/18/02 bac   Corrected a bug in the setting of the hashed HMAC key.  If the
 *                key was odd in size, the final byte was being dropped.
 *                (BUG #482).  Also removed a debugging message that was
 *                inadvertantly left in (BUG #479).
 * 01/16/02 bac   Several routines were allocating the maximum size allowed for
 *                hashing when kernel memory was needed.  This was grossly
 *                inefficient and slow.  The routines were changed to only
 *                allocate the exact amount needed.
 * 01/14/02 bac   Changed resultHandlerHMACInit to copy the byte-swapped results
 *                back to the kernel memory context for proper formatting in the
 *                subsequent load context request.
 * 01/12/02 bac   Removed all blocking calls within a single API method.  This
 *                required the restructuring of the N8_HashInitialize,
 *                N8_HashPartial, and N8_HashEnd calls, especially as they dealt
 *                with HMAC.  (BUG #313)
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/15/01 mel   Fixed bug #291 : N8_HashInitialize does not modify event
 *                structure 
 * 11/12/01 hml   Added unitID to initMD5 and initSHA1.
 * 11/12/01 hml   Added structure verification: bug #261
 * 11/08/01 mel   Fixed bug #289 : by passing unitID parameter
 * 11/08/01 mel   Fixed bug# 272: N8_IKEEncryptKeyExpand and 
 *                N8_IKEKeyMaterialExpand seg faults 
 * 11/08/01 mel   Fixed bug# 271: N8_IKEEncryptKeyExpand and 
 *                N8_IKEKeyMaterialExpand returns incorrect error code for 
 *                outputLength=0.
 * 11/06/01 dkm   Added null error check to MD5_/SHA1_Init.
 * 10/30/01 bac   Removed incrementHashObjectLengths and replaced with shared
 *                function n8_incrLength64
 * 10/22/01 mel   Re-designed HMAC to make it non-blocking. But it will remain
 *                blocking for HMAC or message longer than 64.
 * 10/19/01 hml   Fixed merge issues.
 * 10/16/01 mel   Made computeSSLHandshakeHash non-bloking.
 * 10/16/01 spm   IKE APIs: removed key physical addr parms in cb calls.
 * 10/15/01 spm   IKE APIs: removed unnecessary IKE result handlers.
 *                msg_length is now compared to N8_MAX_HASH_LENGTH as 
 *                NOT N8_MAX_KEY_LENGTH.  Miscellaneous optimiations/
 *                simplifications brought up in the IKE API code review.
 *                Removed unnecessary memsets.  User objects are added to
 *                free list before kernel objects are mallocated to avoid
 *                kernel malloc leaks.  Changed if-else on alg to switch
 *                on alg.  Removed virtual pointer to msg in cb calls.
 *                Had to keep the virtual pointer to key, since the
 *                key has to be copied into the cb itself.  Added
 *                N8_64BYTE_IKE_KEY_LIMIT.
 * 10/15/01 bac   Corrected problems with N8_HandshakeHashPartial due to
 *                incorrectly handling computeResidual when the hashing length
 *                was zero.
 * 10/15/01 mel   Redesigned computeSSLhandshake.
 * 10/11/01 hml   Use n8_strdup instead of strdup in getRoleString().
 * 10/11/01 hml   Added use of RESULT_HANDLER_WARNING for the 
 *                N8_HandshakeHashEnd result handler.
 * 10/10/01 hml   Added N8_HashCompleteMessage and added support for the
 *                TLS modes to N8_HandshakeHashEnd.
 * 10/10/01 brr   Free resources in N8_HashEnd if error encountered.
 * 10/02/01 bac   Added use of RESULT_HANDLER_WARNING in all result handlers.
 * 10/01/01 hml   Updated rest of hash interfaces for multiple chips.
 * 09/26/01 hml   Updated to support multiple chips.  The N8_HashInitialize,
 *                N8_TLSKeyMaterialHash and N8_SSLKeyMaterialHash were changed.
 * 09/20/01 bac   The interface to the command block generators changed and now
 *                accept the command block buffer.  All calls to cb_ea methods
 *                changed herein.
 * 09/18/01 bac   Updated calls to createEARequestBuffer to pass the number of
 *                commands to be allocated.
 * 09/17/01 spm   Truncated lines >80 chars.
 * 09/12/01 mel   Deleted N8_UNIMPLEMENTED_FUNCTION in N8_HashPartial for HMAC.
 * 09/10/01 spm   Fixed memory leak in N8_IKEEncryptKeyExpand (0 byte not
 *                allocated)
 * 09/08/01 spm   IKE APIs: Swapped order of alg and hashInfo args.
 * 09/07/01 spm   Added IKE APIs
 * 09/07/01 bac   Fixed bug where we weren't checking the return code from a
 *                call to N8_HashPartial in N8_HandshakeHashPartial.
 *                (BUG #159)
 * 09/07/01 bac   Fixed a comment in N8_HashPartial which had the wrong size
 *                limit for data sent to that method.  (BUG #60)
 * 09/06/01 bac   Added include of <string.h> to silence warning.
 * 08/24/01 bac   Removed DBG prints of unterminated buffers.
 * 08/14/01 bac   Fixed memcpy bugs in computeHMAC where the wrong number of
 *                bytes were being moved.  Also determined HMAC hashes are not
 *                implemented correctly and have disallowed the use of
 *                N8_HashPartial and N8_HashEnd with HMAC algorithms.  This
 *                partially fixes Bug #160.
 * 08/01/01 bac   Changed MIN to N8_MIN to avoid compilation warnings.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/20/01 bac   Changed calls to create__RequestBuffer to pass the chip id.
 * 07/09/01 bac   Bug #115 and Bug #116 -- fixed N8_HandshakeHashPartial and
 *                N8_HandshakeHashEnd to use the user-supplied event when
 *                making the final API call.  All other API calls made by
 *                these methods remain synchronous but the final one is
 *                non-blocking and correctly indicates the status of
 *                the original call.  This is not the final solution.
 * 07/02/01 mel   Fixed comments.
 * 06/28/01 bac   Added HANDLE_EVENT calls as necessary.
 * 06/28/01 bac   Removed calls to 'freeRequest' in the result handlers as
 *                that belongs in the EventCheck routine.
 * 06/27/01 mel     Added init of hash object in computeHMAC between pad0 and
 *                pad 1 compute
 * 06/25/01 mel   In N8_TLSKeyMaterialHash added check :
 *                if the sum of KeyLength, LabelLength, and SeedLength is >
 *                18 Kbytes, return ERROR.
 * 06/25/01 bac   More changes for QMgr v.1.0.1
 * 06/19/01 bac   Correct use of kernel memory.
 * 06/18/01 mel   Added N8_TLSKeyMaterialHash API.
 * 06/17/01 bac   Free request upon failure in N8_SSLKeyMaterialHash
 * 05/30/01 bac   Doxygenation.
 * 05/30/01 bac   Test for hashingLength==0 in HashEnd to avoid the suspicion
 *                of memory alloc problems (malloc(0)).
 * 05/24/01 dws   Fixed the pointer arithmetic in the copy of the 
 *                N8_HashPartial result to the N8_HashObject_t iv field in 
 *                ResultHandlerHashPartial.
 * 05/19/01 bac   Corrected the freeing of postProcessingData.  Since the
 *                data passed in this pointer may be user data or local data
 *                sometimes it is appropriate to free it other times it isn't.
 *                Thus, the decision needs to live in the resultHandler, not
 *                the freeRequest utility.
 * 05/19/01 bac   Fixed memory leak.
 * 05/18/01 mel   Fix bug with wrong size memory allocation.
 * 05/18/01 bac   Converted to N8_xMALLOC and N8_xFREE
 * 05/15/01 bac   Fixed a bug where pointers were not initialized to NULL but
 *                were freed under error conditions.
 * 05/14/01 bac   Fixed the copy from results_p to IV to copy the right
 *                amount whether MD5 or SHA-1.
 * 05/09/01 dws   Changed the way that the result bytes are loaded into the
 *                hash object IV in N8_HashPartial. It now uses a series of 
 *                BE_to_uint32 operations instead of memcpy. 
 * 04/30/01 dkm   Modified N8_HashPartial so that if message is too small
 *                to start a hash, the message is appended onto the
 *                residual and residualLength is updated.
 * 04/30/01 bac   changed printResult to printN8Buffer
 * 04/26/01 bac   Completion of handshake hash functions.
 * 04/24/01 bac   Removed printResult definition.  It is now
 *                printN8Buffer in n8_util.
 * 04/09/01 bac   Added functionality for N8_HashInitialize
 * 03/29/01 bac   Original version.
 ****************************************************************************/
/** @defgroup n8_hash Hashing Functions
 */

#include "n8_pub_errors.h"
#include "n8_hash.h"
#include "n8_ea_common.h"
#include "n8_cb_ea.h"
#include "n8_enqueue_common.h"
#include "n8_util.h"
#include "n8_API_Initialize.h"
#include "n8_precompute.h"
#include "n8_OS_intf.h"
#include <opencrypto/cryptodev.h>
/*
 * Defines for values and macros
 */
#define MD5_PAD_LENGTH  (48)
#define SHA1_PAD_LENGTH (40)

/*
 * Prototypes of local static functions
 */
static N8_Status_t 
computeTLSHandshakeHash(N8_HashObject_t         *md5Obj_p,
                        N8_HashObject_t         *sha1Obj_p,
                        const N8_HashProtocol_t  protocol,
                        const char              *roleString_p,
                        const N8_Buffer_t       *key_p,
                        const unsigned int       keyLength,
                        N8_Buffer_t             *md5Result_p,
                        N8_Buffer_t             *sha1Result_p,
                        N8_Event_t              *event_p);

static char *getRoleString(const N8_HashProtocol_t protocol,
                           const N8_HashRole_t role);
static N8_Status_t  computeSSLHandshakeHash(N8_HashObject_t *hObjMD5_p,
                                            N8_HashObject_t *hObjSHA_p,
                                            const char *roleString_p,
                                            const N8_Buffer_t *key_p,
                                            const unsigned int keyLength,
                                            N8_Buffer_t *resultMD5_p,
                                            N8_Buffer_t *resultSHA_p,
                                            N8_Event_t  *event_p);
static N8_Status_t computeResidual(N8_HashObject_t   *obj_p,
                                   const N8_Buffer_t *msg_p,
                                   N8_Buffer_t       *hashMsg_p,
                                   const unsigned int msgLength,
                                   unsigned int      *hashingLength);

N8_Status_t n8_initializeHMAC(N8_Buffer_t             *HMACKey, 
                              uint32_t                 HMACKeyLength, 
                              N8_HashObject_t         *hashObj_p,
                              N8_Event_t              *event_p);
N8_Status_t n8_HMACHashEnd_req(N8_HashObject_t *obj_p,
                               N8_Buffer_t *result_p,
                               API_Request_t **req_pp);
/*
 * local structure definitions
 */
typedef struct 
{
   N8_Buffer_t *result_p;
   N8_Buffer_t *dest_p;
   int length;
} N8_SSLKeyHashResults_t;

typedef struct 
{
   int          outputLength;
   N8_Buffer_t *result_p;
   N8_Buffer_t *prS_p[2];
} N8_TLSKeyHashResults_t;

typedef struct
{
    uint32_t    outputLength;
    N8_Buffer_t *result_p;   /* this buffer is 3*outputLength bytes long */
    N8_Buffer_t *SKEYIDd_p;
    N8_Buffer_t *SKEYIDa_p;
    N8_Buffer_t *SKEYIDe_p;
} N8_IKESKEYIDExpandResults_t;

typedef struct 
{
   N8_Buffer_t *MD5_results_p;
   N8_HashObject_t *MD5_obj_p;
   N8_Buffer_t *SHA_results_p;
   N8_HashObject_t *SHA_obj_p;

} N8_HandshakePartialResults_t;

typedef struct
{
    N8_HashProtocol_t  protocol;
    N8_Buffer_t       *MD5Result_p;
    N8_Buffer_t       *SHA1Result_p;
    N8_Buffer_t       *resMD5_p;
    N8_Buffer_t       *resSHA1_p;
    N8_Buffer_t       *resMD5PRF_p;
    N8_Buffer_t       *resSHA1PRF_p;
} N8_TLSHandshakeHashResults_t;
typedef struct 
{
   N8_Buffer_t *to_resultSHA_p;
   N8_Buffer_t *from_end_resultSHA_p;
   N8_Buffer_t *to_objSHA_p;
   N8_Buffer_t *from_ivSHA_p;
   int length_SHA;
   N8_Buffer_t *to_resultMD5_p;
   N8_Buffer_t *from_end_resultMD5_p;
   N8_Buffer_t *to_objMD5_p;
   N8_Buffer_t *from_ivMD5_p;
   int length_MD5;
} N8_HandshakeEndResults_t;

typedef struct 
{
   N8_Buffer_t *result1_p;
   N8_Buffer_t *result2_p;
} N8_HMACResults_t;


/*
 * Static Methods
 */
static void n8_setHashedHMACKey(N8_HashObject_t *obj_p,
                                N8_Buffer_t *key_p,
                                unsigned int keyLength)
{
   N8_Buffer_t *tmp_p;
   int i;
   int iterations = CEIL(keyLength, sizeof(uint32_t));
   tmp_p = (N8_Buffer_t *) obj_p->hashedHMACKey;
   /* copy the key to the hashedHMACKey */
   memcpy(tmp_p, key_p, keyLength);
   /* pad the remainder with zeroes */
   memset(&tmp_p[keyLength], 0x0, sizeof(obj_p->hashedHMACKey)-keyLength);
   /* now, byte-swap the key in place */
   for (i = 0; i < iterations; i++) 
   {
      obj_p->hashedHMACKey[i] = BE_to_uint32(tmp_p);
      tmp_p += 4;
   }
} /* n8_setHashedHMACKey */

/**********************************************************************
 * resultHandlerHashPartial
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
 *
 **********************************************************************/
static void resultHandlerHashPartial(API_Request_t* req_p)
{
   N8_Buffer_t *iv_p;
   N8_HashObject_t *obj_p;
   int i;
   const char *title = "resultHandlerHashPartial";
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      /* the partial hash has been pre-computed.  the results must now
       * be copied back into the hash object */
      obj_p = (N8_HashObject_t *) req_p->copyBackTo_p;
      iv_p = req_p->copyBackFrom_p;
#ifdef N8DEBUG
      printf("result(%d bytes): ", obj_p->hashSize);
      for (i=0; i<obj_p->hashSize / sizeof(uint32_t); i+=4) {
	      printf("%08x ", *(uint32_t *)(&iv_p[i]));
      }
      printf("\n");
#endif

      for (i = 0; i < obj_p->hashSize / sizeof(uint32_t); i++) 
      {
         obj_p->iv[i] = BE_to_uint32(iv_p);
         iv_p += 4;
      }
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }
   /* freeRequest(req_p); */
} /* resultHandlerHashPartial */


/**********************************************************************
 * resultHandlerTLSKeyHash
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
 *
 **********************************************************************/
static void resultHandlerTLSKeyHash(API_Request_t* req_p)
{
   int i;
   N8_TLSKeyHashResults_t *keyResults_p;
   const char *title = "resultHandlerTLSKeyHash";
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      keyResults_p =
           (N8_TLSKeyHashResults_t* ) req_p->postProcessingData_p;
      /* produce the key material by XOR-ing pseudorandom streams */
      for (i = 0; i < keyResults_p->outputLength; i++)
      {
         /* when bug in chip is fixed, don't forget to change index to i+16
          * and i+20 */ 
         keyResults_p->result_p[i] = (keyResults_p->prS_p[0][i] ^
                                      keyResults_p->prS_p[1][i]);
      }
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }
} /* resultHandlerTLSKeyHash */

/*****************************************************************************
 * resultHandlerIKESKEYIDExpand
 *****************************************************************************/
/** @ingroup n8_hash
 * @brief Handles result for N8_IKESKEYIDExpand
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
 * @param req_p RW: pointer to API request structure
 *
 * @par Externals:
 *
 * @return 
 *    void
 *
 * @par Errors:
 *    None.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
static void resultHandlerIKESKEYIDExpand(API_Request_t* req_p)
{
   N8_IKESKEYIDExpandResults_t *keyResults_p;
   const char *title = "resultHandlerIKESKEYIDExpand";

   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      keyResults_p =
         (N8_IKESKEYIDExpandResults_t *) req_p->postProcessingData_p;

      memcpy(keyResults_p->SKEYIDd_p, keyResults_p->result_p,
             keyResults_p->outputLength);
      memcpy(keyResults_p->SKEYIDa_p,
             keyResults_p->result_p+keyResults_p->outputLength,
             keyResults_p->outputLength);
      memcpy(keyResults_p->SKEYIDe_p,
             keyResults_p->result_p+2*keyResults_p->outputLength,
             keyResults_p->outputLength);
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }

} /* resultHandlerIKESKEYIDExpand */

/*****************************************************************************
 * resultHandlerTLSHandshakeHash
 *****************************************************************************/
/** @ingroup n8_hash
 * @Handles the result for the N8_TLS_FINISH and N8_TLS_CERT modes in 
 *  N8_HandshakeHashEnd.
 *
 * This function is called by the EA Request Queue handler when
 * either the request is completed successfully and all the commands
 * copied back to the command blocks allocated by the API, or when
 * the hardware has encountered an error with one of the commands such 
 * that the hardware has locked up. Note that the function is finishing the
 * HandshakeHashEnd protocol according to section 5 of RFC 2246.
 *
 * @param req_p RW: pointer to API request structure.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    Void.
 *
 * @par Errors:
 *    None.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/

static void resultHandlerTLSHandshakeHash(API_Request_t* req_p)
{
   N8_TLSHandshakeHashResults_t *data_p = NULL;
   int                           i;
   unsigned int                  dataBuffer[NUM_WORDS_TLS_RESULT]; 
   unsigned int                 *ptr1;
   unsigned int                 *ptr2;
   const char                         *title = "resultHandlerTLSHandshakeHash";


   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("resultHandlerTLSHandshakeHash call-back with success\n"));
      data_p = (N8_TLSHandshakeHashResults_t *) req_p->postProcessingData_p;

      if (data_p->protocol == N8_TLS_CERT)
      {
         memcpy(data_p->MD5Result_p, 
                data_p->resMD5_p, 
                MD5_HASH_RESULT_LENGTH);

         memcpy(data_p->SHA1Result_p, 
                data_p->resSHA1_p, 
                SHA1_HASH_RESULT_LENGTH);
      }

      else
      {
         /* This can only be the N8_TLS_FINISH case.  This algorithm is 
            defined in section 5 of RFC 2246 */
         /* We need to ignore the first sample from each of the MD5 and
            SHA1 results */
         ptr1 = 
            (unsigned int *) (data_p->resMD5PRF_p + MD5_HASH_RESULT_LENGTH); 
         ptr2 = 
            (unsigned int *) (data_p->resSHA1PRF_p + SHA1_HASH_RESULT_LENGTH); 

         /* Now we Xor 12 bytes worth of data. Read the RFC ... */
         for (i = 0; i < NUM_WORDS_TLS_RESULT; i++)
         {
            dataBuffer[i] = (*ptr1) ^ (*ptr2);
            ptr1 ++;
            ptr2 ++;
         }

         memcpy(data_p->MD5Result_p, dataBuffer, MD5_HASH_RESULT_LENGTH);
      }
   }

   else
   {
      /* Unknown error */
      RESULT_HANDLER_WARNING(title, req_p);
   }

   N8_UFREE(req_p->postProcessingData_p);
   /* freeRequest(req_p); */

} /* resultHandlerTLSHandshakeHash */


/*****************************************************************************
 * resultHandlerHandshakePartial
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Handles the result for the HandshakePartial 
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
 *
 **********************************************************************/
static void resultHandlerHandshakePartial(API_Request_t* req_p)
{
   const char *title = "resultHandlerHandshakePartial";

   N8_Buffer_t *md5_res_p;
   N8_Buffer_t *sha_res_p;
   N8_HashObject_t *md5_obj_p;
   N8_HashObject_t *sha_obj_p;
   N8_HandshakePartialResults_t   *callBackData_p; 
   
   int i;
   
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      /* the partial hash has been pre-computed.  the results must now
       * be copied back into the hash object */

      callBackData_p = (N8_HandshakePartialResults_t *)
         req_p->postProcessingData_p; 
      /* ensure the call back data is not null */
      if (callBackData_p == NULL)
      {
         DBG(("ERROR:  %s call-back with NULL callBackData_p.", title));
      }
      else
      {
         md5_obj_p = callBackData_p->MD5_obj_p;
         md5_res_p = callBackData_p->MD5_results_p;
         sha_obj_p = callBackData_p->SHA_obj_p;
         sha_res_p = callBackData_p->SHA_results_p;

         for (i = 0; i < md5_obj_p->hashSize / sizeof(uint32_t); i++) 
         {
            md5_obj_p->iv[i] = BE_to_uint32(md5_res_p);
            md5_res_p += 4;
         }
         for (i = 0; i < sha_obj_p->hashSize / sizeof(uint32_t); i++) 
         {
            sha_obj_p->iv[i] = BE_to_uint32(sha_res_p);
            sha_res_p += 4;
         }
      }
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }
   /* DO NOT free request (req_p) here */
} /* resultHandlerHandshakePartial */

/**********************************************************************
 * resultHandlerHandshakeEnd
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
 *
 **********************************************************************/
static void resultHandlerHandshakeEnd(API_Request_t* req_p)
{
   char title[] = "resultHandlerHandshakeEnd";
   N8_Buffer_t *endresultmd5_p;
   N8_Buffer_t *endresultsha_p;
   N8_Buffer_t *resultmd5_p;
   N8_Buffer_t *resultsha_p;
   N8_HashObject_t *objmd5_p;
   N8_HashObject_t *objsha_p;
   N8_HandshakeEndResults_t   *callBackData_p 
                      = (N8_HandshakeEndResults_t *) req_p->postProcessingData_p;
   
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK )
   {
      DBG(("%s call-back with success\n", title));
      /* the partial hash has been pre-computed.  the results must now
       * be copied back into the hash object */
      resultsha_p = callBackData_p->to_resultSHA_p;
      endresultsha_p = callBackData_p->from_end_resultSHA_p;
      objsha_p = (N8_HashObject_t *) callBackData_p->to_objSHA_p;
      resultmd5_p = callBackData_p->to_resultMD5_p;
      endresultmd5_p = callBackData_p->from_end_resultMD5_p;
      objmd5_p = (N8_HashObject_t *) callBackData_p->to_objMD5_p;

      memcpy(resultmd5_p, endresultmd5_p, objmd5_p->hashSize);
      memcpy(resultsha_p, endresultsha_p, objsha_p->hashSize);
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }
} /* resultHandlerHandshakeEnd */

/*****************************************************************************
 * computeResidual
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Finish compute residual data.
 *
 * @param obj_p      RW: Hash object to store intermediate results.
 * @param msg_p      RO: Pointer to the message to be hashed.
 * @param msgLength  RO: Length of message to be hashed.
 * @param event_p    RO: Pointer to event for async calls.  Null for
 *                       synchronous call.
 * @par Externals:
 *    None
 *
 * @return 
 *    Status code of type N8_Status.
 *
 * @par Errors:
 *    N8_STATUS_OK:           A-OK<br>
 *    N8_INVALID_INPUT_SIZE:  message passed was larger than allowed
 *                            limit of N8_MAX_HASH_LENGTH<br>
 * @par Assumptions:
 *    None
 ****************************************************************************/
static N8_Status_t computeResidual(N8_HashObject_t   *obj_p,
                                   const N8_Buffer_t *msg_p,
                                   N8_Buffer_t       *hashMsg_p,
                                   const unsigned int msgLength,
                                   unsigned int      *hashingLength)
{
   int totalLength;             /* length of message + residual */
   int nextResidualLength;
   int hashFromMsgLength;

   DBG(("computeResidual\n"));

   totalLength = obj_p->residualLength + msgLength;
   nextResidualLength = totalLength % N8_HASH_BLOCK_SIZE;
   *hashingLength = totalLength - nextResidualLength;
   hashFromMsgLength = *hashingLength - obj_p->residualLength;

   if (*hashingLength == 0) {
      /* residualLength + msgLength is less than N8_HASH_BLOCK_SIZE.
         Just append msg to residual. */
      memcpy(&(obj_p->residual[obj_p->residualLength]), msg_p, msgLength);
      obj_p->residualLength = nextResidualLength;
      DBG(("Just append msg to residual\n"));
      DBG(("hash length: %d\n", *hashingLength));
      DBG(("msg to hash: %s\n", hashMsg_p));
      DBG(("residual length: %d\n", nextResidualLength));
      DBG(("residual: %s\n", obj_p->residual));
      return N8_STATUS_OK;
   }

   /* prepend the residual, if any */
   if (obj_p->residualLength > 0)
   {
      memcpy(hashMsg_p, obj_p->residual, obj_p->residualLength);
   }
   /* now copy the hashable portion of the incoming message */
   if (hashFromMsgLength > 0)
   {
      memcpy(&(hashMsg_p[obj_p->residualLength]), msg_p,
             hashFromMsgLength);
   }
   /* finally, copy the new residual */
   memset(obj_p->residual, 0x0, N8_MAX_RESIDUAL_LEN);
   if (nextResidualLength > 0)
   {
      memcpy(obj_p->residual,
             &(msg_p[msgLength - nextResidualLength]),
             nextResidualLength);
   }
   obj_p->residualLength = nextResidualLength;
   n8_incrLength64(&obj_p->Nh, &obj_p->Nl, *hashingLength);
   DBG(("hash length: %d\n", *hashingLength));
   DBG(("residual length: %d\n", nextResidualLength));
   DBG(("computeResidual - OK\n"));
   return N8_STATUS_OK;
} /* computeResidual */


/*****************************************************************************
 * n8_initializeHMAC_req
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Generate the HMAC initialization request.
 *
 *  @param HMACKey             RW:  HMAC key
 *  @param HMACKeyLength       RW:  HMAC key length
 *  @param hashObj_p           RW:  object to be initialized
 *  @param result_p            RW:  optional pointer to results area.  Used for
 *                                  IPsec only.
 *  @param req_pp              RW:  Pointer to request pointer.
 *
 * @par Errors
 *    N8_INVALID_HASH - the specified hash is not valid
 *    N8_INVALID_OBJECT - object passed was null<br>
 *    N8_INVALID_KEY_SIZE - the value of HMACKeyLength is less than 0
 *                          and greater than 18 KBytes.
 *
 * @par Assumptions
 *    None
 ****************************************************************************/
N8_Status_t n8_initializeHMAC_req(N8_Buffer_t             *HMACKey, 
                                  uint32_t                 HMACKeyLength, 
                                  N8_HashObject_t         *hashObj_p,
                                  void                    *result_p,
                                  N8_Buffer_t            **ctx_pp,
                                  uint32_t                *ctxa_p,
                                  API_Request_t          **req_pp)
{
   API_Request_t      *headReq_p = NULL; /* head pointer to request list */
   N8_Status_t         ret = N8_STATUS_OK;

   DBG(("n8_initializeHMAC\n"));
   do
   {

      N8_Buffer_t         hashedKey[N8_HASH_BLOCK_SIZE];
      if (result_p == NULL)
      {
         ret = n8_precompute_tls_ipad_opad(hashObj_p->type,
                                           HMACKey,
                                           hashedKey,
                                           &HMACKeyLength,
                                           hashObj_p->ipadHMAC_iv,
                                           hashObj_p->opadHMAC_iv);
         memcpy(hashObj_p->iv, hashObj_p->ipadHMAC_iv, 20);
      }
      else
      {
         N8_CipherInfo_t  *obj_p = (N8_CipherInfo_t *) result_p;
         ret = n8_precompute_tls_ipad_opad(hashObj_p->type,
                                           HMACKey,
                                           hashedKey,
                                           &HMACKeyLength,
                                           obj_p->key.IPsecKeyDES.ipad,
                                           obj_p->key.IPsecKeyDES.opad);
      }
      CHECK_RETURN(ret);
      hashObj_p->opad_Nl = 64;
      hashObj_p->Nl = 64;
      n8_setHashedHMACKey(hashObj_p, hashedKey, HMACKeyLength);
   } while (FALSE);

   *req_pp = headReq_p;
   return ret;
} /* n8_initializeHMAC_req */ 

/*****************************************************************************
 * n8_initializeHMAC
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Compute HMAC for future hashing and store results in context memory
 * if needed.
 *
 *  @param HMACKey             RW:  HMAC key
 *  @param HMACKeyLength       RW:  HMAC key length
 *  @param hashObj_p           RW:  object to be initialized
 *  @param event_p             RW:  Pointer to asynchronous event structure.
 *
 * @par Errors
 *    N8_INVALID_HASH - the specified hash is not valid
 *    N8_INVALID_OBJECT - object passed was null<br>
 *    N8_INVALID_KEY_SIZE - the value of HMACKeyLength is less than 0
 *                          and greater than 18 KBytes.
 *
 * @par Assumptions
 *    None
 ****************************************************************************/
N8_Status_t n8_initializeHMAC(N8_Buffer_t             *HMACKey, 
                              uint32_t                 HMACKeyLength, 
                              N8_HashObject_t         *hashObj_p,
                              N8_Event_t              *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Status_t ret = N8_STATUS_OK;
   uint32_t ctx_a;              /* unused dummy value */
   N8_Buffer_t *ctx_p;          /* unused dummy value */
   DBG(("N8_HashPartial\n"));
   do
   {
      /* generate the request */
      ret = n8_initializeHMAC_req(HMACKey, HMACKeyLength,
                                  hashObj_p,
                                  NULL,
                                  &ctx_p,
                                  &ctx_a,
                                  &req_p);
      CHECK_RETURN(ret);
      if (req_p == NULL)
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   }
   while (FALSE);
   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* n8_initializeHMAC */
/*****************************************************************************
 * N8_HashInitialize
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Initialize a HashObject for future hashing.
 *
 * Hashing functions require a HashObject to maintain state across
 * calls.  Before use, the object must be initialized to set the
 * algorithm and algorith-specific initial conditions.
 *
 *  @param hashObj_p           RW:  object to be initialized
 *  @param alg                 RO:  algorithm to be used with this object
 *  @param hashInfo_p          RO:  pointer to hash info structure for HMAC key.
 * @par Externals
 *    None
 *
 * @return
 *    N8_Status_t
 *
 * @par Errors
 *    N8_INVALID_HASH - the specified hash is not valid
 *    N8_INVALID_OBJECT - object passed was null<br>
 *    N8_INVALID_OBJECT - The unit value specified was invalid<br>
 *    N8_INVALID_KEY_SIZE - the value of HMACKeyLength is less than 0 and
 *                          greater than 18 KBytes.
 *
 * @par Assumptions
 *    None
 ****************************************************************************/
N8_Status_t N8_HashInitialize(N8_HashObject_t          *hashObj_p,
                              const N8_HashAlgorithm_t  alg,
                              const N8_HashInfo_t      *hashInfo_p,
                              N8_Event_t               *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;

   DBG(("N8_HashInitialize\n"));
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(hashObj_p, ret);

      hashObj_p->unitID = hashInfo_p->unitID;
      ret = n8_setInitialIVs(hashObj_p, alg, hashObj_p->unitID);
      CHECK_RETURN(ret);
      switch (hashObj_p->type)
      {
         case N8_HMAC_MD5:
         case N8_HMAC_MD5_96:
         case N8_HMAC_SHA1:
         case N8_HMAC_SHA1_96:
            /* check the key length to make sure it is in range */
            if (hashInfo_p->keyLength > N8_MAX_HASH_LENGTH)
            {
               ret = N8_INVALID_KEY_SIZE;
               break;
            }
            CHECK_OBJECT(hashInfo_p, ret);
            ret = n8_initializeHMAC(hashInfo_p->key_p,
                                    hashInfo_p->keyLength,
                                    hashObj_p,
                                    event_p);
            break;
         default:
            break;
      }
   } while (FALSE);

   DBG(("N8_HashInitialize after loop\n"));
   if (ret == N8_STATUS_OK)
   {
      /* Set the structure ID */
      hashObj_p->structureID = N8_HASH_STRUCT_ID;
      if ((event_p != NULL) && 
          ((alg == N8_MD5) || (alg == N8_SHA1)))
      {
         N8_SET_EVENT_FINISHED(event_p, N8_EA);
      }
   }

   DBG(("N8_HashInitialize - OK\n"));
   return ret;
} /* N8_HashInitialize */
/*****************************************************************************
 * n8_HashPartial_req
 *****************************************************************************/
/** @ingroup n8_hash
 * @brief Generate a request for a N8_HashPartial.
 *
 * The generated request is returned but not queued.
 *
 *  @param obj_p               RW:  Pointer to hash object
 *  @param msg_p               RO:  Pointer to message
 *  @param msgLength           RO:  Message length in bytes
 *  @param req_pp              RW:  Pointer to request pointer
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t n8_HashPartial_req(N8_HashObject_t *obj_p,
                               const N8_Buffer_t *msg_p,
                               const unsigned int msgLength,
                               const n8_IVSrc_t ivSrc,
                               API_Request_t **req_pp)
{
   N8_Status_t           ret = N8_STATUS_OK;
   
   unsigned int          hashingLength;     /* totalLength rounded down to the
                                             * next multiple of  N8_HASH_BLOCK_SIZE */
   N8_Buffer_t          *hashMsg_p = NULL;
   unsigned long         hashMsg_a;
   uint32_t              result_a;
   N8_Buffer_t          *result_p = NULL;
   unsigned int          nBytes;

   do
   {
      CHECK_OBJECT(req_pp, ret);
      *req_pp = NULL;           /* set the return request to NULL in case we
                                 * have to bail out. */
      CHECK_OBJECT(obj_p, ret);
      CHECK_STRUCTURE(obj_p->structureID, N8_HASH_STRUCT_ID, ret);

      ret = N8_preamble();
      CHECK_RETURN(ret);

      if (msgLength == 0)
      {
         /* this is legal but has no effect.  do not change any state
          * and return successfully.
          */
         break;
      }

      /* only after verifying that the message length is non-zero, test for
       * valid msg_p */
      CHECK_OBJECT(msg_p, ret);

      if (msgLength > N8_MAX_HASH_LENGTH)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      CHECK_OBJECT(obj_p, ret);
      CHECK_OBJECT(msg_p, ret);

      /* create the EA request */
      nBytes = (NEXT_WORD_SIZE(obj_p->hashSize) + 
                NEXT_WORD_SIZE(msgLength + N8_MAX_RESIDUAL_LEN));
      ret = createEARequestBuffer(req_pp,
                                  obj_p->unitID,
                                  N8_CB_EA_HASHPARTIAL_NUMCMDS,
                                  resultHandlerHashPartial, nBytes);
      CHECK_RETURN(ret);

      /* allocate space for the chip to place the result */
      result_a = (*req_pp)->qr.physicalAddress + (*req_pp)->dataoffset;
      result_p = (N8_Buffer_t *) ((int)(*req_pp) + (*req_pp)->dataoffset);
      memset(result_p, 0, nBytes);

      hashMsg_a = result_a + NEXT_WORD_SIZE(obj_p->hashSize);
      hashMsg_p = result_p + NEXT_WORD_SIZE(obj_p->hashSize);

      /*
       * The actual input is concat(residual, *msg_p).  Of this, we
       * only hash multiple lengths of N8_HASH_BLOCK_SIZE and save the
       * rest for the next time around.
       *
       * We compute the residual and message first, since we have nothing to do
       * if the hashing length is zero.
       */
      ret = computeResidual(obj_p,
                            msg_p,
                            hashMsg_p,
                            msgLength,
                            &hashingLength);
      CHECK_RETURN(ret);
      /* if the hashing length is 0, then there is nothing to do for this call.
       * Set the event status to done (if asynchronous) and return.
       */
      if (hashingLength == 0)
      {
         freeRequest(*req_pp);
         *req_pp = NULL;
         ret = N8_STATUS_OK;
         break;
      }

      /* set up the copy back data for processing in the result handler */
      (*req_pp)->copyBackFrom_p = result_p;
      (*req_pp)->copyBackTo_p = (void *) obj_p;
      
      /* handle messages longer than 64 bytes for HMAC */
      ret = cb_ea_hashPartial(*req_pp,
                              (*req_pp)->EA_CommandBlock_ptr,
                              obj_p,
                              ivSrc,
                              hashMsg_a,
                              hashingLength,
                              result_a,
                              NULL /* next command block */,
			      N8_TRUE);
      CHECK_RETURN(ret);
   } while (FALSE);
   return ret;
} /* n8_HashPartial_req */

/*****************************************************************************
 * N8_HashPartial
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Hash a partial message.
 *
 * To hash a message over 18 K bytes long, multiple calls to this
 * function must be made processing chunks of the message.  After all
 * chunks are processed a call to the companion function N8_HashEnd
 *
 * @param obj_p      RW: Hash object to store intermediate results.
 * @param msg_p      RO: Pointer to the message to be hashed.
 * @param msgLength  RO: Length of message to be hashed.
 * @param event_p    RO: Pointer to event for async calls.  Null for
 *                       synchronous call.
 * @par Externals:
 *    None
 *
 * @return 
 *    Status code of type N8_Status.
 *
 * @par Errors:
 *    N8_STATUS_OK:           A-OK<br>
 *    N8_INVALID_INPUT_SIZE:  message passed was larger than allowed
 *                            limit of N8_MAX_HASH_LENGTH<br>
 * @par Assumptions:
 *    None
 ****************************************************************************/
N8_Status_t N8_HashPartial(N8_HashObject_t *obj_p,
                           const N8_Buffer_t *msg_p,
                           const unsigned int msgLength,
                           N8_Event_t *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Status_t ret = N8_STATUS_OK;
   
   DBG(("N8_HashPartial\n"));
   do
   {
      /* generate the request */
      ret = n8_HashPartial_req(obj_p, msg_p, msgLength, N8_IV, &req_p);
      CHECK_RETURN(ret);
      if (req_p == NULL)
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   }
   while (FALSE);
   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_HashPartial - OK\n"));
   return ret;
} /* N8_HashPartial */

/*****************************************************************************
 * n8_HMACHashEnd_req
 *****************************************************************************/
/** @ingroup n8_hash
 * @brief Generate request for N8_HashEnd for HMAC algorithms only.
 *
 * Recall a HMAC consists of the following:
 * HMAC = H(o + H(i + msg))
 * At the time the end is being called, hash partial for all of the msg has been
 * called and msg is completely consumed.  Some portion of it may be in the
 * residual.  The first step to ending the HMAC is to perform a hash end on the
 * inner portion.  The results of this is guaranteed to be small (16 or 20
 * bytes, depending upon the hash algorithm in use).  Also note that the
 * initial i and o values are exactly 64 bytes long, so they are hashed with no
 * residual.  So this reduces the complexity of finishing an HMAC hash to:
 * res1 = hashEnd(obj loaded with inner IV, residual)
 * res  = hashEnd(obj loaded with outer IV, res1)
 *
 *  @param obj_p               RW:  Pointer to hash object.
 *  @param result_p            RW:  Pointer to results buffer provided by
 *                                  original caller.
 *  @param req_pp              RW:  Pointer to request pointer.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_INVALID_OBJECT, N8_INVALID_HASH
 *
 * @par Assumptions
 *    The object algorithm has been pre-screened to be one of the supported HMAC
 * algorithms. 
 *****************************************************************************/
N8_Status_t n8_HMACHashEnd_req(N8_HashObject_t *obj_p,
                               N8_Buffer_t *result_p,
                               API_Request_t **req_pp)
{
   N8_Status_t        ret = N8_STATUS_OK;
   int                hashingLength;
   N8_Buffer_t       *hashMsg_p = NULL;
   uint32_t           hashMsg_a;
   N8_Buffer_t       *kResults_p = NULL;
   uint32_t           kResults_a;
   N8_Buffer_t       *tempResults_p = NULL;
   uint32_t           tempResults_a;
   API_Request_t     *req_p;
   EA_CMD_BLOCK_t    *nextCommandBlock_p = NULL;   
   int                nBytes;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_OBJECT(req_pp, ret);
      *req_pp = NULL;
      
      /* minimal sanity checking on the hash object */
      CHECK_OBJECT(obj_p, ret);
      CHECK_OBJECT(result_p, ret);
      CHECK_STRUCTURE(obj_p->structureID, N8_HASH_STRUCT_ID, ret);

      DBG(("residual length: %d\n", obj_p->residualLength));
      DBG(("residual: %s\n", obj_p->residual));

      hashingLength = obj_p->residualLength;

      /*
       * The input to the final hash is the residual in the hash object.
       */

      /* create the request */
      nBytes = NEXT_WORD_SIZE(obj_p->hashSize) + /* final results */
               NEXT_WORD_SIZE(obj_p->hashSize) + /* temporary results */
               NEXT_WORD_SIZE(hashingLength);    /* residual */
      ret = createEARequestBuffer(req_pp,
                                  obj_p->unitID,
                                  2 * N8_CB_EA_HASHEND_NUMCMDS,
                                  resultHandlerGeneric, nBytes);
      CHECK_RETURN(ret);
      
      req_p = *req_pp;

      kResults_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(kResults_p, 0, nBytes);
      kResults_a = req_p->qr.physicalAddress + req_p->dataoffset;
      tempResults_p = kResults_p + NEXT_WORD_SIZE(obj_p->hashSize);
      tempResults_a = kResults_a + NEXT_WORD_SIZE(obj_p->hashSize);
      hashMsg_p = tempResults_p + NEXT_WORD_SIZE(obj_p->hashSize);
      hashMsg_a = tempResults_a + NEXT_WORD_SIZE(obj_p->hashSize);
      
      if (hashingLength != 0)
      {
         /* copy the residual, which we know to be non-zero in length */
         memcpy(hashMsg_p, obj_p->residual, hashingLength);
         DBG(("hash length: %d\n", hashingLength));
      }
      else
      {
         hashMsg_p = NULL;
         hashMsg_a = 0;
         DBG(("hash length: %d\n", hashingLength));
         DBG(("msg to hash: none\n"));
      }

      /* first, do a hash on the residual */
      ret = cb_ea_hashEnd(req_p,                      /* request */
                          req_p->EA_CommandBlock_ptr, /* command block pointer */
                          obj_p,                      /* object pointer */
                          N8_IV,                      /* get the IV from the
                                                       * regular place */
                          hashMsg_a,                  /* message to hash */
                          hashingLength,              /* length of message */
                          tempResults_a,              /* results physical address */
                          &nextCommandBlock_p,        /* next command block pointer */
			  N8_FALSE);
      CHECK_RETURN(ret);

      /* next, create a command request to hash the results that were just
       * generated */
      ret = cb_ea_hashEnd(req_p,                      /* request */
                          nextCommandBlock_p,         /* command block pointer */
                          obj_p,                      /* object pointer */
                          N8_OPAD,                    /* get the IV from the opad */
                          tempResults_a,              /* message to hash */
                          obj_p->hashSize,            /* length of message */
                          kResults_a,                 /* results physical address */
                          NULL,                       /* next command block pointer */
			  N8_TRUE);
      CHECK_RETURN(ret);

      req_p->copyBackSize = obj_p->hashSize;
      req_p->copyBackFrom_p = kResults_p;
      req_p->copyBackTo_p = result_p;

   } while (FALSE);

   return ret;

} /* n8_HMACHashEnd_req */ 

/*****************************************************************************
 * n8_HashEnd_req
 *****************************************************************************/
/** @ingroup n8_hash
 * @brief Generate the request for a Hash End operation for non-HMAC
 * algorithms. 
 *
 *  @param obj_p               RW:  Pointer to hash object
 *  @param result_p            RW:  Results area
 *  @param req_pp              RW:  Pointer to request pointer
 *
 * @par Externals
 *    None
 *
 * @return
 *    <description of return value>
 *
 * @par Errors
 *    <description of possible errors><br>
 *
 * @par Assumptions
 *    <description of assumptions><br>
 *****************************************************************************/
N8_Status_t n8_HashEnd_req(N8_HashObject_t *obj_p,
                           N8_Buffer_t *result_p,
                           API_Request_t **req_pp)
{
   N8_Status_t      ret = N8_STATUS_OK;
   int              totalLength;
   int              hashingLength;
   N8_Buffer_t     *hashMsg_p = NULL;
   uint32_t         hashMsg_a;
   unsigned long    pAddr;
   char            *vAddr;
   N8_Buffer_t     *kResults_p = NULL;
   uint32_t         kResults_a;
   API_Request_t   *req_p;
   int              nBytes;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_OBJECT(req_pp, ret);
      *req_pp = NULL;
      
      /* minimal sanity checking on the hash object */
      CHECK_OBJECT(obj_p, ret);
      CHECK_OBJECT(result_p, ret);
      CHECK_STRUCTURE(obj_p->structureID, N8_HASH_STRUCT_ID, ret);

      DBG(("residual length: %d\n", obj_p->residualLength));
      DBG(("residual: %s\n", obj_p->residual));

      totalLength = obj_p->residualLength;
      hashingLength = totalLength;

      /*
       * The input to the final hash is the residual in the hash object.
       */
      /* allocate a kernel buffer for the results and for
       * the message to hash
       */

      /* The message to hash is ready.  Now, create the command
       * blocks and do the final hash.
       */
      nBytes = NEXT_WORD_SIZE(obj_p->hashSize) + NEXT_WORD_SIZE(hashingLength);
      ret = createEARequestBuffer(&req_p,
                                  obj_p->unitID,
                                  N8_CB_EA_HASHEND_NUMCMDS,
                                  resultHandlerGeneric, nBytes);
      CHECK_RETURN(ret);

      pAddr = req_p->qr.physicalAddress + req_p->dataoffset;
      vAddr = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(vAddr, 0, nBytes);

      kResults_p = vAddr;
      kResults_a = pAddr;
      vAddr += NEXT_WORD_SIZE(obj_p->hashSize);
      pAddr += NEXT_WORD_SIZE(obj_p->hashSize);
      
      

      if (hashingLength != 0)
      {
         hashMsg_p = (N8_Buffer_t *) vAddr;
         hashMsg_a = pAddr;
         /* copy the residual, which we know to be non-zero in length */
         memcpy(hashMsg_p, obj_p->residual, hashingLength);
         DBG(("hash length: %d\n", hashingLength));
      }
      else
      {
         /* even if the hashing length is zero, the hardware call must be made
          * to finalize the hash.  it will see no new data is presented for
          * hashing and only do the portions necessary to finish the hash in
          * progress. */
         hashMsg_p = NULL;
         hashMsg_a = 0;
         DBG(("hash length: %d\n", hashingLength));
         DBG(("msg to hash: none\n"));
      }

      CHECK_RETURN(ret);
      ret = cb_ea_hashEnd(req_p,
                          req_p->EA_CommandBlock_ptr,
                          obj_p, 
                          N8_IV,                      /* get the IV from the
                                                       * regular place */
                          hashMsg_a,
                          hashingLength,
                          kResults_a,
                          NULL, /* next command block */
                          N8_TRUE);
      CHECK_RETURN(ret);

      req_p->copyBackSize = obj_p->hashSize;
      req_p->copyBackFrom_p = kResults_p;
      req_p->copyBackTo_p = result_p;

      *req_pp = req_p;
   } while (FALSE);

   return ret;

} /* n8_HashEnd_req */
/*****************************************************************************
 * N8_HashEnd
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Finalize a hash in progress.
 *
 * After one or more calls to N8_HashPartial, after all portions are
 * hashed, N8_HashEnd is called to compute the final values and return
 * the hash results.
 *
 *  @param obj_p               RW:  Hash object to use
 *  @param result_p            RW:  Pointer to pre-allocated buffer to
 *                                  hold returned results
 *  @param event_p             RW:  Asynchronous event pointer
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status condition
 *
 * @par Errors
 *    N8_INVALID_OBJECT<br>
 *    N8_MALLOC_FAILED<br>
 *
 * @par Assumptions
 *    Asynchronous calls not implemented.
 ****************************************************************************/
N8_Status_t N8_HashEnd(N8_HashObject_t *obj_p,
                       N8_Buffer_t *result_p,
                       N8_Event_t *event_p)
{
   API_Request_t   *req_p = NULL;
   N8_Status_t      ret = N8_STATUS_OK;

   DBG(("N8_HashEnd\n"));
   do
   {
      switch (obj_p->type)
      {
         case N8_HMAC_MD5:
         case N8_HMAC_MD5_96:
         case N8_HMAC_SHA1:
         case N8_HMAC_SHA1_96:
            ret = n8_HMACHashEnd_req(obj_p, result_p, &req_p);
            break;
         case N8_MD5:
         case N8_SHA1:
            ret = n8_HashEnd_req(obj_p, result_p, &req_p);
            break;
         default:
            ret = N8_INVALID_HASH;
            break;
      } 
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   
   DBG(("N8_HashEnd - OK\n"));
   return ret;
} /* N8_HashEnd */

/*****************************************************************************
 * N8_HashClone
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Given a HashObject produce a clone.
 *
 * Perform a clone of a given HashObject and return the clone.
 *
 *  @param orig_p              RO:  Hash object to be cloned.
 *  @param clone_p             RW:  Cloned version of hash object.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error condition if raised.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    Values in the original are unchecked.  Only a copy is
 *    performed.<br>
 *    Clone_p points to a pre-allocated buffer of sufficient size.
 ****************************************************************************/
N8_Status_t N8_HashClone(const N8_HashObject_t *orig_p,
                         N8_HashObject_t *clone_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(orig_p, ret);
      CHECK_OBJECT(clone_p, ret);
      CHECK_STRUCTURE(orig_p->structureID, N8_HASH_STRUCT_ID, ret);
      memcpy(clone_p, orig_p, sizeof(N8_HashObject_t));
   } while (FALSE);
   return ret;
} /* N8_HashClone */

/*****************************************************************************
 * N8_HandshakeHashPartial
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Computes partial handshake hash.  
 *
 * The SSL and TLS handshake protocol requires the computation of a
 * hash.  A unique message is hashed and that is then hashed with
 * other known quantities.  This function is called repeatedly until
 * all of the parts of the unique message are hashed.  A subsequent
 * call to the companion functions N8_HandshakeHashEnd wraps up the
 * handshake. 
 *
 *  @param md5Obj_p            RW:  Pointer to MD5 hash object.
 *  @param shaObj_p            RW:  Pointer to SHA-1 hash object.
 *  @param msg_p               RW:  Pointer to the message portion to
 *                                  be hashed
 *  @param msgLength           RW:  Length of message portion to be hashed.
 *  @param event_p             RW:  Asynchronous event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error code if encountered.
 *
 * @par Errors
 *    N8_INVALID_HASH
 *    N8_INVALID_OBJECT
 *    N8_INVALID_INPUT_SIZE
 *    N8_MALLOC_FAILED
 *
 * @par Assumptions
 *    None
 ****************************************************************************/
N8_Status_t N8_HandshakeHashPartial(N8_HashObject_t *md5Obj_p,
                                    N8_HashObject_t *shaObj_p,
                                    const N8_Buffer_t *msg_p,
                                    const unsigned int msgLength,
                                    N8_Event_t *event_p)
{
   N8_Status_t  ret = N8_STATUS_OK;
   unsigned int hashingLength;

   N8_Buffer_t *hashMsg_p = NULL;
   uint32_t     hashMsg_a;
   uint32_t     resultMD5_a;
   N8_Buffer_t *resultMD5_p = NULL;
   uint32_t     resultSHA_a;
   N8_Buffer_t *resultSHA_p = NULL;
   API_Request_t *req_p = NULL;
   int nBytes;
   EA_CMD_BLOCK_t      *nextCommandBlock = NULL;   
   N8_HandshakePartialResults_t  *callBackData_p = NULL;
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      if (msgLength == 0)
      {
         /* legal but has no effect or side-effects */
         break;
      }
      if (msgLength > N8_MAX_HASH_LENGTH)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      CHECK_OBJECT(md5Obj_p, ret);
      CHECK_OBJECT(shaObj_p, ret);
      CHECK_OBJECT(msg_p, ret);
      CHECK_STRUCTURE(md5Obj_p->structureID, N8_HASH_STRUCT_ID, ret);
      CHECK_STRUCTURE(shaObj_p->structureID, N8_HASH_STRUCT_ID, ret);

      /* ensure the hash objects were initialized correctly */
      if (md5Obj_p->type != N8_MD5 || shaObj_p->type != N8_SHA1)
      {
         ret = N8_INVALID_HASH;
         break;
      }

      nBytes = (NEXT_WORD_SIZE(md5Obj_p->hashSize) +
                NEXT_WORD_SIZE(shaObj_p->hashSize) +
                NEXT_WORD_SIZE(msgLength + N8_MAX_RESIDUAL_LEN));
      /* In order to perform the HandshakeHashPartial, the equivalent of a hash
       * partial needs to be done for the MD5 object and the SHA-1 object.  In
       * order to perform this in one request, the command blocks are generated
       * and submitted as a single request to the queue manager. */
      ret = createEARequestBuffer(&req_p,
                                  md5Obj_p->unitID,
                                  2 * N8_CB_EA_HASHPARTIAL_NUMCMDS,
                                  resultHandlerHandshakePartial, nBytes);
      CHECK_RETURN(ret);

      /* setup the space for the chip to place the result */
      resultMD5_a = req_p->qr.physicalAddress + req_p->dataoffset;
      resultMD5_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(resultMD5_p, 0, nBytes);

      resultSHA_a = resultMD5_a + NEXT_WORD_SIZE(md5Obj_p->hashSize);
      resultSHA_p = resultMD5_p + NEXT_WORD_SIZE(md5Obj_p->hashSize);

      hashMsg_a = resultSHA_a + NEXT_WORD_SIZE(shaObj_p->hashSize);
      hashMsg_p = resultSHA_p + NEXT_WORD_SIZE(shaObj_p->hashSize);

      /*
       * The actual input is concat(residual, *msg_p).  Of this, we
       * only hash multiple lengths of N8_HASH_BLOCK_SIZE and save the
       * rest for the next time around.
       * Note that the MD5 and SHA-1 messages and residuals will always be the
       * same.  Therefore, we can calculate the hashing message and residual once
       * and share between the hash objects.
       */
      ret = computeResidual(md5Obj_p,
                            msg_p,
                            hashMsg_p,
                            msgLength,
                            &hashingLength);

      CHECK_RETURN(ret);

      /* there is hashing to be done.  copy the residual from the MD5 object to
       * the SHA object. */
      memcpy(shaObj_p->residual, md5Obj_p->residual, md5Obj_p->residualLength);
      shaObj_p->residualLength = md5Obj_p->residualLength;
      n8_incrLength64(&shaObj_p->Nh, &shaObj_p->Nl, hashingLength);

      /* if the hashing length is 0, then there is nothing to do for this call.
       * Set the event status to done (if asynchronous) and return.
       */
      if (hashingLength == 0)
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         freeRequest(req_p);
         ret = N8_STATUS_OK;
         break;
      }

      
      /* The message to hash this time around has been isolated.  The
       * residual is in place for next time.  Now, create the command
       * blocks and do the hash.
       */

      /* create callback structure */
      callBackData_p = (N8_HandshakePartialResults_t *)&req_p->postProcessBuffer;
      callBackData_p->SHA_results_p = resultSHA_p;
      callBackData_p->SHA_obj_p = shaObj_p;
      callBackData_p->MD5_results_p = resultMD5_p;
      callBackData_p->MD5_obj_p = md5Obj_p;
      req_p->postProcessingData_p = (void *) callBackData_p;

      ret = cb_ea_hashPartial(req_p,
                              req_p->EA_CommandBlock_ptr,
                              md5Obj_p,
                              N8_IV,
                              hashMsg_a,
                              hashingLength,
                              resultMD5_a,
                              &nextCommandBlock,
			      N8_FALSE);
      CHECK_RETURN(ret);
      ret = cb_ea_hashPartial(req_p,
                              nextCommandBlock,
                              shaObj_p, 
                              N8_IV,
                              hashMsg_a,
                              hashingLength,
                              resultSHA_a,
                              &nextCommandBlock,
			      N8_TRUE);
      CHECK_RETURN(ret);
      
      DBG(("command block:\n"));

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   }
   while (FALSE);

   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* N8_HandshakeHashPartial */

/*****************************************************************************
 * N8_HandshakeHashEnd
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Finishes ongoing handshake hash computation.
 *
 * After all portions of the message have been processed by
 * N8_HandshakeHashPartial this method must be called to do the final
 * processing.  The function produces a hash result for MD5 and SHA-1
 * simultaneously. 
 *
 *  @param md5Obj_p            RW:  MD5 hash object
 *  @param sha1Obj_p           RW:  SHA-1 hash object
 *  @param protocol            RW:  Hash protocol specifier (TLS
 *                                  finish, TLS cert, SSL finish, or
 *                                  SSL cert)
 *  @param key_p               RW:  Hash key
 *  @param keyLength           RW:  Length of hash key
 *  @param role                RW:  Role of hash (server or client)
 *  @param md5Result_p         RW:  Pointer to pre-allocated space for
 *                                  MD5 results.
 *  @param sha1Result_p        RW:  Pointer to pre-allocated space for
 *                                  SHA-1 results.
 *  @param event_p             RW:  Async event.  NULL for blocking.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error condition if met.
 *
 * @par Errors
 *    N8_MALLOC_FAILED<br>
 *    N8_INVALID_HASH<br>
 *    N8_INVALID_PROTOCOL
 *
 * @par Assumptions
 *    TLS functions not implemented.
 ****************************************************************************/
N8_Status_t N8_HandshakeHashEnd(N8_HashObject_t *md5Obj_p,
                                N8_HashObject_t *sha1Obj_p,
                                const N8_HashProtocol_t protocol,
                                const N8_Buffer_t *key_p,
                                const unsigned int keyLength,
                                const N8_HashRole_t role,
                                N8_Buffer_t *md5Result_p,
                                N8_Buffer_t *sha1Result_p,
                                N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   char *roleString_p = NULL;
   
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      CHECK_STRUCTURE(md5Obj_p->structureID, N8_HASH_STRUCT_ID, ret);
      CHECK_STRUCTURE(sha1Obj_p->structureID, N8_HASH_STRUCT_ID, ret);

      if (md5Obj_p == NULL || sha1Obj_p == NULL)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      if (md5Obj_p->type != N8_MD5 || sha1Obj_p->type != N8_SHA1)
      {
         ret = N8_INVALID_HASH;
         break;
      }
      roleString_p = getRoleString(protocol, role);
      
      switch (protocol)
      {
         case N8_TLS_FINISH:
         case N8_TLS_CERT:
            ret = computeTLSHandshakeHash(md5Obj_p,
                                          sha1Obj_p,
                                          protocol,
                                          roleString_p,
                                          key_p, 
                                          keyLength,
                                          md5Result_p,
                                          sha1Result_p,
                                          event_p);
            break;
         case N8_SSL_FINISH:
         case N8_SSL_CERT:
            ret = computeSSLHandshakeHash(md5Obj_p,
                                          sha1Obj_p,
                                          roleString_p,
                                          key_p, keyLength,
                                          md5Result_p,
                                          sha1Result_p,
                                          event_p);
            break;
         default:
            ret = N8_INVALID_PROTOCOL;
            break;
      }
      CHECK_RETURN(ret);
   }
   while (FALSE);

   /* clean up */
   N8_UFREE(roleString_p);

   return ret;
} /* N8_HandshakeHashEnd */

/*****************************************************************************
 * N8_SSLKeyMaterialHash
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief <One line description of the function.>
 *
 * <More detailed description of the function including any unusual algorithms
 * or suprising details.>
 *
 *  @param obj_p               RO:  hash info pointer.
 *  @param random_p            RO:  64 bytes of random input.
 *  @param outputLength        RO:  Length of requested output.
 *  @param keyMaterial_p       RW:  Key material output.
 *  @param event_p             RW:  Asynch event.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error condition if raised.
 *
 * @par Errors
 *    N8_INVALID_KEY_SIZE - provided key is the wrong size<br>
 *    N8_INVALID_OUTPUT_SIZE - the requested output length is wrong<br>
 *    N8_INVALID_OBJECT - An input is NULL or the specified unit is invalid.
 *
 * @par Assumptions
 *    keyMaterial_p points to an allocated buffer of sufficient size.
 ****************************************************************************/
N8_Status_t N8_SSLKeyMaterialHash (N8_HashInfo_t    *obj_p,
                                  const N8_Buffer_t *random_p,
                                  const unsigned int outputLength,
                                  N8_Buffer_t       *keyMaterial_p,
                                  N8_Event_t        *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Buffer_t *result_p = NULL;
   unsigned long result_a;
   N8_Buffer_t *kKey_p = NULL;
   unsigned long kKey_a;
   N8_Status_t ret = N8_STATUS_OK;
   int         nBytes;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(obj_p, ret); 
      CHECK_OBJECT(obj_p->key_p, ret); 
      CHECK_OBJECT(random_p, ret); 

      /* check requested output size */
      if (outputLength > N8_MAX_SSL_KEY_MATERIAL_LENGTH)
      {
         ret = N8_INVALID_OUTPUT_SIZE;
         break;
      }
      /* check key length */
      if (obj_p->keyLength > N8_MAX_KEY_LENGTH)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      /* compute space for the chip to place the result */
      /* this needs to be larger than the next multiple of 16.  rather
       * than do the computation, just allocate it to the max size, which is
       * relatively small.
       */
      nBytes = NEXT_WORD_SIZE(N8_MAX_SSL_KEY_MATERIAL_LENGTH) +
               NEXT_WORD_SIZE(obj_p->keyLength);

      ret = createEARequestBuffer(&req_p,
                                  obj_p->unitID, 
                                  N8_CB_EA_SSLKEYMATERIALHASH_NUMCMDS,
                                  resultHandlerGeneric, nBytes);
                                  
      CHECK_RETURN(ret);

      result_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(result_p, 0, nBytes);
      result_a = req_p->qr.physicalAddress + req_p->dataoffset;

      /* set up the key in kernel memory and copy the incoming key over */
      kKey_p = result_p + NEXT_WORD_SIZE(N8_MAX_SSL_KEY_MATERIAL_LENGTH);
      kKey_a = result_a + NEXT_WORD_SIZE(N8_MAX_SSL_KEY_MATERIAL_LENGTH);

      memcpy(kKey_p, obj_p->key_p, obj_p->keyLength);

      req_p->copyBackTo_p = keyMaterial_p;
      req_p->copyBackFrom_p = result_p;
      req_p->copyBackSize = outputLength;

      ret = cb_ea_SSLKeyMaterialHash(req_p,
                                     req_p->EA_CommandBlock_ptr, 
                                     kKey_a,
                                     obj_p->keyLength,
                                     random_p,
                                     outputLength,
                                     result_a);
      CHECK_RETURN(ret);

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   }
   while (FALSE);
   /* clean up */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);              
   }
   return ret;
} /* N8_SSLKeyMaterialHash */

/*****************************************************************************
 * N8_TLSKeyMaterialHash
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief This computes the TLS Pseudo Random Function (PRF) of the specified
 * inputs as described in Section 5 of the TLS Specification [TLS].
 *
 * The PRF is used to generate various keying material from a secret key, a
 * label, and a seed.  The values of each of these depends on what's being
 * generated.  The required key is specified by Key and its length by
 * KeyLength.
 * The Label is typically an ascii string literal (such as "key expansion" or
 * "master secret"), as specified by [TLS]; its length is given in
 * LabelLength.
 * The Seed is usually the client generated random value concatenated with the
 * server generated random value (or vice-versa). It is specified in the Seed
 * parameter and its length in bytes is specified by the SeedLength parameter.
 * Using these values this routine then generates OutputLength bytes of key
 * material, according to the TLS Protocol, and returns them in KeyMaterial.
 * Up to 224 bytes of key material may be generated.
 *
 *  @param obj_p               RO:  The Hash info pointer.  It contains:
 *                                  0 - 18 KBytes of secret key (typically the 
 *                                  TLS master secret) and the  length of Key, 
 *                                  from 0 to 18 KBytes inclusive. A length of 0 
 *                                  is legal; the result is computed with a null key.
 *                                  Also defined here is the unit ID.
 *  @param label_p             RO:  A short ascii string; its value depends 
 *                                  on what is being computed. 
 *                               Specific values are
 *                                  defined in [TLS].
 *  @param labelLength        RO:   The  length of Label, from 0 to 18 KBytes 
 *                                  inclusive.
 *                             A length of 0 is legal;
 *                                  the result is 
 *                                  computed with a null label.
 *  @param seed_p          RO:   A seed value, typically some combination 
 *                                  of random bytes supplied by 
 *                             the server and client.
 *                                  Specific values 
 *                                  are defined in [TLS]. 
 *  @param seedLength         RO:   The  length of Seed, from 0 to 18 KBytes 
 *                                  inclusive.
 *                               A length of 0 is legal;
 *                                  the result is computed with a
 *                                  null seed.
 *  @param outputLength       RO:   The number of bytes of output requested; 
 *                                  i.e. the length of KeyMaterial,
 *                               from 0 - 224 inclusive.
 *  @param keyMaterial_p      WO:   The computed key material.  Note only as 
 *                                  many bytes as requested are ever returned, 
 *                                  even if the algorithm may compute more.
 *                             (As defined the algorithm almost
 *                                  always
 *                                  computes extra bytes.) 
 *  @param event_p             RW:  On input, if null the call
 *                                  is synchronous and no event is
 *                                  returned. The operation is
 *                                  complete when the call returns. 
 *                                  If non-null, then the call is
 *                                  asynchronous; 
 *                                  an event is returned that can be used to 
 *                                  determine when the operation completes.
 *
 * @return
 *    Error condition if raised.
 *
 * @par Errors
 *    N8_INVALID_KEY_SIZE - provided key is the wrong size<br>
 *    N8_INVALID_OUTPUT_SIZE - the requested output length is wrong<br>
 *    N8_INVALID_OBJECT - data pointer is null<br>
 *
 * @par Assumptions
 *    keyMaterial_p points to an allocated buffer of sufficient size.
 ****************************************************************************/
N8_Status_t N8_TLSKeyMaterialHash(N8_HashInfo_t     *obj_p,
                                  const N8_Buffer_t *label_p,
                                  const unsigned int labelLength,
                                  const N8_Buffer_t *seed_p,
                                  const unsigned int seedLength,
                                  const unsigned int outputLength,
                                  N8_Buffer_t       *keyMaterial_p,
                                  N8_Event_t        *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Buffer_t *msg_p = NULL;
   uint32_t     msg_a;
   uint32_t     kKey_a;
   N8_Buffer_t *kKey_p = NULL;
   uint32_t     prS_a[2];
   N8_Buffer_t *prS_p[2];
   N8_Buffer_t resultStream_p[N8_MAX_TLS_KEY_MATERIAL_LENGTH];
   N8_Status_t ret = N8_STATUS_OK;
   int totalLength;
   int secretHalves;
   N8_TLSKeyHashResults_t *keyResultStruct_p;
   int keyLen, hashLen;
   unsigned int numCommands;
   int bufferA_MD5_length;
   int bufferA_SHA1_length;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(obj_p, ret);
      CHECK_OBJECT(obj_p->key_p, ret);
      CHECK_OBJECT(label_p, ret);
      CHECK_OBJECT(seed_p, ret);
      CHECK_OBJECT(keyMaterial_p, ret);

      /* check key length */
      if (obj_p->keyLength > N8_MAX_KEY_LENGTH)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      /* check label length */
      if (labelLength > N8_MAX_KEY_LENGTH)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }
 
      /* check seed length */
      if (seedLength > N8_MAX_KEY_LENGTH)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      if ((obj_p->keyLength + labelLength + seedLength) > N8_MAX_KEY_LENGTH)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      /* check output buffer length */
      if (outputLength > N8_MAX_TLS_KEY_MATERIAL_LENGTH)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      numCommands = N8_CB_EA_TLSKEYMATERIALHASH_NUMCMDS(outputLength);

      /* concatenate label and seeds */
      totalLength = labelLength+seedLength;

      /* allocate kernel memory for the key, psuedo random streams, and msg*/
      keyLen = (NEXT_WORD_SIZE(obj_p->keyLength) +
               (NEXT_WORD_SIZE(N8_MAX_TLS_KEY_MATERIAL_LENGTH) * 2) +
               NEXT_WORD_SIZE(totalLength));

      /* compute the buffer size for the hashes */
      bufferA_MD5_length  = totalLength + EA_MD5_Hash_Length;
      bufferA_SHA1_length = totalLength + EA_SHA1_Hash_Length;
      hashLen = NEXT_WORD_SIZE(bufferA_MD5_length) +
                NEXT_WORD_SIZE(bufferA_SHA1_length);

      ret = createEARequestBuffer(&req_p,
                                  obj_p->unitID, 
                                  numCommands,
                                  resultHandlerTLSKeyHash,
                                  keyLen + hashLen);
      CHECK_RETURN(ret);

      kKey_a = req_p->qr.physicalAddress + req_p->dataoffset;
      kKey_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(kKey_p, 0, keyLen + hashLen);
      prS_a[0] = kKey_a + NEXT_WORD_SIZE(obj_p->keyLength);
      prS_p[0] = kKey_p + NEXT_WORD_SIZE(obj_p->keyLength);
      prS_a[1] = prS_a[0] + NEXT_WORD_SIZE(N8_MAX_TLS_KEY_MATERIAL_LENGTH);
      prS_p[1] = prS_p[0] + NEXT_WORD_SIZE(N8_MAX_TLS_KEY_MATERIAL_LENGTH);
      msg_a = prS_a[1] + NEXT_WORD_SIZE(N8_MAX_TLS_KEY_MATERIAL_LENGTH);
      msg_p = prS_p[1] + NEXT_WORD_SIZE(N8_MAX_TLS_KEY_MATERIAL_LENGTH);
      /* copy the input key to the kernel memory area */
      memcpy(kKey_p, obj_p->key_p, obj_p->keyLength);
        
      memcpy(msg_p, label_p, labelLength);
      memcpy(msg_p + labelLength, seed_p, seedLength);

      memset(&resultStream_p[0], 0x0, N8_MAX_TLS_KEY_MATERIAL_LENGTH);
      memset(prS_p[0], 0x0, N8_MAX_TLS_KEY_MATERIAL_LENGTH);
      memset(prS_p[1], 0x0, N8_MAX_TLS_KEY_MATERIAL_LENGTH);
        
      secretHalves = (int) (obj_p->keyLength / 2);

      ret = cb_ea_TLSKeyMaterialHash(req_p,
                                     req_p->EA_CommandBlock_ptr, 
                                     msg_p,
                                     msg_a,
                                     totalLength,
                                     kKey_p,
                                     kKey_a,
                                     secretHalves,
                                     outputLength,
                                     prS_a[0], 
                                     prS_a[1],
                                     keyLen);
      CHECK_RETURN(ret);

      /* create a struct to carry results to the result handler */
      keyResultStruct_p = (N8_TLSKeyHashResults_t *)&req_p->postProcessBuffer;
      keyResultStruct_p->result_p = keyMaterial_p;
      keyResultStruct_p->prS_p[0] = prS_p[0];
      keyResultStruct_p->prS_p[1] = prS_p[1];
      keyResultStruct_p->outputLength = outputLength;
        
      req_p->postProcessingData_p = (void *) keyResultStruct_p;
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

   } while (FALSE);
   /* clean up */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* N8_TLSKeyMaterialHash */

/*****************************************************************************
 * N8_IKEPrf
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Computes the PRF/HMAC for the given message and key
 *
 * Computes the IKE keyed pseudo-random-function (prf) of the specified Key
 * of length KeyLength and Message of length MessageLength and returns the
 * results in Result. As specified in Sec. 4 of RFC 2409 (see [IKE] in
 * Appendix B: Reference Documentation), the only current pseudo-random
 * functions are the HMAC version of the negotiated hash algorithm. The hash
 * algorithm to use, and by implication the length of Result, is indicated
 * by the enumeration value specified in the Algorithm element of HashInfo.
 * If the Algorithm element of HashInfo is N8_HMAC_MD5, Result is 16 bytes;
 * if Algorithm is N8_HMAC_SHA1, Result is 20 bytes.
 *
 *
 * @param alg RO: algorthim (MD5 or SHA-1)
 * @param hashInfo_p RO: pointer to hash info struct
 * @param msg_p RO: pointer to message buffer
 * @param msgLength RO: input message length in bytes
 * @param result_p RW: pointer where hardware should copy result
 * @param event_p RW: pointer to event N8 event structure
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - The hash algorithm specified in hashInfo is not one
 *                      of the constants N8_HMAC_MD5 or N8_HMAC_SHA1.
 *    N8_INVALID_INPUT_SIZE - The value of msgLength is < 0 or > 18KB; no
 *                            operation is performed and no result is returned.
 *    N8_INVALID_OBJECT -  if any pointers are bad
 *    N8_INVALID_KEY_SIZE - The value of hashInfo_p->keyLength is < 0 or > 64B
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 * The caller is responsible for ensuring that sufficient space is allocated
 * for Result.  Hash info must contain valid key pointer and key length.
 * Key cannot be longer than 64 bytes!
 ****************************************************************************/
N8_Status_t N8_IKEPrf(const N8_HashAlgorithm_t alg,
                      const N8_HashInfo_t *hashInfo_p,
                      const N8_Buffer_t *msg_p,
                      const uint32_t msgLength,
                      N8_Buffer_t *result_p,
                      N8_Event_t *event_p)
{
    API_Request_t *req_p = NULL;
    N8_Status_t ret = N8_STATUS_OK;
    uint32_t  nBytes;
    uint32_t     kKey_a;
    N8_Buffer_t *kKey_p = NULL;
    uint32_t     kRes_a;
    N8_Buffer_t *kRes_p = NULL;
    N8_Buffer_t *kMsg_p = NULL;
    uint32_t     kMsg_a;

    do
    {
        ret = N8_preamble();
        CHECK_RETURN(ret);

        CHECK_OBJECT(hashInfo_p, ret);
        CHECK_OBJECT(hashInfo_p->key_p, ret);
        CHECK_OBJECT(msg_p, ret);
        CHECK_OBJECT(result_p, ret);

        /* Also, if the key length is less than 64 bytes we
         * need to post-pad the key with 0x00 up to 64 bytes.
         * The padding is performed by the lower command
         * block building routine.
         * If the key length is greater than 64 bytes we
         * need to hash the key (results in 64 bytes for MD5 or
         * SHA-1). 
         */

        /* TODO: handle the >64 byte key case:
         * key must be hashed and truncated
         * to 64 bytes
         */
        /* check key length */
#ifdef N8_64BYTE_IKE_KEY_LIMIT
        /* for now we will only accept up to 64 bytes of key */
        if (hashInfo_p->keyLength > EA_HMAC_Key_Length)
#else
        if (hashInfo->keyLength > N8_MAX_KEY_LENGTH)
#endif
        {
            ret = N8_INVALID_KEY_SIZE;
            break;
        }
        /* check msg length */
        if (msgLength > N8_MAX_HASH_LENGTH)
        {
            ret = N8_INVALID_INPUT_SIZE;
            break;
        }

        /* compute kernel memory size for the key and msg */
        /* SHA1 result length=20 is greater than MD5 result length */
        nBytes = (NEXT_WORD_SIZE(hashInfo_p->keyLength) +
                  NEXT_WORD_SIZE(msgLength) +
                  NEXT_WORD_SIZE(SHA1_HASH_RESULT_LENGTH));

        ret = createEARequestBuffer(&req_p,
                                    hashInfo_p->unitID, 
                                    N8_CB_EA_IKEPRF_NUMCMDS,
                                    resultHandlerGeneric,
                                    nBytes);
        CHECK_RETURN(ret);

        /* set up pointers for key | msg | res */
        kKey_a = req_p->qr.physicalAddress + req_p->dataoffset;
        kKey_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
        memset(kKey_p, 0, nBytes);
        kMsg_a = kKey_a + NEXT_WORD_SIZE(hashInfo_p->keyLength);
        kMsg_p = kKey_p + NEXT_WORD_SIZE(hashInfo_p->keyLength);
        kRes_a = kMsg_a + NEXT_WORD_SIZE(msgLength);
        kRes_p = kMsg_p + NEXT_WORD_SIZE(msgLength);

        /* copy the input key to the kernel memory area */
        memcpy(kKey_p, hashInfo_p->key_p, hashInfo_p->keyLength);
        /* copy the input msg to the kernel memory area */
        memcpy(kMsg_p, msg_p, msgLength);

        ret = cb_ea_IKEPrf(req_p,
                           req_p->EA_CommandBlock_ptr, 
                           alg,
                           kMsg_a,
                           msgLength,
                           kKey_p,
                           hashInfo_p->keyLength,
                           kRes_a);
        CHECK_RETURN(ret);

        /* setup up post-processing info */
        req_p->copyBackFrom_p = kRes_p;
        req_p->copyBackTo_p = result_p;

        
        /* set copy back size based on alg */
        switch (alg)
        {
        case N8_HMAC_SHA1:
            {
                req_p->copyBackSize = SHA1_HASH_RESULT_LENGTH;
                break;
            }
        case N8_HMAC_MD5:
            {                   
                req_p->copyBackSize = MD5_HASH_RESULT_LENGTH;
                break;
            }
        default:
            {
                ret = N8_INVALID_HASH;
                break;
            }
        }
        
        CHECK_RETURN(ret);

        QUEUE_AND_CHECK(event_p, req_p, ret);
        HANDLE_EVENT(event_p, req_p, ret);
    } while (FALSE);
    /* clean up */
    if (ret != N8_STATUS_OK)
    {
        freeRequest(req_p);
    }
    return ret;
}   /* N8_IKEPrf */

/*****************************************************************************
 * N8_IKESKEYIDExpand
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Computes SKEYID_d,a,e for the given message and key
 *
 * Computes key material from the specified Key of length KeyLength and
 * Message of length Message-Length, according to the IKE authenticated
 * keying material generating algorithm in Sec. 5 of RFC 2409 (see [IKE] in
 * Appendix B: Reference Documentation), returning the three results SKEYID_d,
 * SKEYID_a, and SKEYID_e.
 *
 * The hash algorithm to use, and by implication the lengths of the three
 * results, is indicated by the enu-meration value specified in the Algorithm\
 * element of HashInfo. If Algorithm is N8_HMAC_MD5, each result is 16 bytes;
 * if Algorithm is N8_HMAC_SHA1 each result is 20 bytes. The caller is
 * responsible for ensuring that sufficient space is allocated for these
 * results. (Note that if these result values are shorter than the required
 * key material, IKE specifies how these results are expanded.) 
 *
 * @param alg RO: algorthim (MD5 or SHA-1)
 * @param hashInfo_p RO: a pointer to hashInfo struct
 * @param msg_p RO: pointer to input message
 * @param msgLength RO: input message length in bytes
 * @param SKEYIDd_p RW: pointer to SKEYID_d output buffer
 * @param SKEYIDa_p RW: pointer to SKEYID_a output buffer
 * @param SKEYIDe_p RW: pointer to SKEYID_e output buffer
 * @param event_p RW: pointer to N8 event structure
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - The hash algorithm specified in hashInfo is not one
 *                      of the constants N8_HMAC_MD5 or N8_HMAC_SHA1.
 *    N8_INVALID_INPUT_SIZE - The value of msgLength is < 0 or > 18KB; no
 *                            operation is performed and no result is returned.
 *    N8_INVALID_OBJECT -  if any pointers are bad
 *    N8_INVALID_KEY_SIZE - The value of hashInfo_p->keyLength is < 0 or > 64B
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    The caller is responsible for ensuring that sufficient space is allocated
 *    for Result.  Hash info must contain valid key pointer and key length.
 * Key cannot be longer than 64 bytes!
 ****************************************************************************/
N8_Status_t N8_IKESKEYIDExpand (const N8_HashAlgorithm_t alg,
                                const N8_HashInfo_t *hashInfo_p,
                                const N8_Buffer_t *msg_p,
                                const uint32_t msgLength,
                                N8_Buffer_t *SKEYIDd_p,
                                N8_Buffer_t *SKEYIDa_p,
                                N8_Buffer_t *SKEYIDe_p,
                                N8_Event_t *event_p)
{
    API_Request_t *req_p = NULL;
    N8_Status_t ret = N8_STATUS_OK;
    uint32_t  nBytes;
    uint32_t     kKey_a;
    N8_Buffer_t *kKey_p = NULL;
    uint32_t     kSKEYIDd_a;
    N8_Buffer_t *kSKEYIDd_p = NULL;
    N8_Buffer_t *kMsg_p = NULL;
    uint32_t     kMsg_a;
    N8_IKESKEYIDExpandResults_t *keyResultStruct_p = NULL;

    do
    {
        ret = N8_preamble();
        CHECK_RETURN(ret);

        CHECK_OBJECT(hashInfo_p, ret);
        CHECK_OBJECT(hashInfo_p->key_p, ret);
        CHECK_OBJECT(msg_p, ret);
        CHECK_OBJECT(SKEYIDd_p, ret);
        CHECK_OBJECT(SKEYIDa_p, ret);
        CHECK_OBJECT(SKEYIDe_p, ret);

        /* Also, if the key length is less than 64 bytes we
         * need to post-pad the key with 0x00 up to 64 bytes.
         * The padding is performed by the lower command
         * block building routine.
         * If the key length is greater than 64 bytes we
         * need to hash the key (results in 64 bytes for MD5 or
         * SHA-1). 
         */

        /* TODO: handle the >64 byte key case:
         * key must be hashed and truncated
         * to 64 bytes
         */
        /* check key length */
#ifdef N8_64BYTE_IKE_KEY_LIMIT
        /* for now we will only accept up to 64 bytes of key */
        if (hashInfo_p->keyLength > EA_HMAC_Key_Length)
#else
        if (hashInfo->keyLength > N8_MAX_KEY_LENGTH)
#endif
        {
            ret = N8_INVALID_KEY_SIZE;
            break;
        }
        /* check msg length */
        if (msgLength > N8_MAX_HASH_LENGTH)
        {
            ret = N8_INVALID_INPUT_SIZE;
            break;
        }

        /* compute kernel memory size for the key and msg */
        /* SHA1 result length=20 is the max(sha1,md5)
         * result length
         */
        nBytes =
           (NEXT_WORD_SIZE(hashInfo_p->keyLength) +
            NEXT_WORD_SIZE(msgLength) +
            NEXT_WORD_SIZE(N8_IKE_SKEYID_ITERATIONS*SHA1_HASH_RESULT_LENGTH));

        ret = createEARequestBuffer(&req_p,
                                    hashInfo_p->unitID, 
                                    N8_CB_EA_IKESKEYIDEXPAND_NUMCMDS,
                                    resultHandlerIKESKEYIDExpand,
                                    nBytes);
        CHECK_RETURN(ret);

        /* set up pointers for key | msg | res */
        kKey_a = req_p->qr.physicalAddress + req_p->dataoffset;
        kKey_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
        memset(kKey_p, 0, nBytes);
        kMsg_a = kKey_a + NEXT_WORD_SIZE(hashInfo_p->keyLength);
        kMsg_p = kKey_p + NEXT_WORD_SIZE(hashInfo_p->keyLength);
        kSKEYIDd_a = kMsg_a + NEXT_WORD_SIZE(msgLength);
        kSKEYIDd_p = kMsg_p + NEXT_WORD_SIZE(msgLength);

        /* copy the input key to the kernel memory area */
        memcpy(kKey_p, hashInfo_p->key_p, hashInfo_p->keyLength);
        /* copy the input msg to the kernel memory area */
        memcpy(kMsg_p, msg_p, msgLength);

        ret = cb_ea_IKESKEYIDExpand(req_p,
                                    req_p->EA_CommandBlock_ptr, 
                                    alg,
                                    kMsg_a,
                                    msgLength,
                                    kKey_p,
                                    hashInfo_p->keyLength,
                                    kSKEYIDd_a);
        CHECK_RETURN(ret);

        /* create a struct to carry results to the result handler */
        keyResultStruct_p = (N8_IKESKEYIDExpandResults_t *)&req_p->postProcessBuffer;
        keyResultStruct_p->result_p = kSKEYIDd_p;
        keyResultStruct_p->SKEYIDd_p = SKEYIDd_p;
        keyResultStruct_p->SKEYIDa_p = SKEYIDa_p;
        keyResultStruct_p->SKEYIDe_p = SKEYIDe_p;

        /* set output length based on alg */
        switch (alg)
        {
        case N8_HMAC_SHA1:
            {
                keyResultStruct_p->outputLength = SHA1_HASH_RESULT_LENGTH;
                break;
            }
        case N8_HMAC_MD5:
            {                   
                keyResultStruct_p->outputLength = MD5_HASH_RESULT_LENGTH;
                break;
            }
        default:
            {
                ret = N8_INVALID_HASH;
                break;
            }
        }
        
        CHECK_RETURN(ret);

        req_p->postProcessingData_p = (void *) keyResultStruct_p;
        QUEUE_AND_CHECK(event_p, req_p, ret);
        HANDLE_EVENT(event_p, req_p, ret);
    } while (FALSE);
    /* clean up */
    if (ret != N8_STATUS_OK)
    {
        freeRequest(req_p);
    }
    return ret;
}   /* N8_IKESKEYIDExpand */

/*****************************************************************************
 * N8_IKEKeyMaterialExpand
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Performs key expansion using the given message and key up to the
 *        specfied length
 *
 * Computes OutputLength bytes of key material from the specified Key of
 * length KeyLength and Mes-sage of length MessageLength using the hash
 * function specified by the Algorithm element of HashInfo. If
 * MessageLength is greater than 0, then it denotes the size of Message,
 * and Message is used to generate key material according to the IKE key
 * expansion algorithm in Sec. 5.5 of RFC 2409 (see [IKE] in Appendix B:
 * Reference Documentation).
 * The generated key material is returned in Output. Up to 240 bytes of key
 * material may be generated using HMAC-MD5, and up to 300 bytes using
 * HMAC-SHA-1. The requested number of returned Out-put bytes is specified
 * in OutputLength. The caller is responsible for ensuring that sufficient
 * space is allocated for Output. The hash algorithm used is indicated by
 * the enumeration value in the Algorithm element of HashInfo. 
 *
 * @param alg RO: algorthim (MD5 or SHA-1)
 * @param hashInfo_p RO: pointer to hashInfo struct
 * @param msg_p RO: pointer to input message
 * @param msgLength RO: input message length in bytes
 * @param result_p RO: pointer to output buffer
 * @param result_len RO: number of key expansion bytes requested by caller
 * @param event_p RW: pointer to N8 event structure
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - The hash algorithm specified in hashInfo is not one
 *                      of the constants N8_HMAC_MD5 or N8_HMAC_SHA1.
 *    N8_INVALID_INPUT_SIZE - The value of msgLength is < 0 or > 18KB;
 *                            no operation is performed and no result is
 *                            returned.
 *    N8_INVALID_OUTPUT_SIZE - The value of result_len > 240 when
 *                             alg is N8_HMAC_MD5, or > 300 when alg
 *                             is N8_HMAC_SHA1; no operation is performed
 *                             and no result is returned.
 *    N8_INVALID_OBJECT -  if any pointers are bad
 *    N8_INVALID_KEY_SIZE - The value of hashInfo_p->keyLength is < 0 or > 64B
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    The caller is responsible for ensuring that sufficient space is
 *    allocated for Result.  Hash info must contain valid key pointer
 *    and key length.
 * Key cannot be longer than 64 bytes!
 ****************************************************************************/
N8_Status_t N8_IKEKeyMaterialExpand(const N8_HashAlgorithm_t alg,
                                    const N8_HashInfo_t *hashInfo_p,
                                    const N8_Buffer_t *msg_p,
                                    const uint32_t msgLength,
                                    N8_Buffer_t *result_p,
                                    const uint32_t result_len,
                                    N8_Event_t *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Status_t ret = N8_STATUS_OK;
   uint32_t  nBytes;
   uint32_t     kKey_a;
   N8_Buffer_t *kKey_p = NULL;
   N8_Buffer_t *kMsg_p = NULL;
   uint32_t     kMsg_a;
   N8_Buffer_t *kRes_p = NULL;
   uint32_t     kRes_a;
   uint32_t     hash_len = 0;
   uint32_t     i_count;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
        
      /* if result_len is 0 - no computing is done */
      if (result_len == 0)
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }
      CHECK_OBJECT(hashInfo_p, ret);
      CHECK_OBJECT(hashInfo_p->key_p, ret);
      CHECK_OBJECT(msg_p, ret);
      CHECK_OBJECT(result_p, ret);

      /* Also, if the key length is less than 64 bytes we
       * need to post-pad the key with 0x00 up to 64 bytes.
       * The padding is performed by the lower command
       * block building routine.
       * If the key length is greater than 64 bytes we
       * need to hash the key (results in 64 bytes for MD5 or
       * SHA-1). 
       */

      /* TODO: handle the >64 byte key case:
       * key must be hashed and truncated
       * to 64 bytes
       */
      /* check key length */
#ifdef N8_64BYTE_IKE_KEY_LIMIT
      /* for now we will only accept up to 64 bytes of key */
      if (hashInfo_p->keyLength > EA_HMAC_Key_Length)
#else
         if (hashInfo_p->keyLength > N8_MAX_KEY_LENGTH)
#endif
         {
            ret = N8_INVALID_KEY_SIZE;
            break;
         }
      /* check msg length */
      if (msgLength > N8_MAX_HASH_LENGTH)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      /* set hash_len based on alg */
      switch (alg)
      {
         case N8_HMAC_SHA1:
         {
            hash_len = SHA1_HASH_RESULT_LENGTH;
            break;
         }
         case N8_HMAC_MD5:
         {                   
            hash_len = MD5_HASH_RESULT_LENGTH;
            break;
         }
         default:
         {
            ret = N8_INVALID_HASH;
            break;
         }
      }
        
      CHECK_RETURN(ret);
        
      /* check output length */
      if (result_len > N8_MAX_IKE_ITERATIONS*hash_len)
      {
         ret = N8_INVALID_OUTPUT_SIZE;
         break;
      }

      /* compute iteration count: we want the WHOLE number
       * of iterations we need to produce the requested
       * amount of data
       */
      if ((result_len % hash_len) != 0)
      {
         i_count = result_len/hash_len + 1;
      }
      else
      {
         i_count = result_len/hash_len;
      }

      /* compute kernel memory size for the key and msg */
      nBytes = (NEXT_WORD_SIZE(hashInfo_p->keyLength) +
                NEXT_WORD_SIZE(msgLength) +
                NEXT_WORD_SIZE(i_count*hash_len));   /* for result */
      ret = createEARequestBuffer(&req_p,
                                  hashInfo_p->unitID, 
                                  N8_CB_EA_IKEKEYMATERIALEXPAND_NUMCMDS,
                                  resultHandlerGeneric, nBytes);
      CHECK_RETURN(ret);

      /* set up pointers for key | msg | res */
      kKey_a = req_p->qr.physicalAddress + req_p->dataoffset;
      kKey_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(kKey_p, 0, nBytes);
      kMsg_a = kKey_a + NEXT_WORD_SIZE(hashInfo_p->keyLength);
      kMsg_p = kKey_p + NEXT_WORD_SIZE(hashInfo_p->keyLength);
      kRes_a = kMsg_a + NEXT_WORD_SIZE(msgLength);
      kRes_p = kMsg_p + NEXT_WORD_SIZE(msgLength);

      /* copy the input key to the kernel memory area */
      memcpy(kKey_p, hashInfo_p->key_p, hashInfo_p->keyLength);
      /* copy the input msg to the kernel memory area */
      memcpy(kMsg_p, msg_p, msgLength);

      ret = cb_ea_IKEKeyMaterialExpand(req_p,
                                       req_p->EA_CommandBlock_ptr,
                                       alg,
                                       kMsg_a,
                                       msgLength,
                                       kKey_p,
                                       hashInfo_p->keyLength,
                                       kRes_a,
                                       i_count);
      CHECK_RETURN(ret);

      /* setup up post-processing info */
      req_p->copyBackFrom_p = kRes_p;
      req_p->copyBackTo_p = result_p;
      req_p->copyBackSize = result_len;

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);
   /* clean up */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;

}   /* N8_IKEKeyMaterialExpand */


/*****************************************************************************
 * N8_IKEEncryptKeyExpand
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Performs key expansion using the given message and key up to the
 *        specfied length
 * 
 * Computes OutputLength bytes of key material from the specified Key of
 * length KeyLength using the hash function specified in the Algorithm element
 * of HashInfo, according to the IKE key expansion algo-rithm in Sec. 5.3 of
 * RFC 2409. (See [IKE] in Appendix B: Reference Documentation.) The generated
 * key material is returned in Output.
 * Up to 240 bytes of key material may be generated using HMAC-MD5, and 300
 * bytes using HMAC-SHA-1. The caller is responsible for ensuring that
 * sufficient space is allocated for Output. The hash algorithm used is
 * indicated by the enumeration value in the Algorithm element of HashInfo.
 * Proper computation requires a msg of a single zero byte.  This zero byte
 * is allocated by internally by the API.
 *
 * @param alg RO: algorthim (MD5 or SHA-1)
 * @param hashInfo_p RO: pointer to hashInfo structure
 * @param result_p RW: pointer to result buffer
 * @param result_len RO: caller-specified output length in bytes
 * @param event_p RW: pointer to N8 event structure
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - The hash algorithm specified in hashInfo is not one
 *                      of the constants N8_HMAC_MD5 or N8_HMAC_SHA1.
 *    N8_INVALID_OUTPUT_SIZE - The value of result_len is < 0 or > 240 when
 *                             alg is N8_HMAC_MD5, or < 0 or > 300 when alg
 *                             is N8_HMAC_SHA1; no operation is performed
 *                             and no result is returned.
 *    N8_INVALID_OBJECT -  if any pointers are bad
 *    N8_INVALID_KEY_SIZE - The value of hashInfo_p->keyLength is < 0 or > 64B
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    The caller is responsible for ensuring that sufficient space is allocated
 *    for Result.  Hash info must contain valid key pointer and key length.
 * Key cannot be longer than 64 bytes!
 ****************************************************************************/
N8_Status_t N8_IKEEncryptKeyExpand(const N8_HashAlgorithm_t alg,
                                   const N8_HashInfo_t *hashInfo_p,
                                   N8_Buffer_t *result_p,
                                   const uint32_t result_len,
                                   N8_Event_t *event_p)
{
   API_Request_t *req_p = NULL;
   N8_Status_t ret = N8_STATUS_OK;
   uint32_t  nBytes;
   uint32_t     kKey_a;
   N8_Buffer_t *kKey_p = NULL;
   N8_Buffer_t *kMsg_p = NULL;
   uint32_t     kMsg_a;
   N8_Buffer_t *kRes_p = NULL;
   uint32_t     kRes_a;
   uint32_t     hash_len = 0;
   uint32_t     i_count;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* if result_len is 0 - no computing is done */
      if (result_len == 0)
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }

      CHECK_OBJECT(hashInfo_p, ret);
      CHECK_OBJECT(hashInfo_p->key_p, ret);
      CHECK_OBJECT(result_p, ret);

      /* Also, if the key length is less than 64 bytes we
       * need to post-pad the key with 0x00 up to 64 bytes.
       * The padding is performed by the lower command
       * block building routine.
       * If the key length is greater than 64 bytes we
       * need to hash the key (results in 64 bytes for MD5 or
       * SHA-1). 
       */

      /* TODO: handle the >64 byte key case:
       * key must be hashed and truncated
       * to 64 bytes
       */
      /* check key length */
#ifdef N8_64BYTE_IKE_KEY_LIMIT
      /* for now we will only accept up to 64 bytes of key */
      if (hashInfo_p->keyLength > EA_HMAC_Key_Length)
#else
         if (hashInfo_p->keyLength > N8_MAX_KEY_LENGTH)
#endif
         {
            ret = N8_INVALID_KEY_SIZE;
            break;
         }
        
      /* set hash_len based on alg */
      switch (alg)
      {
         case N8_HMAC_SHA1:
         {
            hash_len = SHA1_HASH_RESULT_LENGTH;
            break;
         }
         case N8_HMAC_MD5:
         {                   
            hash_len = MD5_HASH_RESULT_LENGTH;
            break;
         }
         default:
         {
            ret = N8_INVALID_HASH;
            break;
         }
      }
        
      CHECK_RETURN(ret);
        
      /* check output length */
      if (result_len > N8_MAX_IKE_ITERATIONS*hash_len)
      {
         ret = N8_INVALID_OUTPUT_SIZE;
         break;
      }

      /* compute iteration count: we want the WHOLE number
       * of iterations we need to produce the requested
       * amount of data
       */
      if ((result_len % hash_len) != 0)
      {
         i_count = result_len/hash_len + 1;
      }
      else
      {
         i_count = result_len/hash_len;
      }

      /* compute kernel memory size for the key and msg */
      /* there is a single zero byte message that
       * always required for this API call
       */
      nBytes = (NEXT_WORD_SIZE(hashInfo_p->keyLength) +
                NEXT_WORD_SIZE(N8_IKE_ZERO_BYTE_LEN) +   /* one zero byte message */
                NEXT_WORD_SIZE(i_count*hash_len));   /* for result */

      ret = createEARequestBuffer(&req_p,
                                  hashInfo_p->unitID, 
                                  N8_CB_EA_IKEENCRYPTKEYEXPAND_NUMCMDS,
                                  resultHandlerGeneric, nBytes);
      CHECK_RETURN(ret);

      /* set up pointers for key | res | msg */
      kKey_a = req_p->qr.physicalAddress + req_p->dataoffset;
      kKey_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(kKey_p, 0, nBytes);
      kRes_a = kKey_a + NEXT_WORD_SIZE(hashInfo_p->keyLength);
      kRes_p = kKey_p + NEXT_WORD_SIZE(hashInfo_p->keyLength);
      kMsg_a = kRes_a + NEXT_WORD_SIZE(i_count*hash_len);
      kMsg_p = kRes_p + NEXT_WORD_SIZE(i_count*hash_len);

      /* copy the input key to the kernel memory area */
      memcpy(kKey_p, hashInfo_p->key_p, hashInfo_p->keyLength);

      ret = cb_ea_IKEEncryptKeyExpand(req_p,
                                      req_p->EA_CommandBlock_ptr,
                                      alg,
                                      kMsg_a,
                                      N8_IKE_ZERO_BYTE_LEN,
                                      kKey_p,
                                      hashInfo_p->keyLength,
                                      kRes_a,
                                      i_count);
      CHECK_RETURN(ret);

      /* setup up post-processing info */
      req_p->copyBackFrom_p = kRes_p;
      req_p->copyBackTo_p = result_p;
      req_p->copyBackSize = result_len;

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

   } while (FALSE);
   /* clean up */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;

}   /* N8_IKEEncryptKeyExpand */


/*****************************************************************************
 * n8_setInitialIVs
 *****************************************************************************/
/** @ingroup n8_hash
 * @brief Determine the hash type and set the initial IV values for that type.
 *
 *  @param hashObj_p           RW:  Pointer to hash object to be initialized.
 *  @param unit                RO:  Validated unit identifier.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_INVALID_HASH if the hash is not from the set of supported hashes.
 *
 * @par Assumptions
 *    The hash object has the type filed set correctly.
 *****************************************************************************/
N8_Status_t n8_setInitialIVs(N8_HashObject_t *hashObj_p,
                             const N8_HashAlgorithm_t alg,
                             const N8_Unit_t  unit)
{
   N8_Status_t ret = N8_STATUS_OK;
   switch (alg)
   {
      case N8_MD5:
      case N8_HMAC_MD5:
      case N8_HMAC_MD5_96:
         ret = initMD5(hashObj_p, alg, unit);
         break;
      case N8_SHA1:
      case N8_HMAC_SHA1:
      case N8_HMAC_SHA1_96:
         ret = initSHA1(hashObj_p, alg, unit);
         break;
      case N8_HASH_NONE:
         break;
      default:
         ret = N8_INVALID_HASH;
         break;
   }
   return ret;
} /* n8_setInitialIVs */
/*****************************************************************************
 * initMD5
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Initialize MD5 hash
 *
 * Initialize hash object for subsequent MD5 hashing
 *
 *  @param obj_p               RW:  object to be initialized
 *  @param alg                 RO:  algorithm to use (e.g. N8_MD5 or
 *                                  N8_HMAC_MD5) 
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error code.  No abnormal error conditions have been identified.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 ****************************************************************************/
N8_Status_t initMD5(N8_HashObject_t *obj_p,
                    const N8_HashAlgorithm_t alg,
                    int unitID)
{
   N8_Status_t ret = N8_STATUS_OK;
   do 
   {
      DBG(("In initMD5 SAPI Layer\n"));
      CHECK_OBJECT(obj_p, ret);
      CHECK_RETURN(ret);
      memset(obj_p, 0, sizeof(N8_HashObject_t));
      obj_p->iv[0] = MD5_INIT_A;
      obj_p->iv[1] = MD5_INIT_B;
      obj_p->iv[2] = MD5_INIT_C;
      obj_p->iv[3] = MD5_INIT_D;
      obj_p->type = alg;
      obj_p->hashSize = MD5_HASH_RESULT_LENGTH;
      obj_p->unitID = unitID;
      obj_p->structureID = N8_HASH_STRUCT_ID;
   } while (FALSE);

   DBG(("returning from initMD5 SAPI Layer\n"));
   return ret;
} /* initMD5 */

/*****************************************************************************
 * initSHA1
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Initialize SHA-1 hash
 *
 * Initialize hash object for subsequent MD5 hashing
 *
 *  @param obj_p               RW:  object to be initialized
 *  @param alg                 RO:  algorithm to use (e.g. N8_SHA1 or
 *                                  N8_HMAC_SHA1) 
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error code.  No abnormal error conditions have been identified.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 ****************************************************************************/
N8_Status_t initSHA1(N8_HashObject_t *obj_p,
                     const N8_HashAlgorithm_t alg,
                     int unitID)
{
   N8_Status_t ret = N8_STATUS_OK;
   do 
   {
      CHECK_OBJECT(obj_p, ret);
      CHECK_RETURN(ret);
      memset(obj_p, 0, sizeof(N8_HashObject_t));
      obj_p->iv[0] = SHA1_INIT_H0;
      obj_p->iv[1] = SHA1_INIT_H1;
      obj_p->iv[2] = SHA1_INIT_H2;
      obj_p->iv[3] = SHA1_INIT_H3;
      obj_p->iv[4] = SHA1_INIT_H4;
      obj_p->type = alg;
      obj_p->hashSize = SHA1_HASH_RESULT_LENGTH;
      obj_p->unitID = unitID;
      obj_p->structureID = N8_HASH_STRUCT_ID;
   } while (FALSE);
   return ret;
} /* initSHA1 */

/*
 * Static Functions
 */

/*****************************************************************************
 * getRoleString
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Produce the correct role string for a given protocol and role.
 *
 * Various protocols and roles require different constant strings to
 * be included in a hash.  This routine returns the correct string for
 * a given protocol/role combination.
 *
 *  @param protocol            RO:  Protocol in use.
 *  @param role                RW:  Role in use.
 *
 * @par Externals
 *    None
 *
 * @return
 *    String to use.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 ****************************************************************************/
static char * getRoleString(const N8_HashProtocol_t protocol,
                            const N8_HashRole_t role)
{
   char *string;
   switch (protocol)
   {
      case N8_SSL_FINISH:
         if (role == N8_SERVER)
         {
            string = n8_strdup("SRVR");
         }
         else
         {
            string = n8_strdup("CLNT");
         }
         break;
      case N8_TLS_FINISH:
         if (role == N8_SERVER)
         {
            string = n8_strdup("server finished");
         }
         else
         {
            string = n8_strdup("client finished");
         }
         break;
      case N8_SSL_CERT:
      case N8_TLS_CERT:
      default:
         string = n8_strdup("");
         break;
   }
   return string;
} /* getRoleString */

/*****************************************************************************
 * computeSSLHandshakeHash
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Compute the SSL Hash
 *
 * Do the work to compute the SSL handshake hash for one specified hash.
 *
 *  @param hObj_p              RW:  Hash object to use in performing
 *                                  the hash.
 *  @param roleString_p        RW:  String constant for role to be
 *                                  used in the hash.
 *  @param key_p               RW:  Key to be used in the hash.
 *  @param keyLength           RW:  Length of the key.
 *  @param result_p            RW:  Pointer to buffer for results.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    Standard
 *
 * @par Assumptions
 *    result_p is a pointer to an allocated buffer of adequate size.
 ****************************************************************************/
static N8_Status_t  computeSSLHandshakeHash(N8_HashObject_t *hObjMD5_p,
                                            N8_HashObject_t *hObjSHA_p,
                                            const char *roleString_p,
                                            const N8_Buffer_t *key_p,
                                            const unsigned int keyLength,
                                            N8_Buffer_t *resultMD5_p,
                                            N8_Buffer_t *resultSHA_p,
                                            N8_Event_t  *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;

   API_Request_t *req_p = NULL;
   unsigned int nBytes;
   N8_HandshakeEndResults_t  *callBackData_p = NULL;

   N8_Buffer_t *innerMsg_md5_p = NULL;
   uint32_t     innerMsg_md5_a;
   N8_Buffer_t *innerResult_md5_p = NULL;
   uint32_t     innerResult_md5_a;
   N8_Buffer_t *outerMsg_md5_p = NULL;
   uint32_t     outerMsg_md5_a;
   N8_Buffer_t *endResult_md5_p = NULL;
   uint32_t     endResult_md5_a;

   N8_Buffer_t *innerMsg_sha_p = NULL;
   uint32_t     innerMsg_sha_a;
   N8_Buffer_t *innerResult_sha_p = NULL;
   uint32_t     innerResult_sha_a;
   N8_Buffer_t *outerMsg_sha_p = NULL;
   uint32_t     outerMsg_sha_a;
   N8_Buffer_t *endResult_sha_p = NULL;
   uint32_t     endResult_sha_a;

   N8_Buffer_t *hashMsgMD5_p = NULL;
   uint32_t     hashMsgMD5_a;

   N8_Buffer_t *hashMsgSHA_p = NULL;
   uint32_t     hashMsgSHA_a;

   unsigned int roleStringLen = strlen(roleString_p);

   unsigned int hashingLength_md5 = 0; /* totalLength rounded down to the */
   unsigned int hashingLength_sha = 0; /* next multiple of  N8_HASH_BLOCK_SIZE */

   unsigned int innerMsgLen_md5;
   unsigned int innerMsgLen_sha;
   unsigned int outerMsgLen_md5;
   unsigned int outerMsgLen_sha;
   unsigned int i;
   do
   {
      innerMsgLen_md5 = keyLength + MD5_PAD_LENGTH + roleStringLen;
      outerMsgLen_md5 = MD5_HASH_RESULT_LENGTH + keyLength + MD5_PAD_LENGTH;
      innerMsgLen_sha = keyLength + SHA1_PAD_LENGTH + roleStringLen;
      outerMsgLen_sha = SHA1_HASH_RESULT_LENGTH + keyLength + SHA1_PAD_LENGTH;

      /* compute size of the results and the incoming message */
      nBytes =  NEXT_WORD_SIZE(innerMsgLen_md5) +           /* innerMsg_md5  */
                NEXT_WORD_SIZE(outerMsgLen_md5) +           /* outerMsg_md5  */
                NEXT_WORD_SIZE(hObjMD5_p->hashSize) +       /* endResult_md5 */
                NEXT_WORD_SIZE(innerMsgLen_sha) +           /* innerMsg_sha  */
                NEXT_WORD_SIZE(outerMsgLen_sha) +           /* outerMsg_sha  */
                NEXT_WORD_SIZE(hObjSHA_p->hashSize) +       /* endResult_sha */
                NEXT_WORD_SIZE(N8_MAX_RESIDUAL_LEN + innerMsgLen_md5) +
                                                            /* hashMsgmd5    */
                NEXT_WORD_SIZE(N8_MAX_RESIDUAL_LEN + innerMsgLen_sha) ;
                                                            /* hashMsgsha    */
      
      ret = createEARequestBuffer(&req_p,
                                  hObjMD5_p->unitID,
                                  N8_CB_EA_SSLSHANDSHAKEHASH_NUMCMDS,
                                  resultHandlerHandshakeEnd, nBytes);

      /* setup the space for the chip to place the result */
      innerMsg_md5_a = req_p->qr.physicalAddress + req_p->dataoffset;
      innerMsg_md5_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(innerMsg_md5_p, 0, nBytes);

      outerMsg_md5_a = innerMsg_md5_a + NEXT_WORD_SIZE(innerMsgLen_md5);
      outerMsg_md5_p = innerMsg_md5_p + NEXT_WORD_SIZE(innerMsgLen_md5);

      /* the inner result must start EXACTLY (keyLength + MD5_PAD_LENGTH) bytes
       * from the beginning of outerMsg_md5.  do not round up to the next word
       * size. */
      innerResult_md5_a = outerMsg_md5_a + (keyLength + MD5_PAD_LENGTH);
      innerResult_md5_p = outerMsg_md5_p + (keyLength + MD5_PAD_LENGTH);

      endResult_md5_a = outerMsg_md5_a + NEXT_WORD_SIZE(outerMsgLen_md5);
      endResult_md5_p = outerMsg_md5_p + NEXT_WORD_SIZE(outerMsgLen_md5);
   
      innerMsg_sha_a = endResult_md5_a + NEXT_WORD_SIZE(hObjMD5_p->hashSize);
      innerMsg_sha_p = endResult_md5_p + NEXT_WORD_SIZE(hObjMD5_p->hashSize);
   
      outerMsg_sha_a = innerMsg_sha_a + NEXT_WORD_SIZE(innerMsgLen_sha);
      outerMsg_sha_p = innerMsg_sha_p + NEXT_WORD_SIZE(innerMsgLen_sha);
      
      innerResult_sha_a = outerMsg_sha_a + (keyLength + SHA1_PAD_LENGTH);
      innerResult_sha_p = outerMsg_sha_p + (keyLength + SHA1_PAD_LENGTH);
      
      endResult_sha_a = outerMsg_sha_a + NEXT_WORD_SIZE(outerMsgLen_sha);
      endResult_sha_p = outerMsg_sha_p + NEXT_WORD_SIZE(outerMsgLen_sha);
   
      hashMsgMD5_a = endResult_sha_a + NEXT_WORD_SIZE(hObjSHA_p->hashSize);
      hashMsgMD5_p = endResult_sha_p + NEXT_WORD_SIZE(hObjSHA_p->hashSize);
   
      hashMsgSHA_a = hashMsgMD5_a + NEXT_WORD_SIZE(N8_MAX_RESIDUAL_LEN + innerMsgLen_md5);
      hashMsgSHA_p = hashMsgMD5_p + NEXT_WORD_SIZE(N8_MAX_RESIDUAL_LEN + innerMsgLen_md5);
   
      /* construct the remainder of the inner message to be hashed:
       * innerMsg_p = roleString_p + key_p + pad1_p
       * note that the hash of the message is already present in the
       * results of the hash object.
       */
      /* construct the inner message:
       */
      i = 0;
      memcpy(&(innerMsg_md5_p[i]), roleString_p, roleStringLen);
      i += roleStringLen;
      memcpy(&(innerMsg_md5_p[i]), key_p, keyLength);
      i += keyLength;
      memset(&(innerMsg_md5_p[i]), PAD1, MD5_PAD_LENGTH);

      i = 0;
      memcpy(&(innerMsg_sha_p[i]), roleString_p, roleStringLen);
      i += roleStringLen;
      memcpy(&(innerMsg_sha_p[i]), key_p, keyLength);
      i += keyLength;
      memset(&(innerMsg_sha_p[i]), PAD1, SHA1_PAD_LENGTH);

      /* prepend the residual, if any */
      if (hObjMD5_p->residualLength > 0)
      {
         memcpy(hashMsgMD5_p, hObjMD5_p->residual, hObjMD5_p->residualLength);
         hashingLength_md5 = hObjMD5_p->residualLength;
      }
      memcpy(&(hashMsgMD5_p[hObjMD5_p->residualLength]), innerMsg_md5_p, innerMsgLen_md5);
      hashingLength_md5 += innerMsgLen_md5;

      /* prepend the residual, if any */
      if (hObjSHA_p->residualLength > 0)
      {
         memcpy(hashMsgSHA_p, hObjSHA_p->residual, hObjSHA_p->residualLength);
         hashingLength_sha = hObjSHA_p->residualLength;
      }
      memcpy(&(hashMsgSHA_p[hObjSHA_p->residualLength]), innerMsg_sha_p, innerMsgLen_sha);
      hashingLength_sha += innerMsgLen_sha; 


      /* construct the outer message:
       * outerMsg_p = key + pad2 + innerResult
       */
      i = 0;
      memcpy(&(outerMsg_sha_p[i]), key_p, keyLength);
      i+= keyLength;
      memset(&(outerMsg_sha_p[i]), PAD2, SHA1_PAD_LENGTH);

      i = 0;
      memcpy(&(outerMsg_md5_p[i]), key_p, keyLength);
      i+= keyLength;
      memset(&(outerMsg_md5_p[i]), PAD2, MD5_PAD_LENGTH);

      /* create callback structure */
      callBackData_p = (N8_HandshakeEndResults_t *)&req_p->postProcessBuffer;

      callBackData_p->to_resultSHA_p = resultSHA_p;
      callBackData_p->from_end_resultSHA_p = endResult_sha_p;
      callBackData_p->to_objSHA_p = (void *) hObjSHA_p; 
      callBackData_p->to_resultMD5_p = resultMD5_p;
      callBackData_p->from_end_resultMD5_p = endResult_md5_p;
      callBackData_p->to_objMD5_p = (void *) hObjMD5_p;  
      
      req_p->postProcessingData_p = (void *) callBackData_p;

      ret = cb_ea_SSLHandshakeHash(req_p,
                                   req_p->EA_CommandBlock_ptr,
                                   hObjMD5_p,
                                   innerResult_md5_a,
                                   hashMsgMD5_a,
                                   hashingLength_md5,
                                   hObjSHA_p,
                                   innerResult_sha_a,
                                   hashMsgSHA_a,
                                   hashingLength_sha,
                                   endResult_md5_a,
                                   endResult_sha_a,
                                   outerMsg_md5_a,
                                   outerMsgLen_md5,
                                   outerMsg_sha_a,
                                   outerMsgLen_sha);

      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   }
   while (FALSE);

   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
   
} /* computeSSLHandshakeHash */

/*****************************************************************************
 * computeTLSHandshakeHash
 *****************************************************************************/
/** @ingroup n8_hash
 * @brief Generate the command blocks and set up the result handler for
 * the TLS modes of the N8_HandshakeHashEnd command.
 *
 * This function completes the N8_Handshake in either TLS_FINISH or TLS_CERT
 * mode. The TLS_FINISH calculation is done according to RFC 2246, section 5.
 *
 * @param md5Obj_p     RO: Contains the md5 partial result to complete.
 * @param sha1Obj_p    RO: Contains the sha1 partial result to complete.
 * @param protocol     RO: N8_TLS_CERT or N8_TLS_FINISH
 * @param roleString_p RO: Either "server finished" or "client finished"
 * @param key_p        RO: The key (secret) for the TLS PRF.
 * @param md5Result_p  RW: The md5 result pointer passed in from API.
 * @param sha1Result_p RW: The sha1 result pointer passed in from API.
 * @param event_p      RO: The event pointer
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK: The function has succeeded.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT:  One of the input parameters in invalid.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/

N8_Status_t 
computeTLSHandshakeHash(N8_HashObject_t         *md5Obj_p,
                        N8_HashObject_t         *sha1Obj_p,
                        const N8_HashProtocol_t  protocol,
                        const char              *roleString_p,
                        const N8_Buffer_t       *key_p,
                        const unsigned int       keyLength,
                        N8_Buffer_t             *md5Result_p,
                        N8_Buffer_t             *sha1Result_p,
                        N8_Event_t              *event_p)
{
   N8_Status_t     ret = N8_STATUS_OK;
   N8_Buffer_t    *hashMsgMD5_p = NULL;
   uint32_t        hashMsgMD5_a;
   N8_Buffer_t    *resMD5_p = NULL;
   uint32_t        resMD5_a;
   N8_Buffer_t    *hashMsgSHA1_p = NULL;
   uint32_t        hashMsgSHA1_a;
   N8_Buffer_t    *resSHA1_p = NULL;
   uint32_t        resSHA1_a;
   N8_Buffer_t    *resMD5PRF_p = NULL;
   uint32_t        resMD5PRF_a;
   N8_Buffer_t    *resSHA1PRF_p = NULL;
   uint32_t        resSHA1PRF_a;
   N8_Buffer_t    *roleStr_p = NULL;
   uint32_t        roleStr_a;
   API_Request_t  *req_p = NULL;
   int             nBytes;
   N8_TLSHandshakeHashResults_t *hashResultStruct_p;
   int             nCmdBlocks = 0;
   int             roleSize;

   do
   {
      CHECK_OBJECT(md5Obj_p, ret);
      CHECK_OBJECT(sha1Obj_p, ret);
      CHECK_OBJECT(md5Result_p, ret);
      CHECK_OBJECT(sha1Result_p, ret);
      CHECK_OBJECT(roleString_p, ret);
      CHECK_OBJECT(key_p, ret);

      /* Note:  If we are not doing TLS_FINISH or TLS_CERT we won't be in this
         function */
      if (protocol == N8_TLS_FINISH)
      {
         nCmdBlocks =  N8_CB_EA_FINISHTLSHANDSHAKE_NUMCMDS;
      }
      else if (protocol == N8_TLS_CERT)
      {
         nCmdBlocks =  N8_CB_EA_CERTTLSHANDSHAKE_NUMCMDS;
      }

      /* We need to allocate kernel space for the role string, but we
         don't want to include the null terminator */
      roleSize = strlen(roleString_p);
   
      /* allocate kernel memory for the keys and msgs */
      /* The number of bytes is derived as follows:  We are going
         to complete the MD5 and SHA1 hashes (using opcodes 0x10 and 0x20).
         This requires the MD5 residual lengh, the SHA1 residual length and
         1 * hashSize for each of MD5 and SHA1.  The additional 2 * hashSize
         for each of MD5 and SHA1 are used for the TLS Pseudo Random Function
         required for the N8_TLS_FINISH protocol (using opcodes 0x18 and 0x28).
         We are also allocating NEXT_WORD_SIZE(roleSize) bytes for the role 
         string (either "client finished" or "server finished"). We use the
         NEXT_WORD_SIZE macro to add the extra bytes for the role string
         because we expect to need those extra bytes in the buffer when
         we restore word alignment. Of course, if we are doing a TLS_CERT
         instead of a TLS_FINISH, we could get by with less memory but this
         works fine and handles both cases correctly.
       */
      nBytes = ((3 * NEXT_WORD_SIZE(md5Obj_p->hashSize)) + 
                (3 * NEXT_WORD_SIZE(sha1Obj_p->hashSize)) +
                NEXT_WORD_SIZE(md5Obj_p->residualLength) +
                NEXT_WORD_SIZE(sha1Obj_p->residualLength)
                + NEXT_WORD_SIZE(roleSize));
   
      /* Notice that we are using the unit from the MD5 object.  At some
         point in the future we might like to use different units for 
         each of the requests, but that will require a more general 
         solution */
      ret = createEARequestBuffer(&req_p,
                                  md5Obj_p->unitID,
                                  nCmdBlocks,
                                  resultHandlerTLSHandshakeHash,
                                  nBytes);
      CHECK_RETURN(ret);

      /* Set up the addresses. Note that we are setting this up so that
         after the first two command blocks the role string, the completed
         MD5 hash, and the completed SHA1 hash are contiguous in physical
         memory.  This is because the last two steps of the TLS Finish
         protocol use this concatenation as an input. Also note that
         since the role strings are both 15 bytes, the MD5 and SHA1 result
         buffers are not word aligned.
         */

      /* The role string */
      roleStr_a = req_p->qr.physicalAddress + req_p->dataoffset;
      roleStr_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      memset(roleStr_p, 0, nBytes);

      /* Results of MD5 hash */
      resMD5_a = roleStr_a + roleSize;
      resMD5_p = roleStr_p + roleSize;

      /* Results of SHA1 hash */
      resSHA1_a = resMD5_a + md5Obj_p->hashSize;
      resSHA1_p = resMD5_p + md5Obj_p->hashSize;
   
      /* Input for MD5 hash. The use of NEXT_WORD_SIZE on the resSHA1 ptrs
         is to restore word alignment. */
      hashMsgMD5_a = NEXT_WORD_SIZE(resSHA1_a) + NEXT_WORD_SIZE(sha1Obj_p->hashSize);
      hashMsgMD5_p = (N8_Buffer_t *) NEXT_WORD_SIZE((uint32_t) resSHA1_p) + 
         NEXT_WORD_SIZE(sha1Obj_p->hashSize);
   
      /* Input for SHA1 hash */
      hashMsgSHA1_a = hashMsgMD5_a + NEXT_WORD_SIZE(md5Obj_p->residualLength);
      hashMsgSHA1_p = hashMsgMD5_p + NEXT_WORD_SIZE(md5Obj_p->residualLength);
   
      /* Results of MD5 PRF */
      resMD5PRF_a = hashMsgSHA1_a + NEXT_WORD_SIZE(sha1Obj_p->residualLength);
      resMD5PRF_p = hashMsgSHA1_p + NEXT_WORD_SIZE(sha1Obj_p->residualLength);
   
      /* Results of SHA1 PRF */
      resSHA1PRF_a = resMD5PRF_a + (2 * NEXT_WORD_SIZE(md5Obj_p->hashSize));
      resSHA1PRF_p = resMD5PRF_p + (2 * NEXT_WORD_SIZE(md5Obj_p->hashSize));
   
      /* Copy in the role string */
      memcpy(roleStr_p, roleString_p, roleSize);

      /* Copy in messages to hash */
      /* preappend the residual, if any */
      if (md5Obj_p->residualLength > 0)
      {
         memcpy(hashMsgMD5_p, md5Obj_p->residual, md5Obj_p->residualLength);
      }

      /* prepend the residual, if any */
      if (sha1Obj_p->residualLength > 0)
      {
          memcpy(hashMsgSHA1_p, sha1Obj_p->residual, sha1Obj_p->residualLength);
      }
      ret = cb_ea_TLSHandshakeHash(req_p,
                                   protocol,
                                   resMD5_a,
                                   hashMsgMD5_a,
                                   md5Obj_p,
                                   md5Obj_p->residualLength,
                                   resSHA1_a,
                                   hashMsgSHA1_a,
                                   sha1Obj_p,
                                   sha1Obj_p->residualLength,
                                   resMD5PRF_a,
                                   resSHA1PRF_a,
                                   key_p,
                                   keyLength,
                                   roleStr_a);
   
      CHECK_RETURN(ret);
   
      /* create a struct to carry results to the result handler */
      CREATE_UOBJECT(hashResultStruct_p, N8_TLSHandshakeHashResults_t,
                      ret);
      hashResultStruct_p->protocol = protocol;
      hashResultStruct_p->MD5Result_p = md5Result_p;
      hashResultStruct_p->SHA1Result_p = sha1Result_p;
      hashResultStruct_p->resMD5_p = resMD5_p;
      hashResultStruct_p->resSHA1_p = resSHA1_p;
      hashResultStruct_p->resMD5PRF_p = resMD5PRF_p;
      hashResultStruct_p->resSHA1PRF_p = resSHA1PRF_p;
   
      req_p->postProcessingData_p = (void *) hashResultStruct_p;
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
}

/*****************************************************************************
 * n8_HashCompleteMessage_req
 *****************************************************************************/
/** @ingroup n8_hash
 * @brief Generate the request for N8_HashCompleteMessage
 *
 *  @param obj_p               RW:  Hash object pointer.
 *  @param msg_p               RO:  Message to be hashed.
 *  @param msgLength           RO:  Length of message to be hashed.
 *  @param result_p            RW:  Destination of hash results.
 *  @param resultHandler       RO:  Pointer to callback function.  If NULL
 *                                  the default is used.
 *  @param req_pp              RW:  Pointer to request pointer.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_INVALID_OBJECT if req_pp is NULL<br>
 *    N8_INVALID_INPUT_SIZE if the msgLength is greater than N8_MAX_HASH_LENGTH
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t n8_HashCompleteMessage_req(N8_HashObject_t   *obj_p,
                                       const N8_Buffer_t *msg_p,
                                       const unsigned int msgLength,
                                       N8_Buffer_t       *result_p,
                                       const void        *resultHandler,
                                       API_Request_t    **req_pp)
{
   N8_Status_t ret = N8_STATUS_OK;
   
   N8_Buffer_t *hashMsg_p = NULL;
   unsigned long hashMsg_a;
   uint32_t     res_a;
   N8_Buffer_t *res_p = NULL;
   int nBytes;

   do
   {
      CHECK_OBJECT(req_pp, ret);
      *req_pp = NULL;           /* set the return request to NULL in case we
                                 * have to bail out. */
      ret = N8_preamble();
      CHECK_RETURN(ret);

      if (resultHandler == NULL)
      {
         resultHandler = resultHandlerGeneric;
      }
      if (msgLength > N8_MAX_HASH_LENGTH)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      CHECK_OBJECT(obj_p, ret);
      if (msgLength > 0)
      {
         CHECK_OBJECT(msg_p, ret);
      }
      CHECK_OBJECT(result_p, ret);
      CHECK_STRUCTURE(obj_p->structureID, N8_HASH_STRUCT_ID, ret);

      /* comput the size for the results and the incoming message. */
      nBytes = (NEXT_WORD_SIZE(obj_p->hashSize) + 
                NEXT_WORD_SIZE(msgLength));

      /* create the command blocks and do the hash. */
      ret = createEARequestBuffer(req_pp,
                                  obj_p->unitID,
                                  N8_CB_EA_HASHCOMPLETEMESSAGE_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      /* allocate space for the chip to place the result */
      res_a = (*req_pp)->qr.physicalAddress + (*req_pp)->dataoffset;
      res_p = (N8_Buffer_t *) ((int)(*req_pp) + (*req_pp)->dataoffset);
      memset(res_p, 0, nBytes);

      hashMsg_a = res_a + NEXT_WORD_SIZE(obj_p->hashSize);
      hashMsg_p = res_p + NEXT_WORD_SIZE(obj_p->hashSize);

      /* Copy in message to hash */
      memcpy(hashMsg_p, msg_p, msgLength);

      DBG(("hash length: %d\n", msgLength));

      (*req_pp)->copyBackFrom_p = res_p;
      (*req_pp)->copyBackTo_p = (void *) result_p;
      (*req_pp)->copyBackSize = obj_p->hashSize;
      
      ret = cb_ea_hashCompleteMessage(*req_pp,
                                      (*req_pp)->EA_CommandBlock_ptr,
                                      obj_p, 
                                      hashMsg_a,
                                      msgLength,
                                      res_a);
      CHECK_RETURN(ret);
   } while (FALSE);
   return ret;
} /* n8_HashCompleteMessage_req */


N8_Status_t n8_HashCompleteMessage_req_uio(N8_HashObject_t *obj_p,
                                       struct uio          *msg_p,
                                       const unsigned int   msgLength,
                                       N8_Buffer_t         *result_p,
                                       const void          *resultHandler,
                                       API_Request_t      **req_pp)
{
   N8_Status_t ret = N8_STATUS_OK;
   
   N8_Buffer_t *hashMsg_p = NULL;
   unsigned long hashMsg_a;
   uint32_t     res_a;
   N8_Buffer_t *res_p = NULL;
   int nBytes;

   do
   {
      CHECK_OBJECT(req_pp, ret);
      *req_pp = NULL;           /* set the return request to NULL in case we
                                 * have to bail out. */
      ret = N8_preamble();
      CHECK_RETURN(ret);

      if (resultHandler == NULL)
      {
         resultHandler = resultHandlerGeneric;
      }
      if (msgLength > N8_MAX_HASH_LENGTH)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      CHECK_OBJECT(obj_p, ret);
      if (msgLength > 0)
      {
         CHECK_OBJECT(msg_p, ret);
      }
      CHECK_OBJECT(result_p, ret);
      CHECK_STRUCTURE(obj_p->structureID, N8_HASH_STRUCT_ID, ret);

      /* comput the size for the results and the incoming message. */
      nBytes = (NEXT_WORD_SIZE(obj_p->hashSize) + 
                NEXT_WORD_SIZE(msgLength));

      /* create the command blocks and do the hash. */
      ret = createEARequestBuffer(req_pp,
                                  obj_p->unitID,
                                  N8_CB_EA_HASHCOMPLETEMESSAGE_NUMCMDS,
                                  resultHandler, nBytes);
      CHECK_RETURN(ret);

      /* allocate space for the chip to place the result */
      res_a = (*req_pp)->qr.physicalAddress + (*req_pp)->dataoffset;
      res_p = (N8_Buffer_t *) ((int)(*req_pp) + (*req_pp)->dataoffset);
      memset(res_p, 0, nBytes);

      hashMsg_a = res_a + NEXT_WORD_SIZE(obj_p->hashSize);
      hashMsg_p = res_p + NEXT_WORD_SIZE(obj_p->hashSize);

      /* Copy in message to hash */
      cuio_copydata(msg_p, 0, msgLength, hashMsg_p);

      DBG(("hash length: %d\n", msgLength));
      DBG(("cleartext: %08x|%08x|%08x|%08x\n",
			      *(((uint32_t *)hashMsg_p)),
			      *(((uint32_t *)hashMsg_p)+1),
			      *(((uint32_t *)hashMsg_p)+2),
			      *(((uint32_t *)hashMsg_p)+3)));

      (*req_pp)->copyBackFrom_p = res_p;
      (*req_pp)->copyBackTo_p = result_p;
      (*req_pp)->copyBackTo_uio = NULL;
      (*req_pp)->copyBackSize = obj_p->hashSize;

      ret = cb_ea_hashCompleteMessage(*req_pp,
                                      (*req_pp)->EA_CommandBlock_ptr,
                                      obj_p, 
                                      hashMsg_a,
                                      msgLength,
                                      res_a);
      CHECK_RETURN(ret);
   } while (FALSE);
   return ret;
} /* n8_HashCompleteMessage_req */
/*****************************************************************************
 * N8_HashCompleteMessage
 ****************************************************************************/
/** @ingroup n8_hash
 * @brief Hash a complete message.
 *
 * To hash a message under 18 K bytes long use this function.  Any 
 * hash greater than that should make multiple calls to 
 * N8_HashPartial.
 *
 * @param obj_p      RW: Hash object to store intermediate results.
 * @param msg_p      RO: Pointer to the message to be hashed.
 * @param msgLength  RO: Length of message to be hashed.
 * @param result_p   WO: Pointer to hashed message.
 * @param event_p    RO: Pointer to event for async calls.  Null for
 *                       synchronous call.
 * @par Externals:
 *    None
 *
 * @return 
 *    Status code of type N8_Status.
 *
 * @par Errors:
 *    N8_STATUS_OK:           A-OK<br>
 *    N8_INVALID_INPUT_SIZE:  message passed was larger than allowed
 *                            limit of N8_MAX_HASH_LENGTH<br>
 *    N8_INVALID_OBJECT       Invalid or null input parameter.
 * @par Assumptions:
 *    None
 ****************************************************************************/
N8_Status_t N8_HashCompleteMessage(N8_HashObject_t   *obj_p,
                                   const N8_Buffer_t *msg_p,
                                   const unsigned int msgLength,
                                   N8_Buffer_t       *result_p,
                                   N8_Event_t        *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;

   DBG(("N8_HashCompleteMessage\n"));
   do
   {
      /* generate the request */
      ret = n8_HashCompleteMessage_req(obj_p, msg_p, msgLength, result_p,
                                       NULL, /* result handler -- use default */
                                       &req_p);
      if (req_p == NULL)
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   }
   while (FALSE);
   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_HashCompleteMessage - OK\n"));
   return ret;
} /* N8_HashCompleteMessage */


/* same as above, but for uio iovec i/o */
N8_Status_t N8_HashCompleteMessage_uio(N8_HashObject_t *obj_p,
			           struct uio          *msg_p,
                                   const unsigned int   msgLength,
                                   N8_Buffer_t         *result_p,
                                   N8_Event_t          *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;

   DBG(("N8_HashCompleteMessage\n"));
   do
   {
      /* generate the request */
      ret = n8_HashCompleteMessage_req_uio(obj_p, msg_p, msgLength, result_p,
                                       NULL, /* result handler -- use default */
                                       &req_p);
      if (req_p == NULL)
      {
         if (event_p != NULL)
         {
            N8_SET_EVENT_FINISHED(event_p, N8_EA);
         }
         break;
      }
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   }
   while (FALSE);
   /* clean up resources */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_HashCompleteMessage - OK\n"));
   return ret;
} /* N8_HashCompleteMessage */

