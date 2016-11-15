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

static char const n8_id[] = "$Id: n8_packet.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_packet.c
 *  @brief Contains packet functions.
 *
 * Functions:
 *          N8_PacketInitializeMemory
 *               Initializes (makes ready for use in packet operations) the
 *               packet object. This version uses supplied kernel memory
 *               buffers. 
 *          N8_PacketInitialize
 *               Initializes (makes ready for use in packet operations) the
 *               packet object. 
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 *
 * 08/18/03 brr   Combine Encrypt/Decrypt command block generators for SSL,
 *                TLS, & IPsec.
 * 07/30/03 bac   Removed obsolete and confusing code referring to chained
 *                requests in N8_PacketInitializeMemory.
 * 07/28/03 brr   Removed obsolete #ifdefs. (Bug 918)
 * 07/01/03 brr   Added option to use no hashing algorithm.
 * 05/20/03 brr   Modified N8_PacketInitialize to setup function pointers &
 *                lengths used in the Encrypt/Decrypt operations. Eliminated
 *                several switch statements from Encrypt/Decrypt operations.
 * 03/05/03 brr   Allow mode = N8_PACKETMEMORY_REQUEST for IPsec.
 * 08/06/02 bac   Fixed Bug #842 -- user's cipher info was being modified.
 * 07/16/02 arh   Fixed N8_PacketInitializeMemory: 1) check mode parameter
 *                for correctness; 2) to return an error if protocol is 
 *                N8_IPSEC & mode != N8_PACKETMEMORY_NONE. (Fix Bug #815)
 * 06/14/02 hml   Added new call N8_PacketInitializeMemory. The old packet
 *                init call is still supported.
 * 06/11/02 bac   Removed #ifdef'd reference to obsolete and missing
 *                n8_validateContext. 
 * 06/10/02 hml   Initialize the request field in the packet to NULL.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 04/04/02 msz   BUG 520, have N8_PacketInitialize check for weak keys.
 * 03/26/02 hml   Calls n8_validateUnit in packet init routine (BUG 643).
 * 03/25/02 hml   Fix for bug 642.  Initialing a packet for ARC4 with a 
 *                NULL context returns N8_UnallocatedContext.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 03/14/02 hml   Cleaned and commented.
 * 02/12/02 hml   Added N8_PacketBuffersSet.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 01/31/02 brr   Eliminated the memory allocation for postProcessingData.
 * 01/22/02 bac   Added code to use software to do SSL and HMAC precomputes.
 * 01/22/02 bac   Removed loading of context.  It is deferred until the packet
 *                object is used the first time.
 * 01/21/02 bac   Changes to support new kernel allocation scheme.
 * 01/14/02 bac   Changed use of loadContext_req to take a physical and virtual
 *                address to a pre-formed context.  This allows the use of the
 *                results of a previous initialization to work properly.
 * 01/12/02 bac   Removed all blocking calls within a single API method.  This
 *                required restructuring the N8_PacketInitialize call and taking
 *                advantage of Queue Manager sub-requests.  (BUG #313)
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/16/01 bac   Fixed a memory leak in handleIPsec.
 * 11/14/01 bac   Cleaned up debugging messages.
 * 11/12/01 hml   Updated initMD5 and initSha1 calls.
 * 11/12/01 hml   Changed PI_PROTOCOL_* to N8_PI_PROTOCOL_*.
 * 11/09/01 hml   Added structure verification code.
 * 11/05/01 bac   Replaced reference to SINGLE_CHIP with correct unitID.  Fixed
 *                a memory leak in handleHmac.
 * 10/30/01 bac   Changed to not block on N8_PacketInitialize unless context is
 *                actually being used.
 * 10/30/01 bac   Fixes to defer loading of the context during a call to
 *                N8_PacketInitialize.  This was required due to the need for
 *                the partial hash IV values to be byte-swapped before loading
 *                into the context.  The result handler is the only place
 *                available to do the byte-swapping, but it occurs after the
 *                context is loaded.  Thus the need to do the load in a separate
 *                request.  Please note this causes N8_PacketInitialize to block
 *                while the partial hashes are computed IFF a context is to be
 *                loaded. 
 * 10/19/01 hml   Fixed compiler warnings.
 * 10/17/01 mel   Cleaned: deleted unnecessary createEARequestBuffer calls.
 * 10/11/01 bac   Fixed a bug where kernel memory was being freed incorrectly.
 * 10/05/01 mel   Added comments to handleIPsec. 
 * 10/04/01 mel   IPsec code optimization. Added handleIPsec function 
 * 10/04/01 mel   SSL/TLS code optimization. Added handleHMAC function 
 * 09/24/01 mel   Code optimization. 
 * 09/20/01 bac   The interface to the command block generators changed and now
 *                accept the command block buffer.  All calls to cb_ea methods
 *                changed herein.
 * 09/12/01 mel   Increased speed by using initMD5/initSHA1 directly instead 
 *                of N8_HashInitialize. Added event parameter to N8_PacketInitialize.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/30/01 bac   Pass chip id to createEARequestBuffer.
 * 07/02/01 mel   Fixed comments.
 * 06/28/01 bac   Changes to get event handling to work properly.
 * 06/25/01 bac   Memory management changes.
 * 06/20/01 mel   Corrected use of kernel memory.
 * 05/31/01 mel   Changed keys and IVs to be a char[] instead of uint32_t[].
 * 05/30/01 bac   Doxygenation
 * 05/23/01 bac   Changed macSecret to be a char[] instead of uint32_t[].
 * 05/23/01 bac   Fixed a bug where there was a ; after an if.
 * 05/21/01 bac   Converted to use N8_ContextHandle_t and N8_Packet_t
 *                with integrated cipher and hash packet.
 * 05/19/01 bac   Formatting changes.  Pass NULL not 0 when appropriate.
 * 05/18/01 bac   Set contextIndex to 0 even when not used for safety.
 * 05/18/01 mel   Moved context index validation from configuration block 
 *                to protocol block.
 * 05/18/01 bac   Reformat to standard.  Cleaned up memory allocation/free.
 * 05/11/01 bac   Naming standardization.
 * 05/11/01 bac   Slight formatting changes.  Changed ProtocolConfiguration
 *                to use the new PROTOCOL_CIPHER_HASH macro.
 * 05/03/01 bac   Replaced integer use of NULL with 0.
 * 04/18/01 mel   Original version.
 ****************************************************************************/
/** @defgroup n8_packet API Packet-Level functions
 */


#include "n8_common.h"          /* common definitions */
#include "n8_pub_errors.h"      /* Errors definition */
#include "n8_enqueue_common.h"  /* common definitions for enqueue */
#include "n8_cb_ea.h"
#include "n8_util.h"
#include "n8_hash.h"
#include "n8_key_works.h"
#include "n8_packet.h"
#include "n8_API_Initialize.h"
#include "n8_pub_buffer.h"
#include "n8_precompute.h"

typedef struct 
{
   N8_Buffer_t *result1_p;
   N8_Buffer_t *result2_p;
} n8_PacketResults_t;


/*****************************************************************************
 * n8_initializeSSL_req
 *****************************************************************************/
/** @ingroup n8_packet  
 * @brief Precomputes values for MD5 hashes.
 *
 *  @param cipherInfo_p        RW:  Cipher information
 *  @param packetObject_p      RO:  Pointer to packet object.  May be NULL.
 *  @param req_pp              RW:  Pointer to request pointer.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status code
 *
 * @par Errors
 *    None
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t n8_initializeSSL_req(N8_CipherInfo_t *cipherInfo_p, 
                                        N8_Packet_t  *packetObject_p,
                                        N8_Buffer_t **ctx_pp,
                                        uint32_t *ctxa_p,
                                        API_Request_t **req_pp)
{
   N8_Status_t        ret = N8_STATUS_OK; 
   API_Request_t     *req_p = NULL;

   do
   {
      CHECK_OBJECT(cipherInfo_p, ret);
      CHECK_OBJECT(packetObject_p, ret);
      /* initialize set the return request pointer to NULL.  it wil be reset
       * with an actual value later if necessary. */
      *req_pp = NULL;

      ret =
         n8_precompute_ssl_ipad_opad(packetObject_p->cipherInfo.macSecret,
                                     packetObject_p->cipherInfo.precompute1,
                                     packetObject_p->cipherInfo.precompute2);
      CHECK_RETURN(ret);

   } while (FALSE);
   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* n8_initializeSSL_req */
 

/*****************************************************************************
 * N8_PacketInitializeMemory
 *****************************************************************************/
/** @ingroup n8_packet
 * @brief Initializes (makes ready for use in packet operations) the packet object.
 *
 * Typically a packet object will be initialized at the beginning of an SSL,
 * TLS, or IPSec session with the cipher and hash information particular to
 * that session.  The packet object can then be used in all subsequent packet
 * operations until the session is terminated.  Note however that if a new key
 * is negotiated during the session, then the packet object must be re-initialized
 * with the new key material.
 *
 * @param packetObject_p WO:    The packet object to be initialized with the
 *                              specified information.<BR> 
 * @param contextHandle_p RO:   A valid context index as returned by
 *                              N8_AllocateContext, if Cipher = ARC4.
 *                              Optional, if Cipher = DES.<BR> 
 * @param protocol RO:          One of the values SSL, TLS, or IPSec. 
 *                              Denotes the protocol that PacketObject should 
 *                              be initialized for.<BR>
 * @param cipher RO:            If Protocol = SSL or TLS, one of the values ARC4
 *                              or  DES.  
 *                              If Protocol = IPSec, then Cipher must be DES. 
 *                              Specifies the cipher algorithm to use, and hence the 
 *                              type and format of the information in
 *                              CipherInfo_p.<BR> 
 * @param cipherInfo_p RO:      The specific information to be used in the 
 *                              initialization of the cipher algorithm. Its
 *                              contents depend on the value of ContextType, as
 *                              specified above.<BR> 
 * @param hashAlgorithm RO:     One of the values MD5, SHA-1, HMAC-MD5, HMAC-SHA-1,  
 *                              HMAC-MD5-96 or HMAC-SHA-1-96 denoting the hash 
 *                              algorithm to be used.
 * @param mode          RO:     One of the values from N8_PacketMemoryMode_t
 *
 *
 * @return 
 *    packetObject_p - initialized packet object.
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_PROTOCOL - The value of Protocol is not one of the legal
 *                          values SSL, TSL or IPSec.<BR>
 *    N8_INVALID_OBJECT   - packet object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_INVALID_CIPHER   - The value of Cipher is not one of the legal values
 *                          ARC4 or DES.<BR>
 *    N8_INVALID_HASH     - The value of HashAlgorithm is not one of the legal
 *                          values MD5,  SHA-1, HMAC-MD5, HMAC-SHA-1, HMAC-MD5-96
 *                          or HMAC-SHA-1-96<BR>
 *    N8_INVALID_VALUE    - The value of ContextIndex is not a valid context 
 *                          index, or ContextIndex is null but a context index 
 *                          is required because ARC4 is specified.<BR>
 *    N8_UNALLOCATED_CONTEXT - ContextIndex denotes a context memory index that
 *                          has not been allocated by N8_AllocateContext.<BR>
 *    N8_INVALID_KEY_SIZE -     The size of the key specified in CipherInfo_p is 
 *                          outside the valid range for the encryption algorithm 
 *                          specified in Cipher, or the size of an HMAC key 
 *                          specified in CipherInfo_p is outside the valid range 
 *                          for the hash algorithm specified in HashAlgorithm.<BR> 
 *    N8_INCONSISTENT  -        The information in  CipherInfo_p and/or its type is 
 *                          different than or inconsistent with the type 
 *                          specified by Cipher or HashAlgorithm, or the 
 *                          combination of values specified by Protocol, Cipher, 
 *                          and HashAlgorithm is invalid (for example, SSL is  
 *                          specified with HMAC-MD5, or IPSec is specified with
 *                          ARC4).<BR> 
 *    N8_MALLOC_FAILED    - memory allocation failed
 *    N8_UNIMPLEMENTED_FUNCTION - not supported protocol configuration requested
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 *    The context entry specified in this call should not be in use with any 
 *    other current  encrypt object or packet object. This condition is not
 *    checked for;  incorrect / indeterminate results and / or errors are likely
 *    in this situation.<BR>
 *    If a new key is negotiated during the session, then the packet object must
 *    be re-initialized with the new key material.<BR>
 *****************************************************************************/
N8_Status_t 
N8_PacketInitializeMemory(N8_Packet_t                *packetObject_p,
                          const N8_ContextHandle_t   *contextHandle_p,
                          const N8_Protocol_t         protocol,
                          const N8_Cipher_t           cipher,
                          const N8_CipherInfo_t            *cipherInfo_p,
                          const N8_HashAlgorithm_t    hashAlgorithm,
                          const N8_PacketMemoryMode_t mode,
                          N8_Event_t                 *event_p)

{
   API_Request_t *req_p = NULL;
   N8_Status_t ret = N8_STATUS_OK;             /* the return status: OK or ERROR */
   int         protocolConfiguration = 0;
   int         i;
   key_cblock_t key1, key2, key3;               /* Keys to be checked */
   N8_Buffer_t *ctx_p = NULL;
   uint32_t ctx_a;              /* physical address of context for
                                 * post-computation processing */
   N8_Boolean_t  unitValid;

   DBG(("N8_PacketInitializeMemory\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* verify packet object */
      CHECK_OBJECT(packetObject_p, ret);

      /* verify cipher object */
      CHECK_OBJECT(cipherInfo_p, ret);

      /* Check mode paramter & Init the mode to the user value */
      if (mode != N8_PACKETMEMORY_REQUEST && mode != N8_PACKETMEMORY_NONE)
      {
         ret = N8_INVALID_ENUM;
         break;
      }
      packetObject_p->mode =  mode;

      if (contextHandle_p != NULL)
      {
         DBG(("N8_PacketInitialize using context\n"));

         /* Make sure the context struct is valid */
         CHECK_STRUCTURE(contextHandle_p->structureID, 
                         N8_CONTEXT_STRUCT_ID, 
                         ret);
         unitValid = n8_validateUnit(contextHandle_p->unitID);
         if (!unitValid)
         {
            ret= N8_INVALID_UNIT;
            break;
         }
         memcpy(&packetObject_p->contextHandle, contextHandle_p, sizeof(N8_ContextHandle_t));
         packetObject_p->contextHandle.inUse = N8_TRUE;
         packetObject_p->unitID = contextHandle_p->unitID;
      }
      else
      {
         if (cipher == N8_CIPHER_ARC4)
         {
            /* The use of ARC4 requires a context */
            ret = N8_UNALLOCATED_CONTEXT;
            break;
         }
         else
         {
            DBG(("N8_PacketInitialize not using context\n"));
            unitValid = n8_validateUnit(cipherInfo_p->unitID);
            if (!unitValid)
            {
               ret= N8_INVALID_UNIT;
               break;
            }
            packetObject_p->contextHandle.inUse = N8_FALSE;
            packetObject_p->contextHandle.index = 0xFFFF;
            packetObject_p->unitID = cipherInfo_p->unitID;
         }
      }

      /* copy the supplied cipher info into the packet object */
      memcpy(&packetObject_p->cipherInfo, cipherInfo_p, sizeof(N8_CipherInfo_t));

      /* verify cipher */
      switch (cipher)
      {
         case   N8_CIPHER_ARC4:
                     
            /* verify key size */
            if ((packetObject_p->cipherInfo.keySize < 1) ||
                (packetObject_p->cipherInfo.keySize > ARC4_KEY_SIZE_BYTES_MAX))
            {
               DBG(("Key size specified for ARC4 is outside the"
                    " valid range:  %d\n",
                    packetObject_p->cipherInfo.keySize));
               DBG(("N8_PacketInitialize - return Error\n"));
               ret = N8_INVALID_KEY_SIZE;
               break;
            }
            packetObject_p->ctxLoadFcn = &cb_ea_loadARC4KeyToContext;
            packetObject_p->ctxLoadCmds = N8_CB_EA_LOADARC4KEYTOCONTEXT_NUMCMDS;
            break;
    
         case   N8_CIPHER_DES:
            /* verify key size */
    
            if (packetObject_p->cipherInfo.keySize != DES_KEY_SIZE_BYTES)
            {
               DBG(("Key size specified for DES is outside "
                    "the valid range: %d\n",
                    packetObject_p->cipherInfo.keySize));
               DBG(("N8_PacketInitialize - return Error\n"));
               ret = N8_INVALID_KEY_SIZE;
               break;
            }

            /* build keys for parity verification */
            /* force key parity */
            for (i=0 ; i < sizeof(key_cblock_t); i++)
            {
               key1[i] = packetObject_p->cipherInfo.key1[i];
               key2[i] = packetObject_p->cipherInfo.key2[i];
               key3[i] = packetObject_p->cipherInfo.key3[i];
            }
            if (checkKeyParity(&key1) == FALSE)
            {
               forceParity(&key1);
               for (i=0 ; i < sizeof(key_cblock_t); i++)
               {
                  packetObject_p->cipherInfo.key1[i] = key1[i];
               }
            }
            if (checkKeyParity(&key2) == FALSE)
            {
               forceParity(&key2);
               for (i=0 ; i < sizeof(key_cblock_t); i++)
               {
                  packetObject_p->cipherInfo.key2[i] = key2[i];
               }
            }
            if (checkKeyParity(&key3) == FALSE)
            {
               forceParity(&key3);
               for (i=0 ; i < sizeof(key_cblock_t); i++)
               {
                  packetObject_p->cipherInfo.key3[i] = key3[i];
               }
            }
    
            /* check key1 and key2 for weakness */
            if (checkKeyForWeakness(&key1) == N8_TRUE ||
                checkKeyForWeakness(&key2) == N8_TRUE ||
                checkKeyForWeakness(&key3) == N8_TRUE)
            {
               DBG(("Weak key\nN8_PacketInitialize - return Error\n"));
               ret = N8_WEAK_KEY;
               break;
            }
            packetObject_p->ctxLoadFcn = &cb_ea_loadDESKeyToContext;
            packetObject_p->ctxLoadCmds = N8_CB_EA_LOADDESKEYTOCONTEXT_NUMCMDS;
            break;

         default:
            /* invalid cipher */
            DBG(("Invalid cipher\n"));
            DBG(("N8_PacketInitialize - return Error\n"));
            ret = N8_INVALID_CIPHER;
            break;
      }
      CHECK_RETURN(ret);

      /* create request buffer */
      /* protocol in use */
      packetObject_p->packetProtocol = protocol;
      /* encription algorithm */
      packetObject_p->packetCipher = cipher;
      /* hash algorithm */
      packetObject_p->packetHashAlgorithm = hashAlgorithm;
      /* verify protocol configuration */
      protocolConfiguration = PROTOCOL_CIPHER_HASH(protocol, cipher, hashAlgorithm);

      /* after the memcpy, ensure we aren't pointing to dynamically allocated
         space the user set up in the cipher info. */
      packetObject_p->cipherInfo.hmac_key = NULL;

      /* verify hash algorithm and HMAC keys if appropriate */
      switch (hashAlgorithm)
      {
         case N8_MD5:
         case N8_SHA1:
         case N8_HASH_NONE:
            break;
         case N8_HMAC_SHA1:
         case N8_HMAC_SHA1_96:
         case N8_HMAC_MD5:
         case N8_HMAC_MD5_96:
            if ((packetObject_p->cipherInfo.hmacKeyLength < 0) ||
                (packetObject_p->cipherInfo.hmacKeyLength > N8_MAX_HASH_LENGTH))
            {
               ret = N8_INVALID_KEY_SIZE;
            }
            break;
         default:
            /* invalid hash algorithm */
            DBG(("Invalid hash algorithm\n"));
            DBG(("N8_PacketInitialize - return Error\n"));
            ret = N8_INVALID_HASH;
            break;

      }
      CHECK_RETURN(ret);


      /* verify protocol */
      switch (protocol)
      {
         case N8_PROTOCOL_SSL:
            packetObject_p->encCommands = N8_CB_EA_SSLENCRYPTAUTHENTICATE_NUMCMDS;
            packetObject_p->decCommands = N8_CB_EA_SSLDECRYPTVERIFY_NUMCMDS;
            packetObject_p->SSLTLScmdFcn = &cb_ea_SSL;
            break;
         case N8_PROTOCOL_TLS:
            packetObject_p->encCommands = N8_CB_EA_TLSENCRYPTAUTHENTICATE_NUMCMDS;
            packetObject_p->decCommands = N8_CB_EA_TLSDECRYPTVERIFY_NUMCMDS;
            packetObject_p->SSLTLScmdFcn = &cb_ea_TLS;
            break;
         case N8_PROTOCOL_IPSEC:
            if (cipher != N8_CIPHER_DES)
            {
               ret = N8_INVALID_CIPHER;
            }
            break;
         default:
            /* invalid protocol */
            DBG(("Invalid protocol\n"));
            DBG(("N8_PacketInitialize - return Error\n"));
            ret = N8_INVALID_PROTOCOL;
            break;
      }
      CHECK_RETURN(ret);


      /* initialize the hash packet */
      n8_setInitialIVs(&packetObject_p->hashPacket,
                       hashAlgorithm,
                       packetObject_p->unitID);
      packetObject_p->hashPacket.hashSize = N8_GetHashLength(hashAlgorithm);

      switch (protocolConfiguration)
      {
         case PACKET_SSL_ARC4_MD5:
            /* Initialize for use with MD5 encryption/decryption */
            ret = n8_initializeSSL_req(&packetObject_p->cipherInfo,
                                       packetObject_p,
                                       &ctx_p,
                                       &ctx_a,
                                       &req_p);
            packetObject_p->encOpCode = EA_Cmd_SSL30_ARC4_MD5_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_SSL30_ARC4_MD5_Decrypt;
            break;

         case PACKET_SSL_ARC4_SHA1:
            /* Initialize for use with ARC4 encryption/decryption */
            ret = n8_initializeSSL_req(&packetObject_p->cipherInfo,
                                       packetObject_p,
                                       &ctx_p,
                                       &ctx_a,
                                       &req_p);
            packetObject_p->encOpCode = EA_Cmd_SSL30_ARC4_SHA1_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_SSL30_ARC4_SHA1_Decrypt;
            break;
    
         case PACKET_SSL_DES_MD5:
            /* Initialize for use with DES encryption/decryption */
            if (packetObject_p->contextHandle.inUse == N8_TRUE)
            {
               ret = n8_initializeSSL_req(&packetObject_p->cipherInfo, 
                                          packetObject_p,
                                          &ctx_p,
                                          &ctx_a,
                                          &req_p);
            }
            packetObject_p->encOpCode = EA_Cmd_SSL30_3DES_MD5_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_SSL30_3DES_MD5_Decrypt;
            break;
    
         case PACKET_SSL_DES_SHA1:
            /* Initialize for use with DES encryption/decryption */
            if (packetObject_p->contextHandle.inUse == N8_TRUE)
            {
               ret = n8_initializeSSL_req(&packetObject_p->cipherInfo, 
                                          packetObject_p,
                                          &ctx_p,
                                          &ctx_a,
                                          &req_p);
            }
            packetObject_p->encOpCode = EA_Cmd_SSL30_3DES_SHA1_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_SSL30_3DES_SHA1_Decrypt;
            break;
    
         case PACKET_TLS_ARC4_HMAC_MD5:
            ret = n8_initializeHMAC_req(cipherInfo_p->hmac_key,
                                        packetObject_p->cipherInfo.hmacKeyLength, 
                                        &packetObject_p->hashPacket,
                                        NULL,
                                        &ctx_p,
                                        &ctx_a,
                                        &req_p);
            packetObject_p->encOpCode = EA_Cmd_TLS10_ARC4_MD5_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_TLS10_ARC4_MD5_Decrypt;
            break;

         case PACKET_TLS_ARC4_HMAC_SHA1:
            ret = n8_initializeHMAC_req(cipherInfo_p->hmac_key,
                                        packetObject_p->cipherInfo.hmacKeyLength, 
                                        &packetObject_p->hashPacket,
                                        NULL,
                                        &ctx_p,
                                        &ctx_a,
                                        &req_p);
            packetObject_p->encOpCode = EA_Cmd_TLS10_ARC4_SHA1_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_TLS10_ARC4_SHA1_Decrypt;
            break;

         case PACKET_TLS_DES_HMAC_MD5:
            ret = n8_initializeHMAC_req(cipherInfo_p->hmac_key,
                                        packetObject_p->cipherInfo.hmacKeyLength, 
                                        &packetObject_p->hashPacket,
                                        NULL,
                                        &ctx_p,
                                        &ctx_a,
                                        &req_p);
            packetObject_p->encOpCode = EA_Cmd_TLS10_3DES_MD5_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_TLS10_3DES_MD5_Decrypt;
            break;
    
         case PACKET_TLS_DES_HMAC_SHA1:
            ret = n8_initializeHMAC_req(cipherInfo_p->hmac_key,
                                        packetObject_p->cipherInfo.hmacKeyLength, 
                                        &packetObject_p->hashPacket,
                                        NULL,
                                        &ctx_p,
                                        &ctx_a,
                                        &req_p);
            packetObject_p->encOpCode = EA_Cmd_TLS10_3DES_SHA1_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_TLS10_3DES_SHA1_Decrypt;
            break;
    
         case PACKET_IPSEC_DES_HMAC_MD5_96:
            ret = n8_initializeHMAC_req(cipherInfo_p->hmac_key,
                                        packetObject_p->cipherInfo.hmacKeyLength, 
                                        &packetObject_p->hashPacket,
                                        &packetObject_p->cipherInfo,
                                        &ctx_p,
                                        &ctx_a,
                                        &req_p);
            packetObject_p->encOpCode = EA_Cmd_ESP_3DES_MD5_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_ESP_3DES_MD5_Decrypt;
            break;

         case PACKET_IPSEC_DES_HMAC_SHA1_96:
            ret = n8_initializeHMAC_req(cipherInfo_p->hmac_key,
                                        packetObject_p->cipherInfo.hmacKeyLength, 
                                        &packetObject_p->hashPacket,
                                        &packetObject_p->cipherInfo,
                                        &ctx_p,
                                        &ctx_a,
                                        &req_p);
            packetObject_p->encOpCode = EA_Cmd_ESP_3DES_SHA1_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_ESP_3DES_SHA1_Decrypt;
            break;
    
         case PACKET_IPSEC_DES_HASH_NONE:
            packetObject_p->encOpCode = EA_Cmd_3DES_CBC_Encrypt;
            packetObject_p->decOpCode = EA_Cmd_ESP_3DES_SHA1_Decrypt;
            break;
    
         default:
            /* invalid protocol configuration */
            DBG(("Invalid protocol configuration\n"));
            DBG(("N8_PacketInitialize - return Error\n"));
            ret = N8_INCONSISTENT;
            break;
      }
      CHECK_RETURN(ret);


      /* set up the minimum length & macLength */
      switch (protocolConfiguration)
      {
         case PACKET_SSL_DES_MD5:
         case PACKET_TLS_DES_HMAC_MD5:
            packetObject_p->minLength = N8_DES_MD5_MIN_LENGTH;
            packetObject_p->macLength = MD5_HASH_RESULT_LENGTH;
            break;
         case PACKET_SSL_DES_SHA1:
         case PACKET_TLS_DES_HMAC_SHA1:
            packetObject_p->minLength = N8_DES_SHA1_MIN_LENGTH;
            packetObject_p->macLength = SHA1_HASH_RESULT_LENGTH;
            break;
         case PACKET_SSL_ARC4_MD5:
         case PACKET_TLS_ARC4_HMAC_MD5:
            packetObject_p->minLength = N8_ARC4_MD5_MIN_LENGTH;
            packetObject_p->macLength = MD5_HASH_RESULT_LENGTH;
            break;
         case PACKET_SSL_ARC4_SHA1:
         case PACKET_TLS_ARC4_HMAC_SHA1:
            packetObject_p->minLength = N8_ARC4_SHA1_MIN_LENGTH;
            packetObject_p->macLength = SHA1_HASH_RESULT_LENGTH;
            break;
         default:
            break;
      }
      CHECK_RETURN(ret);

      packetObject_p->contextLoadNeeded = packetObject_p->contextHandle.inUse;
   } while (FALSE);

   if (ret == N8_STATUS_OK)
   {
      /* set the structure pointer to the correct state */
      packetObject_p->structureID = N8_PACKET_STRUCT_ID;
   }

   DBG(("N8_PacketInitialize - FINISHED\n"));

   return ret;
} /* N8_PacketInitializeMemory */

/*****************************************************************************
 * N8_PacketInitialize
 *****************************************************************************/
/** @ingroup n8_packet
 * @brief Initializes (makes ready for use in packet operations) the packet object.
 *
 * Typically a packet object will be initialized at the beginning of an SSL,
 * TLS, or IPSec session with the cipher and hash information particular to
 * that session.  The packet object can then be used in all subsequent packet
 * operations until the session is terminated.  Note however that if a new key
 * is negotiated during the session, then the packet object must be re-initialized
 * with the new key material.
 *
 * @param packetObject_p WO:    The packet object to be initialized with the
 *                              specified information.<BR> 
 * @param contextHandle_p RO:   A valid context index as returned by
 *                              N8_AllocateContext, if Cipher = ARC4.
 *                              Optional, if Cipher = DES.<BR> 
 * @param protocol RO:          One of the values SSL, TLS, or IPSec. 
 *                              Denotes the protocol that PacketObject should 
 *                              be initialized for.<BR>
 * @param cipher RO:            If Protocol = SSL or TLS, one of the values ARC4
 *                              or  DES.  
 *                              If Protocol = IPSec, then Cipher must be DES. 
 *                              Specifies the cipher algorithm to use, and hence the 
 *                              type and format of the information in
 *                              CipherInfo_p.<BR> 
 * @param cipherInfo_p RO:      The specific information to be used in the 
 *                              initialization of the cipher algorithm. Its
 *                              contents depend on the value of ContextType, as
 *                              specified above.<BR> 
 * @param hashAlgorithm RO:     One of the values MD5, SHA-1, HMAC-MD5, HMAC-SHA-1,  
 *                              HMAC-MD5-96 or HMAC-SHA-1-96 denoting the hash 
 *                              algorithm to be used.
 *
 *
 * @return 
 *    packetObject_p - initialized packet object.
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_PROTOCOL - The value of Protocol is not one of the legal
 *                          values SSL, TSL or IPSec.<BR>
 *    N8_INVALID_OBJECT   - packet object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_INVALID_CIPHER   - The value of Cipher is not one of the legal values
 *                          ARC4 or DES.<BR>
 *    N8_INVALID_HASH     - The value of HashAlgorithm is not one of the legal
 *                          values MD5,  SHA-1, HMAC-MD5, HMAC-SHA-1, HMAC-MD5-96
 *                          or HMAC-SHA-1-96<BR>
 *    N8_INVALID_VALUE    - The value of ContextIndex is not a valid context 
 *                          index, or ContextIndex is null but a context index 
 *                          is required because ARC4 is specified.<BR>
 *    N8_UNALLOCATED_CONTEXT - ContextIndex denotes a context memory index that
 *                          has not been allocated by N8_AllocateContext.<BR>
 *    N8_INVALID_KEY_SIZE -     The size of the key specified in CipherInfo_p is 
 *                          outside the valid range for the encryption algorithm 
 *                          specified in Cipher, or the size of an HMAC key 
 *                          specified in CipherInfo_p is outside the valid range 
 *                          for the hash algorithm specified in HashAlgorithm.<BR> 
 *    N8_INCONSISTENT  -        The information in  CipherInfo_p and/or its type is 
 *                          different than or inconsistent with the type 
 *                          specified by Cipher or HashAlgorithm, or the 
 *                          combination of values specified by Protocol, Cipher, 
 *                          and HashAlgorithm is invalid (for example, SSL is  
 *                          specified with HMAC-MD5, or IPSec is specified with
 *                          ARC4).<BR> 
 *    N8_MALLOC_FAILED    - memory allocation failed
 *    N8_UNIMPLEMENTED_FUNCTION - not supported protocol configuration requested
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 *    The context entry specified in this call should not be in use with any 
 *    other current  encrypt object or packet object. This condition is not
 *    checked for;  incorrect / indeterminate results and / or errors are likely
 *    in this situation.<BR>
 *    If a new key is negotiated during the session, then the packet object must
 *    be re-initialized with the new key material.<BR>
 *****************************************************************************/
N8_Status_t N8_PacketInitialize(N8_Packet_t                *packetObject_p,
                                const N8_ContextHandle_t   *contextHandle_p,
                                const N8_Protocol_t         protocol,
                                const N8_Cipher_t           cipher,
                                const N8_CipherInfo_t            *cipherInfo_p,
                                const N8_HashAlgorithm_t    hashAlgorithm,
                                N8_Event_t                 *event_p)

{
   N8_Status_t retCode;

   retCode = N8_PacketInitializeMemory(packetObject_p, 
                                       contextHandle_p,
                                       protocol,
                                       cipher,
                                       cipherInfo_p,
                                       hashAlgorithm,
                                       N8_PACKETMEMORY_NONE,
                                       event_p);
   return retCode;
}
