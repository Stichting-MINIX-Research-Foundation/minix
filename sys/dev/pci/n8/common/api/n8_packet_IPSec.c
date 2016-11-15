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

static char const n8_id[] = "$Id: n8_packet_IPSec.c,v 1.2 2013/12/09 09:35:17 wiz Exp $";
/*****************************************************************************/
/** @file n8_packet_IPSec.c
 *  @brief Contains IPSec packet level interface functions.
 *
 * Functions:
 *          N8_IPSecEncryptAuthenticate  -   Encrypt and authenticate an entire 
 *                                           IPSec record.
 *
 *          N8_IPSecDecryptVerify        -   Decrypt and verify an entire IPSec
 *                                           record.
 *
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/18/03 brr   Combine Encrypt/Decrypt command block generators for SSL,
 *                TLS, & IPsec.
 * 07/28/03 brr   Removed obsolete #ifdefs. (Bug 918)
 * 07/01/03 brr   Added option to use no hashing algorithm.
 * 05/27/03 brr   Removed N8_preamble call since N8_PacketInitialize must be
 *                called prior to Encrypt/Decrypt operations.
 * 05/20/03 brr   Modified N8_PacketInitialize to setup function pointers &
 *                lengths used in the Encrypt/Decrypt operations. Eliminated
 *                several switch statements from Encrypt/Decrypt operations.
 * 02/23/03 jpw   Don't copy computedHMAC_p if ptr is NULL. Yields better 
 *		  memory and CPU utilization. 
 * 03/04/03 brr   Added support for N8_PACKETMEMORY_REQUEST mode which avoids
 *                copying the packet into a kernel buffer.
 * 08/05/02 bac   Cosmetic changes to enhance readability.
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 04/19/02 bac   Use IPSEC_DATA_BLOCK_SIZE instead of IPSEC_DATA_LENGTH_MIN,
 *                where checking the data length.  Fixes BUG #716.
 * 04/01/02 brr   Validate computedHMAC_p before use in N8_IPSecDecryptVerify.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 01/31/02 brr   Eliminated the memory allocation for postProcessingData.
 * 01/23/02 bac   Deferred loading of contexts.
 * 12/11/01 mel   Fixed bug #397: addBlockToFree(postData_p..) and deleted 
 *                freeing postData_p from result handlers.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/12/01 hml   Added structure verification (bug 261).
 * 10/30/01 bac   Trivial changes to spell IPsec as preferred.
 * 10/02/01 bac   Added use of RESULT_HANDLER_WARNING in all result handlers.
 * 10/01/01 hml   Added multiunit functionality.
 * 09/20/01 bac   The interface to the command block generators changed and now
 *                accept the command block buffer.  All calls to cb_ea methods
 *                changed herein.
 * 09/06/01 bac   Added include of <string.h> to silence warning.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/30/01 bac   Pass chip id to createEARequestBuffer.
 * 07/11/01 mel   Added use of NEXT_WORD_SIZE to ensure split kernel memory is
 *                word-aligned.
 * 06/25/01 bac   Kernel mem mgt changes.  Standardization.
 * 05/30/01 bac   Removed unused variable.
 * 05/23/01 bac   Fixed verify argument so the value can be returned.
 * 05/08/01 mel   Original version.
 ****************************************************************************/
/** @defgroup ipsec_packet Packet-level routines for IPSec
 */

#include "n8_common.h"          /* common definitions */
#include "n8_pub_errors.h"      /* Errors definition */
#include "n8_enqueue_common.h"  /* common definitions for enqueue */
#include "n8_cb_ea.h"
#include "n8_util.h"
#include "n8_packet.h"
#include "n8_util.h"

/* structure for toting around buffers to be compared and result destination in
 * result handler for N8_IPSecDecryptVerify */
typedef struct
{
   N8_Buffer_t  *computedHMAC_p;
   N8_Buffer_t  *data_p;
   N8_Buffer_t  *res_p;
   int           dataLength;
   N8_Boolean_t *verify_p;
   N8_HashAlgorithm_t hashAlgorithm;
} ipsecVerifyPostDataStruct_t;

/**********************************************************************
 * resultHandlerIPSecVerify
 **********************************************************************/
static void resultHandlerIPSecVerify(API_Request_t* req_p)
{
   ipsecVerifyPostDataStruct_t *postData_p = NULL;
   EA_IPSEC_CMD_BLOCK_t    *cmd_ptr = NULL;
   char *title = "resultHandlerIPSecVerify";
   unsigned int actualDataLength;
   
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      cmd_ptr =  (EA_IPSEC_CMD_BLOCK_t *) req_p->EA_CommandBlock_ptr;
      postData_p = (ipsecVerifyPostDataStruct_t *) req_p->postProcessingData_p;

      /* assign the actual data length to skip over the HMAC */
      actualDataLength = postData_p->dataLength - HMAC_LENGTH;

      /* No copy is necessary when res_p is NULL since the chip wrote the */
      /* results directly to the caller's buffer.                         */
      if (postData_p->res_p != NULL)
      {
         memcpy(postData_p->data_p, postData_p->res_p, actualDataLength);
      }

      if (postData_p->hashAlgorithm != N8_HASH_NONE )
      { 

      if (postData_p->computedHMAC_p != NULL ) { 
         /* copy computed HMAC */
         /* return it network byte order */
         uint32_to_BE(cmd_ptr->opad[2],
                   postData_p->computedHMAC_p + sizeof(uint32_t) * 0);
         uint32_to_BE(cmd_ptr->opad[3],
                   postData_p->computedHMAC_p + sizeof(uint32_t) * 1);
         uint32_to_BE(cmd_ptr->opad[4],
                   postData_p->computedHMAC_p + sizeof(uint32_t) * 2);
        } 

      uint32_to_BE(cmd_ptr->opad[2],
                   postData_p->data_p + actualDataLength + sizeof(uint32_t) * 0);
      uint32_to_BE(cmd_ptr->opad[3],
                   postData_p->data_p + actualDataLength + sizeof(uint32_t) * 1);
      uint32_to_BE(cmd_ptr->opad[4],
                   postData_p->data_p + actualDataLength + sizeof(uint32_t) * 2);
      
      if ((cmd_ptr->result & EA_IPSec_MAC_Mismatch) == 0)
      {
         *(postData_p->verify_p) = N8_TRUE;
      }
      }
      else
      {
         *(postData_p->verify_p) = N8_TRUE;
      }
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }
} /* resultHandlerIPSecVerify */
/*****************************************************************************
 * N8_IPSecEncryptAuthenticate
 *****************************************************************************/
/** @ingroup ipsec_packet
 * @brief Encrypt and authenticate an entire IPSec record.
 *
 * Encrypt and authenticate an entire IPSec record.  PacketObject is a packet 
 * object previously initialized by a call to N8_PacketInitialize specifying 
 * IPSec as the packet protocol to use. PacketObject provides encryption and 
 * authentication information to be used in the call. The decryption algorithm 
 * can only be DES as specified when PacketObject was initialized. The 
 * authentication algorithm can be HMAC-MD5-96 or HMACSHA-1-96 as specified 
 * when PacketObject was initialized. Thus, two combinations of 
 * encryption/authentication can be performed: DES/HMAC-MD5-96 and 
 * DES/HMAC-SHA-1-96. The SPI parameter specifies the IPSec Security Parameter 
 * Index of the message. The SPI is used along with the sequence number and 
 * the DES initialization vector (taken from information specified in the
 * PacketObject) in the authentication computation. The message contents of the
 * IPSec packet is given in Data; its length in bytes must be specified in
 * packetLength. Note that Data must include all of the information to be
 * encrypted according to the IPSec specification; this includes the TCP and
 * IP headers if present (the IP header is only present in tunnel mode) and must
 * also include the terminating pad, pad length and next header fields.
 * The byte length of Data is specified in packetLength and must also include all
 * of these fields. packetLength must be a multiple of 8. The fully encrypted and
 * authenticated Data is returned in Result, including the encrypted  data
 * followed by the HMAC-MD5-96 or HMAC-SHA-1-96 authentication value. 
 *
 * @param packetObject_p RW:    The  object denoting the decryption and
 *                              verification computation to be done.  PacketObject
 *                              must have been initialized for use with IPSec. 
 *                              The state in PacketObject will be updated if 
 *                              necessary as part of the call. <BR> 
 * @param packet_p       RO:    The ESP packet beginning with the Security 
 *                              Parameter Index,  Sequence Number and data payload
 *                              including DES initialization vector and ending 
 *                              with the pad, pad length, and next header fields 
 *                              as specified by the IPSec protocol.<BR>
 * @param packetLength     RO:    The length of Data, in bytes, from 8 bytes - 17 KBytes 
 *                              inclusive. A length < 8 or not a multiple of 8 is 
 *                              illegal and results in an error.<BR>
 * @param result_p       WO:    The encrypted / authenticated data, complete with 
 *                              authentication data. Result must be of sufficient 
 *                              size to hold the fully encrypted & authenticated 
 *                              message; its size must be at least packetLength +
 *                              IPSEC_AUTHENTICATION_DATA_LENGTH  
 *                              (hash size for all hash algorithms).<BR>
 * @param event_p        RW:    On input, if null the call is synchronous and no 
 *                              event is returned. The operation is complete when 
 *                              the call returns. If non-null, then the call is 
 *                              asynchronous; an event is returned that can be used 
 *                              to determine when the operation completes.
 *
 *
 * @return 
 *    packetObject_p - The state in PacketObject will be updated if necessary as 
 *                     part of the call.
 *    result_p       - The encrypted / authenticated data, complete with 
 *                     authentication data.
 *    ret   - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - packet object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_INVALID_INPUT_SIZE - The value of packetLength is less than 8 or bigger 
 *                            than 17 KBytes or is not a multiple of 8.
 *    N8_UNIMPLEMENTED_FUNCTION - not supported protocol configuration requested
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 *      packetObject_p was initialized and all parameters checked.<BR>
 *      For IPSec, the authentication data is always
 *      12 (IPSEC_AUTHENTICATION_DATA_LENGTH) bytes (the 12 most  
 *      significant bytes of the standard 16 byte MD5 or 20 byte SHA-1 value) and
 *      is not encrypted. Result is always packetLength + 12 bytes in length. 
 *      The caller is responsible for ensuring that Result is this size.
 *****************************************************************************/
N8_Status_t N8_IPSecEncryptAuthenticate(N8_Packet_t       *packetObject_p, 
                                        N8_IPSecPacket_t  *packet_p, 
                                        int                packetLength,  
                                        N8_IPSecPacket_t  *result_p, 
                                        N8_Event_t        *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
     
   API_Request_t  *req_p = NULL;       /* request buffer */
   EA_CMD_BLOCK_t *next_cb_p = NULL;
   int dataLength;
   unsigned long pack_a;
   N8_Buffer_t *pack_p = NULL;
   unsigned long res_a;
   N8_Buffer_t *res_p = NULL;
   N8_Buffer_t *ctx_p = NULL;
   uint32_t     ctx_a;
   int nBytes;
   int numCommands;
   int numCtxBytes = 0;
   void *callbackFcn = NULL;
   n8_ctxLoadFcn_t    ctxLoadFcn;
   DBG(("N8_IPSecEncryptAuthenticate\n"));     
   do
   {
      /* verify data length */
      if ((packetLength < IPSEC_DATA_LENGTH_MIN) || (packetLength > IPSEC_DATA_LENGTH_MAX))
      {
         DBG(("Data length is out of range\n"));
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      if (packetLength % IPSEC_DATA_BLOCK_SIZE)
      {
         DBG(("Data length is not a multiple of %d\n", IPSEC_DATA_BLOCK_SIZE));
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      /* verify packet object */
      CHECK_OBJECT(packetObject_p, ret);
      CHECK_STRUCTURE(packetObject_p->structureID, N8_PACKET_STRUCT_ID, ret);
      /* verify data object */
      CHECK_OBJECT(packet_p, ret);
      /* verify result object */
      CHECK_OBJECT(result_p, ret);

      dataLength = packetLength - IPSEC_PACKET_HEADER_LENGTH;
      numCommands = N8_CB_EA_IPSECENCRYPTAUTHENTICATE_NUMCMDS;
      if (packetObject_p->contextLoadNeeded == N8_TRUE)
      {
         numCommands += packetObject_p->ctxLoadCmds;
         numCtxBytes = NEXT_WORD_SIZE(EA_CTX_Record_Byte_Length);
      }

      /* compute the space needed for the size of context load, if required */ 
      nBytes = NEXT_WORD_SIZE(HMAC_LENGTH) + numCtxBytes;              

      /* If the data must be copied to a kernel before the chip can operate  */
      /* on it, compute the additional space required and setup the callback */
      /* function to copy the result once the operation has completed.       */
      if (packetObject_p->mode == N8_PACKETMEMORY_NONE)
      {
         nBytes += NEXT_WORD_SIZE(dataLength) + /* packet data length pack_p  */
                   NEXT_WORD_SIZE(packetLength);/* result packet length res_p */
         callbackFcn = resultHandlerGeneric;
      }

      /* create request buffer */
      ret = createEARequestBuffer(&req_p,
                                  packetObject_p->unitID,
                                  numCommands,
                                  callbackFcn,
                                  nBytes);
      CHECK_RETURN(ret);

      /* Compute the context memory pointers. */
      ctx_a = req_p->qr.physicalAddress + req_p->dataoffset;
      ctx_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      /* Compute the addresses for the packet and result buffer. */
      if (packetObject_p->mode == N8_PACKETMEMORY_NONE)
      {
         /* The data must be copied to this kernel before the chip can operate  */
         /* on it, compute the addresses within the kernel buffer and copy in   */
         /* the packet.                                                         */
         pack_a = ctx_a + numCtxBytes;
         pack_p = ctx_p + numCtxBytes;

         res_a = pack_a + NEXT_WORD_SIZE(dataLength);
         res_p = pack_p + NEXT_WORD_SIZE(dataLength);

         memcpy(pack_p, &packet_p[IPSEC_DATA_OFFSET], dataLength);
      
      }
      else
      {
         /* The chip can access the data directly, compute the */
         /* physical addresses of the packet & result buffer.  */
         pack_a = N8_VirtToPhys(&packet_p[IPSEC_DATA_OFFSET]);
         res_a =  N8_VirtToPhys(&result_p[IPSEC_DATA_OFFSET]);
      }


      next_cb_p = req_p->EA_CommandBlock_ptr;
      
      memcpy(result_p, packet_p, IPSEC_PACKET_HEADER_LENGTH);

      packetObject_p->cipherInfo.key.IPsecKeyDES.sequence_number =
         IPSEC_EXTRACT_SEQUENCE_NIMBER(packet_p);

      /* generate the command blocks necessary to load the context, if required */
      if (packetObject_p->contextLoadNeeded == N8_TRUE)
      {
         /* Generate the command blocks for the Context Load */
         ctxLoadFcn = (n8_ctxLoadFcn_t)packetObject_p->ctxLoadFcn;
         ret = ctxLoadFcn(req_p,
                          next_cb_p,
                          packetObject_p,
                          &packetObject_p->cipherInfo,
                          packetObject_p->packetHashAlgorithm,
                          ctx_p,
                          ctx_a,
                          &next_cb_p);
         CHECK_RETURN(ret);

         packetObject_p->contextLoadNeeded = N8_FALSE;
      }

      memcpy(&packetObject_p->cipherInfo.IV[0], packet_p+IPSEC_IV_OFFSET,
             N8_DES_KEY_LENGTH); 

      cb_ea_IPsec(next_cb_p,
                  packetObject_p,
                  pack_a, 
                  res_a,
                  dataLength,
                  IPSEC_EXTRACT_SPI(packet_p),
                  packetObject_p->encOpCode);


      req_p->copyBackTo_p   = result_p + IPSEC_DATA_OFFSET;
      req_p->copyBackFrom_p = res_p;
      req_p->copyBackSize   = dataLength + HMAC_LENGTH;

      QUEUE_AND_CHECK(event_p, req_p, ret)
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_IPSecEncryptAuthenticate - FINISHED\n"));
   return ret;
} /* N8_IPSecEncryptAuthenticate */


 
/*****************************************************************************
 * N8_IPSecDecryptVerify
 *****************************************************************************/
/** @ingroup ipsec_packet
 * @brief Decrypt and verify an entire IPSec record.
 *
 * Decrypt and verify an entire IPSec record. PacketObject is a packet object 
 * previously initialized by a call to N8_PacketInitialize specifying IPSec 
 * as the packet protocol to use. PacketObject provides encryption and 
 * authentication information to be used in the call. The decryption algorithm 
 * can only be DES as specified when PacketObject was initialized. The 
 * authentication algorithm can be HMAC-MD5-96 or HMAC-SHA-1-96 as specified 
 * when PacketObject was initialized. Thus, two combinations of 
 * decryption/verification can be performed: DES/HMAC-MD5-96 and DES/HMAC-SHA-1-96. 
 * The SPI parameter specifies the IPSec Security Parameter Index of the message. 
 * The SPI is used along with the sequence number and the DES initialization 
 * vector (taken from PacketObject) in the authentication computation. The 
 * message contents of the encrypted and authenticated IPSec packet is given 
 * in EncryptedData; its length in bytes must be specified in encryptedPacketLength. 
 * Note that EncryptedData must end with the unencrypted12-byte authentication 
 * value as required by the IPSec specification. The byte length of EncryptedData 
 * including the 12 byte authentication value is specified in encryptedPacketLength. 
 * encryptedPacketLength must be at least 20 (i.e., EncryptedData must consist of 
 * at least one DES block to be decrypted plus the mandatory 12 byte 
 * authentication value), and must be of the form 12 + n*8 for n > 0. The decrypted 
 * EncryptedData is returned in Result, including the decrypted  data followed by 
 * the 12 bytes of authentication data. Result is always encryptedPacketLength 
 * bytes in length. The caller is responsible for ensuring that Result is at  
 * least this size.  This call also calculates the authentication value on the 
 * EncryptedData and compares this calculated value to the value at the end of 
 * EncryptedData. If these two values are equal, the verification succeeds and 
 * True is returned in Verify, otherwise False is returned in Verify.
 *  
 *
 * @param packetObject_p RW:    The  object denoting the decryption and 
 *                              verification computation to be done. PacketObject
 *                              must have been initialized for use with IPSec. 
 *                              The state in PacketObject will be updated if 
 *                              necessary as part of the call. <BR> 
 * @param encryptedPacket_p RO:   The message portion / contents of the IPSec packet.<BR>
 * @param encryptedPacketLength RO:  The length of EncryptedData, in bytes, 
 *                              from 20 bytes - 18 KBytes inclusive. A length 
 *                              less than 20 or not of the form 12+n*8 is illegal
 *                              and results in an error.<BR>
 * @param Verify         WO:    Returned as True if the computed authentication 
 *                              value on EncryptedData matches the authentication 
 *                              value contained at the end of EncryptedData; 
 *                              otherwise False is returned. If Verify is False, 
 *                              the contents of Result may be gibberish and/or 
 *                              have been altered.<BR>
 * @param result_p       WO:    The decrypted / authenticated data, complete 
 *                              with authentication data. Result must be of 
 *                              sufficient size to hold the decrypted & 
 *                              authenticated message; its size must be at least 
 *                              EncryptedDataLeng.<BR>
 * @param event_p          RW:    On input, if null the call is synchronous and no 
 *                              event is returned. The operation is complete when 
 *                              the call returns. If non-null, then the call is 
 *                              asynchronous; an event is returned that can be used 
 *                              to determine when the operation completes.
 *
 *
 * @return 
 *    packetObject_p - The state in PacketObject will be updated if necessary as 
 *                     part of the call.
 *    result_p       - The encrypted / authenticated data, complete with 
 *                     authentication data.
 *    ret   - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *    N8_INVALID_OBJECT   - packet object is zero, couldn't write to unspecified
 *                          address<BR>
 *    N8_INVALID_INPUT_SIZE - The value of packetLength is < 20 or > 18 KBytes or 
 *                          is not of the form 12 + n*8; no operation is 
 *                          performed and no result is  returned.
 *    N8_UNIMPLEMENTED_FUNCTION - not supported protocol configuration requested
 *    N8_HARDWARE_ERROR   - couldn't write to context memory
 *   
 *
 * @par Assumptions:
 *      packetObject_p was initialized and all parameters checked.
 *****************************************************************************/
N8_Status_t N8_IPSecDecryptVerify( N8_Packet_t   *packetObject_p, 
                                   N8_IPSecPacket_t *encryptedPacket_p, 
                                   int               encryptedPacketLength,
                                   N8_Buffer_t      *computedHMAC_p,
                                   N8_Boolean_t     *verify_p,
                                   N8_IPSecPacket_t *result_p, 
                                   N8_Event_t       *event_p )
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */

   API_Request_t  *req_p = NULL;       /* request buffer */
   EA_CMD_BLOCK_t *next_cb_p = NULL;
   int dataLength;
   unsigned long pack_a;
   N8_Buffer_t *pack_p = NULL;
   unsigned long res_a;
   N8_Buffer_t *res_p = NULL;
   N8_Buffer_t *ctx_p = NULL;
   uint32_t     ctx_a;
   int nBytes;
   int numCommands;
   int numCtxBytes = 0;
   ipsecVerifyPostDataStruct_t *postData_p = NULL;
   n8_ctxLoadFcn_t    ctxLoadFcn;

   DBG(("N8_IPSecDecryptVerify\n"));
   do
   {
      *verify_p = N8_FALSE;
      /* verify data length */
      if ((encryptedPacketLength < IPSEC_DECRYPTED_DATA_LENGTH_MIN) ||
          (encryptedPacketLength > IPSEC_DATA_LENGTH_MAX))
      {
         DBG(("Data length is out of range\n"));
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
    
      if ((encryptedPacketLength - IPSEC_AUTHENTICATION_DATA_LENGTH) % IPSEC_DATA_BLOCK_SIZE)
      {
         DBG(("Data length is not a multiple of %d\n", IPSEC_DATA_BLOCK_SIZE));
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      /* verify packet object */
      CHECK_OBJECT(packetObject_p, ret);
      CHECK_STRUCTURE(packetObject_p->structureID, N8_PACKET_STRUCT_ID, ret);
      /* verify encrypted data object */
      CHECK_OBJECT(encryptedPacket_p, ret);
      /* verify result object */
      CHECK_OBJECT(result_p, ret);
#ifdef NO_MAC_COPYBACK
      CHECK_OBJECT(computedHMAC_p, ret);
#endif 

      dataLength = encryptedPacketLength - IPSEC_PACKET_HEADER_LENGTH;

      numCommands = N8_CB_EA_IPSECDECRYPTVERIFY_NUMCMDS;
      if (packetObject_p->contextLoadNeeded == N8_TRUE)
      {
         numCommands += packetObject_p->ctxLoadCmds;
         numCtxBytes = NEXT_WORD_SIZE(sizeof(EA_ARC4_CTX));
      }

      /* compute the space needed for the chip to place the result */
      nBytes = NEXT_WORD_SIZE(HMAC_LENGTH) + numCtxBytes; /* context to load */
      if (packetObject_p->mode == N8_PACKETMEMORY_NONE)
      {
         nBytes += NEXT_WORD_SIZE(dataLength * 2);
      }

      /* create request buffer */
      ret = createEARequestBuffer(&req_p,
                                  packetObject_p->unitID,
                                  numCommands,
                                  resultHandlerIPSecVerify,
                                  nBytes);
      CHECK_RETURN(ret);

      req_p->copyBackCommandBlock = N8_TRUE;

      /* Compute the addresses for the context. */
      ctx_a = req_p->qr.physicalAddress + req_p->dataoffset;
      ctx_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);

      /* Compute the addresses for the packet and result buffer. */
      if (packetObject_p->mode == N8_PACKETMEMORY_NONE)
      {
         /* The data must be copied to this kernel before the chip can operate  */
         /* on it, compute the addresses within the kernel buffer and copy in   */
         /* the packet.                                                         */
         pack_a = ctx_a + numCtxBytes;
         pack_p = ctx_p + numCtxBytes;

         res_a = pack_a + NEXT_WORD_SIZE(dataLength);
         res_p = pack_p + NEXT_WORD_SIZE(dataLength);

         memcpy(pack_p, encryptedPacket_p + IPSEC_DATA_OFFSET, dataLength); 
      
      }
      else
      {
         /* The chip can access the data directly, compute the */
         /* physical addresses of the packet & result buffer.  */
         pack_a = N8_VirtToPhys(encryptedPacket_p + IPSEC_DATA_OFFSET);
         res_a =  N8_VirtToPhys(result_p + IPSEC_DATA_OFFSET);
      }

      next_cb_p = req_p->EA_CommandBlock_ptr;

      /* generate the command blocks necessary to load the context, if required */
      if (packetObject_p->contextLoadNeeded == N8_TRUE)
      {
         /* Generate the command blocks for the Context Load */
         ctxLoadFcn = (n8_ctxLoadFcn_t)packetObject_p->ctxLoadFcn;
         ret = ctxLoadFcn(req_p,
                          next_cb_p,
                          packetObject_p,
                          &packetObject_p->cipherInfo,
                          packetObject_p->packetHashAlgorithm,
                          ctx_p,
                          ctx_a,
                          &next_cb_p);
         CHECK_RETURN(ret);

         packetObject_p->contextLoadNeeded = N8_FALSE;
      }

      memcpy(&packetObject_p->cipherInfo.IV[0], encryptedPacket_p+8,8);
      packetObject_p->cipherInfo.key.IPsecKeyDES.sequence_number =
         IPSEC_EXTRACT_SEQUENCE_NIMBER(encryptedPacket_p);

      cb_ea_IPsec(next_cb_p,
                  packetObject_p,
                  pack_a, 
                  res_a,
                  dataLength,
                  IPSEC_EXTRACT_SPI(encryptedPacket_p),
                  packetObject_p->decOpCode);

      postData_p = (ipsecVerifyPostDataStruct_t *)req_p->postProcessBuffer;
      postData_p->computedHMAC_p = computedHMAC_p;
      postData_p->data_p = result_p + IPSEC_DATA_OFFSET;
      postData_p->res_p = res_p;
      postData_p->dataLength = dataLength;
      postData_p->verify_p = verify_p;
      postData_p->hashAlgorithm = packetObject_p->packetHashAlgorithm;

      /* setup post processing buffer pointer */
      req_p->postProcessingData_p = (void *) postData_p;

      /* decrypt data */
      QUEUE_AND_CHECK(event_p, req_p, ret)
      HANDLE_EVENT(event_p, req_p, ret);
      /* compare authentication values */

      memcpy(result_p, encryptedPacket_p, IPSEC_PACKET_HEADER_LENGTH); 
      if (*verify_p == N8_TRUE)
      {
         packetObject_p->cipherInfo.key.IPsecKeyDES.sequence_number =
            IPSEC_EXTRACT_SEQUENCE_NIMBER(encryptedPacket_p);
         memcpy(&packetObject_p->cipherInfo.IV[0],
                encryptedPacket_p+IPSEC_IV_OFFSET,
                N8_DES_KEY_LENGTH);

      }

   } while (FALSE);

   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   DBG(("N8_IPSecDecryptVerify - FINISHED\n"));
   return ret;
} /* N8_IPSecDecryptVerify */
