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

static char const n8_id[] = "$Id: n8_ssltls.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_ssltls.c
 *  @brief SSL 3.0 Packet Level Interfaces
 *
 * Implementation of functions for SSL 3.0 Packet Level Interfaces.  Functions
 * include:
 * N8_SSLEncryptAuthenticate
 * N8_SSLDecryptVerify
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 *
 * 08/18/03 brr   Combine Encrypt/Decrypt command block generators for SSL,
 *                TLS, & IPsec.
 * 07/28/03 brr   Removed obsolete #ifdefs. (Bug 918)
 * 07/01/03 brr   Added option to use no hashing algorithm.
 * 05/27/03 brr   Removed N8_preamble call since N8_PacketInitialize must be
 *                called prior to Encrypt/Decrypt operations.
 * 05/20/03 brr   Modified N8_PacketInitialize to setup function pointers & 
 *                lengths used in the Encrypt/Decrypt operations. Eliminated
 *                several switch statements from Encrypt/Decrypt operations.
 * 11/01/02 jpw   If the computedMAC_p parameter from N8_EA_DV is NULL 
 * 		  then don't copy back the MAC value.
 * 08/02/02 arh   Fix N8_SSLTLSEncryptAuthenticate & N8_SSLTLSDecryptVerify
 *                to check packet object ptr before storing mode in it,
 *                preventing segmentation faults if passed ptr is null.
 *                This check got lost when N8_*Memory routines added.
 * 07/16/02 arh   Fix Bug #815: Make the N8_*Memory routines check for
 *                legal values for the mode in the packet object.
 * 07/14/02 bac   Simplified the calculation of the physical address for kernel
 *                buffers.  Consolidated logic for kernel buffer requests.
 * 06/14/02 hml   Added the new N8_SSLTLS*Memory calls.  The old calls are 
 *                still supported.
 * 06/10/02 hml   Handle the new request pools.
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 03/29/02 hml   Works correctly with user allocated kernel buffers.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 02/27/02 hml   Correctly handles the propagation of the IV.
 * 02/07/02 bac   Deferred loading of the context caused the result handler to
 *                not copy the computed mac back from the correct command. 
 * 01/31/02 brr   Eliminated the memory allocation for postProcessingData.
 * 01/22/02 bac   Added code to use software to do SSL and HMAC precomputes.
 * 01/22/02 bac   Added deferred loading of context until the first use.
 * 01/21/02 bac   Changes to support new kernel allocation scheme.
 * 12/11/01 mel   Fixed bug #397: addBlockToFree(postData_p..) and deleted 
 *                freeing postData_p from result handlers.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/12/01 hml   Added structure verification and changed PI_PROTOCOL_*
 *                to N8_PROTOCOL_*.
 * 11/05/01 bac   Added missing check of return code after KMALLOC.
 * 10/11/01 brr   Remove warnings exposed when optimization turned up.
 * 10/02/01 bac   Added use of RESULT_HANDLER_WARNING in all result handlers.
 * 09/26/01 hml   Get chip number out of the packet object.
 * 09/20/01 bac   The interface to the command block generators changed and now
 *                accept the command block buffer.  All calls to cb_ea methods
 *                changed herein.
 * 09/14/01 bac   Fixed a bug where CHECK_RETURN was not being called in the
 *                correct spot.  BUG #208.
 * 09/12/01 bac   Corrected the decrement of the sequence number upon failure of
 *                DES operations that do not use the context memory.  The
 *                affected the methods resultHandlerSSLTLSDecrypt and
 *                resultHandlerSSLTLSEncrypt. BUG #195.
 * 09/06/01 bac   Added include of <string.h> to silence warning.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/30/01 bac   Pass chip id to createEARequestBuffer.
 * 06/28/01 bac   Major fixes to get the encrypt and return packet lengths
 *                correct, removed 'freeRequest' from the result handlers as it
 *                now resides in the EventCheck routine.
 * 06/25/01 bac   Changes for kernel memory management.
 * 06/14/01 bac   Changes per code review:  free requests upon error on enqueue,
 *                use define for N8_DES_BLOCK_MULTIPLE, correct use of
 *                N8_SSLTLS_MAX_DATA_SIZE_DECRYPT and
 *                N8_SSLTLS_MAX_DATA_SIZE_ENCRYPT 
 * 06/05/01 bac   Changes to not rely on N8_SSLTLSPacket_t being packed (Bug
 *                #31). 
 * 05/30/01 bac   Improved documentation of structures and return types (bug
 *                #27).   Removed reference to API Request pointer in the
 *                QUEUE_AND_CHECK macro as the pointer will have already been
 *                freed (bug #17). 
 * 05/22/01 bac   Changes to have N8_SSLTLSEncryptAuthenticate and
 *                N8_SSLTLSDecryptVerify take and return N8_SSLTLSPacket_t.
 *                Also, now calculate and return the packet lengths in the
 *                packet. 
 * 05/21/01 bac   Removed unused resultHandler.  Converted to
 *                N8_ContextHandler_t. 
 * 05/19/01 bac   Free postProcessingData in resultHandlers when required.
 * 05/18/01 bac   Converted to N8_xMALLOC and N8_xFREE
 * 05/18/01 bac   Added support for Decrypt commands.
 * 05/02/01 bac   Original version.
 ****************************************************************************/
/** @defgroup n8_ssltls Simon SSL/TLS Functions
 */

#include "n8_enqueue_common.h"
#include "n8_util.h"
#include "n8_common.h"
#include "n8_ssltls.h"
#include "n8_cb_ea.h"
#include "n8_OS_intf.h"
#include "n8_pub_request.h"

/* local prototypes */
static void resultHandlerSSLTLSEncrypt(API_Request_t* req_p);
static void resultHandlerSSLTLSDecrypt(API_Request_t* req_p);

/*
 * Local structure used to pass data to the result handler.  It is hooked to the
 * API Request in order to be available in the callback.
 */
typedef struct
{
   N8_Buffer_t       *computedMAC_p;
   N8_SSLTLSPacket_t *resultPacket_p;
   N8_Packet_t       *packetObj_p;
   N8_Boolean_t      *verify_p;
   int                macLength;
   int                encryptedLength;
   int                copyData;
   unsigned char      nextIV[8];
} N8_SSL_Post_Decrypt_Data_t;


/* macro defines */

/* exported functions */

/*****************************************************************************
 * N8_SSLTLSEncryptAuthenticate
 *****************************************************************************/
/** @ingroup n8_ssltls
 * @brief Encrypt/Authenticate an SSL or TLS message
 *
 * An entire SSL or TLS record is encrypted/authenticated.
 *
 *  @param packetObj_p         RW:  Pointer to a packet object which was
 *                                  previously initialized for SSL or TLS
 *  @param packet_p            RO:  Pointer to packet
 *  @param result_p            RW:  Pointer to pre-allocated buffer where the
 *                                  results will be stored.
 *  @param request             RW:  Kernel request buffer used when mode =
 *                                  N8_PACKETMEMORY_REQUEST.
 *  @param event_p             RW:  Asynchronous event pointer.
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
 *    None
 *****************************************************************************/
N8_Status_t 
N8_SSLTLSEncryptAuthenticateMemory(N8_Packet_t             *packetObj_p,
                                   const N8_SSLTLSPacket_t *packet_p,
                                   N8_SSLTLSPacket_t       *result_p,
                                   N8_RequestHandle_t       request,
                                   N8_Event_t              *event_p)
{

   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;
   EA_CMD_BLOCK_t *next_cb_p = NULL;
   uint16_t length;
   short int encLen;
   short int packetLen;
   N8_Buffer_t *res_p = NULL;
   uint32_t     res_a;
   N8_Buffer_t *input_p = NULL;
   uint32_t     input_a;
   N8_Buffer_t *ctx_p = NULL;
   uint32_t     ctx_a = 0;
   int          nBytes;
   unsigned int numCommands = 0;
   unsigned int numCtxBytes = 0;
   N8_MemoryHandle_t *request_p;
   n8_ctxLoadFcn_t    ctxLoadFcn;
   n8_SSLTLSFcn_t     encryptFcn;

   do
   {
      /* verify the pointers passed in are not null */
      CHECK_OBJECT(packetObj_p, ret);
      CHECK_STRUCTURE(packetObj_p->structureID, N8_PACKET_STRUCT_ID, ret);
      CHECK_OBJECT(packet_p, ret);
      CHECK_OBJECT(result_p, ret);

      request_p = (N8_MemoryHandle_t *) request;

      /* Check that request_p & mode are consistent */
      if (request_p == NULL)
      {
         if (packetObj_p->mode != N8_PACKETMEMORY_NONE)
         {
            /* No request_p means mode should be "none" */
            ret = N8_INCONSISTENT;
            break;
         }
      }
      else if (packetObj_p->mode != N8_PACKETMEMORY_REQUEST)
      {
         /* They said they would give us a request and they didn't */
         ret = N8_INCONSISTENT;
         break;
      }

      /*
       * convert the length from network order to host order
       */
      length = SSLTLS_EXTRACT_LENGTH(packet_p);
      /* check to see the data length is within range
       * [0..N8_SSLTLS_MAX_DATA_SIZE_ENCRYPT] 
       */
      if (length > N8_SSLTLS_MAX_DATA_SIZE_ENCRYPT)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      numCommands = packetObj_p->encCommands;
      if (packetObj_p->contextLoadNeeded == N8_TRUE)
      {
         numCommands += packetObj_p->ctxLoadCmds;
         numCtxBytes = NEXT_WORD_SIZE(sizeof(EA_ARC4_CTX));
      }

      /* create a kernel memory structure for temporarily storing the results */
      /* compute the lengths for the result packet */
      encLen = N8_ComputeEncryptedLength(length,
                                         packetObj_p->hashPacket.hashSize,
                                         packetObj_p->packetCipher); 
      packetLen = encLen + SSLTLS_HEADER_LEN;

      /* compute the total number of bytes in kernel space we need to allocate.
       * note we round up to the next word boundary. */
      /* Note that if the in packet is NULL, we don't check the out packet */
      if (request_p == NULL)
      {
         nBytes = (NEXT_WORD_SIZE(length) + /* input */
                   NEXT_WORD_SIZE(encLen) + /* output */
                   numCtxBytes);            /* size of context, if needed */
         /* create an API request buffer */
         ret = createEARequestBuffer(&req_p,
                                     packetObj_p->unitID,
                                     numCommands,
                                     resultHandlerSSLTLSEncrypt,
                                     nBytes); 
         CHECK_RETURN(ret);
         /* User has not provided a request buffer */
         res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
         res_a = req_p->qr.physicalAddress + req_p->dataoffset;

         input_p = res_p + NEXT_WORD_SIZE(length);
         input_a = res_a + NEXT_WORD_SIZE(length);

         ctx_p = input_p + NEXT_WORD_SIZE(encLen);
         ctx_a = input_a + NEXT_WORD_SIZE(encLen);
      }
      else
      {
         /* The user has given us a request structure.  We just need to
            initialize it. */
         req_p = (API_Request_t *) request_p->VirtualAddress;
         initializeEARequestBuffer(req_p,
                                   request_p,
                                   packetObj_p->unitID,
                                   numCommands,
                                   resultHandlerSSLTLSEncrypt,
                                   N8_TRUE);

         /* User has provided a request packet, which must contain enough
            space for the input and output packets. The user may
            reuse the input pointer for the output. */
         input_p = (N8_Buffer_t *)((unsigned long) packet_p + SSLTLS_DATA_OFFSET);
         input_a = N8_VirtToPhys(input_p);

         res_p = (N8_Buffer_t *)((unsigned long) result_p + SSLTLS_DATA_OFFSET);
         res_a = N8_VirtToPhys(res_p);
         /* If we are loading context memory, we load it at the last 
            N8_CTX_BYTES before the data section */
         if (numCtxBytes > 0)
         {
            ctx_p = (N8_Buffer_t *)((unsigned long) req_p + sizeof(API_Request_t) + N8_COMMAND_BLOCK_BYTES);
            ctx_a = N8_VirtToPhys(ctx_p);
         }
      }

      next_cb_p = req_p->EA_CommandBlock_ptr;

      /* generate the command blocks necessary to load the context, if required */
      if (packetObj_p->contextLoadNeeded == N8_TRUE)
      {
         /* Generate the command blocks for the Context Load */
         ctxLoadFcn = (n8_ctxLoadFcn_t)packetObj_p->ctxLoadFcn;
         ret = ctxLoadFcn(req_p, 
                          next_cb_p,
                          packetObj_p,
                          &packetObj_p->cipherInfo, 
                          packetObj_p->packetHashAlgorithm,
                          ctx_p,
                          ctx_a,
                          &next_cb_p);
         CHECK_RETURN(ret);
         packetObj_p->contextLoadNeeded = N8_FALSE;
      }

      req_p->copyBackFrom_p = res_p;
      req_p->postProcessingData_p = (void *) packetObj_p;
      /* This sets up the offset for the start of the copy back of the IV */
      req_p->postProcessBuffer[0] = encLen - N8_DES_KEY_LENGTH;

      if (request_p == NULL)
      {
         memcpy(input_p, &packet_p[SSLTLS_DATA_OFFSET], length);
         req_p->copyBackTo_p = &result_p[SSLTLS_DATA_OFFSET];
         req_p->copyBackSize = encLen;
      }
      else
      {
         req_p->copyBackSize = 0;
      }

      /* Generate the command blocks for the Encrypt */
      encryptFcn = (n8_SSLTLSFcn_t)packetObj_p->SSLTLScmdFcn;
      ret = encryptFcn(next_cb_p,
                       packetObj_p,
                       packet_p,
                       input_a,
                       res_a,
                       packetObj_p->encOpCode);
      CHECK_RETURN(ret);

      /* set up the return packet.  note that this can be done before the
       * command is executed as it is only constant header information. */
      memcpy(&result_p[SSLTLS_TYPE_OFFSET],
             &packet_p[SSLTLS_TYPE_OFFSET],
             sizeof(uint8_t) + sizeof(uint16_t)); 
      SSLTLS_SET_LENGTH(result_p, encLen);

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /* clean up if necessary.  note that if there is no error condition, the
    * request will be de-allocated in the result handler. */ 
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;

} /* N8_SSLEncryptAuthenticateMemory */

/*****************************************************************************
 * N8_SSLTLSEncryptAuthenticate
 *****************************************************************************/
/** @ingroup n8_ssltls
 * @brief Encrypt/Authenticate an SSL or TLS message
 *
 * An entire SSL or TLS record is encrypted/authenticated. Note that this call 
 * is now supported as a front end to N8_SSLTLSEncryptAuthenticateMemory.
 *
 *  @param packetObj_p         RW:  Pointer to a packet object which was
 *                                  previously initialized for SSL or TLS
 *  @param packet_p            RO:  Pointer to packet
 *  @param result_p            RW:  Pointer to pre-allocated buffer where the
 *                                  results will be stored.
 *  @param event_p             RW:  Asynchronous event pointer.
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
 *    None
 *****************************************************************************/
N8_Status_t N8_SSLTLSEncryptAuthenticate(N8_Packet_t *packetObj_p,
                                         const N8_SSLTLSPacket_t *packet_p,
                                         N8_SSLTLSPacket_t *result_p,
                                         N8_Event_t *event_p)
{
   N8_Status_t           retCode;
   N8_PacketMemoryMode_t saveMode;

   do
   {
      CHECK_OBJECT(packetObj_p, retCode);

      /* save the existing mode and set it */
      saveMode = packetObj_p->mode;
      packetObj_p->mode = N8_PACKETMEMORY_NONE;

      retCode = N8_SSLTLSEncryptAuthenticateMemory(packetObj_p,
                                                   packet_p,
                                                   result_p,
                                                   NULL,
                                                   event_p);
      /* reset the mode */
      packetObj_p->mode = saveMode;

   } while (FALSE);

   return retCode;
}

/*****************************************************************************
 * N8_SSLTLSDecryptVerifyMemory
 *****************************************************************************/
/** @ingroup n8_ssltls
 * @brief Decrypt and verify an entire SSL or TLS message.
 *
 * Decrypt an verify a SSL or TLS message.  If the verification fails, it will
 * be noted in the return value.
 *
 *  @param packetObj_p         RW:  Pre-initialized packet object.
 *  @param packet_p            RO:  A complete, encrypted SSL or TLS packet
 *                                  including the header.
 *  @param computedMAC_p       RW:  Pointer to result space for the MAC
 *                                  computation output.
 *  @param verify_p:           WO:  Pointer to a verify flag.  If the MAC's
 *                                  match, the verify flag will be set to N8_TRUE.
 *  @param result_p            RW:  Pointer to result space for the decrypted
 *                                  result. 
 *  @param request             RW:  Kernel request buffer used when mode =
 *                                  N8_PACKETMEMORY_REQUEST.
 *  @param event_p             RW:  Asynchronous event pointer.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error condition if raised.
 *
 * @par Errors
 *    N8_MALLOC_FAILED<br>
 *    N8_INVALID_INPUT_SIZE - the value in the packet header is too large or too
 *                            small.<br>
 *    N8_INCONSISTENT       - the version number in the packet header does not
 *                            match the designation in the packet object.<br>
 *    N8_INVALID_OBJECT     - packet object is not a valid object initialized
 *                            for SSL or TLS<br>
 *    N8_INVALID_VALUE      - the value of the type field in the packet header
 *                            is not one of the legal values for SSL or TLS.<br>
 *
 * @par Assumptions
 *    The result_p and computedMAC_p pointers refer to pre-allocated space of
 * sufficient size.  The calling program retains responsibility for freeing this
 * memory. 
 *****************************************************************************/
N8_Status_t 
N8_SSLTLSDecryptVerifyMemory(N8_Packet_t             *packetObj_p,
                             const N8_SSLTLSPacket_t *packet_p,
                             N8_Buffer_t             *computedMAC_p,
                             N8_Boolean_t            *verify_p,
                             N8_SSLTLSPacket_t       *result_p,
                             N8_RequestHandle_t       request,
                             N8_Event_t              *event_p)
{

   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;
   EA_CMD_BLOCK_t *next_cb_p = NULL;
   N8_SSL_Post_Decrypt_Data_t *postData_p = NULL;
   uint16_t length;
   short int encLen;
   int nBytes;
   N8_Buffer_t *res_p = NULL;
   uint32_t     res_a;
   N8_Buffer_t *input_p = NULL;
   uint32_t     input_a;
   N8_Buffer_t *ctx_p = NULL;
   uint32_t     ctx_a = 0;
   unsigned int numCommands = 0;
   unsigned int numCtxBytes = 0;
   N8_MemoryHandle_t *request_p;
   n8_ctxLoadFcn_t  ctxLoadFcn;
   n8_SSLTLSFcn_t   decryptFcn;
   do
   {
      /* ensure none of the pointers we're given are null */
      CHECK_OBJECT(packetObj_p, ret);
      CHECK_STRUCTURE(packetObj_p->structureID, N8_PACKET_STRUCT_ID, ret);
      CHECK_OBJECT(packet_p, ret);
#ifdef N8_MAC_COPYBACK
      CHECK_OBJECT(computedMAC_p, ret);
#endif
      CHECK_OBJECT(result_p, ret);

      request_p = (N8_MemoryHandle_t *) request;

      /* Check that request_p & mode are consistent */
      if (request_p == NULL)
      {
         if (packetObj_p->mode != N8_PACKETMEMORY_NONE)
         {
            /* No request_p means mode should be "none" */
            ret = N8_INCONSISTENT;
            break;
         }
      }
      else if (packetObj_p->mode != N8_PACKETMEMORY_REQUEST)
      {
         /* They said they would give us a request and they didn't */
         ret = N8_INCONSISTENT;
         break;
      }

      /*
       * convert the length from network order to host order
       */
      length  = ntohs(*((uint16_t *) &packet_p[SSLTLS_LENGTH_OFFSET]));

      /* verify the length is not greater than the max */
      if (length > N8_SSLTLS_MAX_DATA_SIZE_DECRYPT)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      numCommands = packetObj_p->decCommands;
      if (packetObj_p->contextLoadNeeded == N8_TRUE)
      {
         numCommands += packetObj_p->ctxLoadCmds;
         numCtxBytes = NEXT_WORD_SIZE(sizeof(EA_ARC4_CTX));
      }

      encLen = N8_ComputeEncryptedLength(length, 
                                         packetObj_p->hashPacket.hashSize,
                                         packetObj_p->packetCipher);

      /* kernel space is needed for the data portion of the incoming packet plus
       * the hash and padding.  the exact amount is needed for the return
       * packet. */
      /* Note that if the in packet is NULL, we don't check the out packet */
      if (request_p == NULL)
      {
         nBytes = NEXT_WORD_SIZE(length) * 2 + numCtxBytes;
         /* create an API request buffer */
         ret = createEARequestBuffer(&req_p,
                                     packetObj_p->unitID,
                                     numCommands,
                                     resultHandlerSSLTLSDecrypt,
                                     nBytes); 
         CHECK_RETURN(ret);
         /* User has not provided any buffers */
         res_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
         res_a = req_p->qr.physicalAddress + req_p->dataoffset;

         input_p = res_p + NEXT_WORD_SIZE(length);
         input_a = res_a + NEXT_WORD_SIZE(length);

         ctx_p = input_p + NEXT_WORD_SIZE(encLen);
         ctx_a = input_a + NEXT_WORD_SIZE(encLen);
      }
      else
      {
         /* User has provided a request packet, which must contain enough
            space for the input and output packets. The user may
            reuse the input pointer for the output. */

         /* The user has given us a request structure.  We just need to
            initialize it. */
         req_p = (API_Request_t *) request_p->VirtualAddress;
         initializeEARequestBuffer(req_p,
                                   request_p,
                                   packetObj_p->unitID,
                                   numCommands,
                                   resultHandlerSSLTLSDecrypt,
                                   N8_TRUE);

         /* Initialize input_p and input_a to the beginning of
            data area in the request */
         input_p = (N8_Buffer_t *)((unsigned long) packet_p + SSLTLS_DATA_OFFSET);
         input_a = N8_VirtToPhys(input_p);


         res_p = result_p + SSLTLS_DATA_OFFSET;
         res_a = N8_VirtToPhys(res_p);

         /* If we are loading context memory, we load it at the last 
            N8_CTX_BYTES before the data section */
         if (numCtxBytes > 0)
         {
            ctx_p = (N8_Buffer_t *)((unsigned long) req_p + sizeof(API_Request_t) + N8_COMMAND_BLOCK_BYTES);
            ctx_a = N8_VirtToPhys(ctx_p);
         }
      }

      next_cb_p = req_p->EA_CommandBlock_ptr;

      /* generate the command blocks necessary to load the context, if required */
      if (packetObj_p->contextLoadNeeded == N8_TRUE)
      {
         /* Generate the command blocks for the Context Load */
         ctxLoadFcn = (n8_ctxLoadFcn_t)packetObj_p->ctxLoadFcn;
         ret = ctxLoadFcn(req_p, 
                          req_p->EA_CommandBlock_ptr,
                          packetObj_p,
                          &packetObj_p->cipherInfo, 
                          packetObj_p->packetHashAlgorithm,
                          ctx_p,
                          ctx_a,
                          &next_cb_p);
         CHECK_RETURN(ret);
         packetObj_p->contextLoadNeeded = N8_FALSE;
      }

      /* copy the data portion of the incoming packet into the input kernel
       * buffer */
      if (request_p == NULL)
      {
         memcpy(input_p, &packet_p[SSLTLS_DATA_OFFSET], length);
      }

      /* use the post processing structure in the API Request */
      postData_p = (N8_SSL_Post_Decrypt_Data_t *)&req_p->postProcessBuffer;

      /* now check the length */
      if (length < packetObj_p->minLength)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      /* if des, ensure the length is a multiple of N8_DES_BLOCK_MULTIPLE */
      if ((packetObj_p->packetCipher == N8_CIPHER_DES) &&
          ((length % N8_DES_BLOCK_MULTIPLE) != 0))
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      /* set up the copy back for the result packet.  we only copy back the data
       * portion which we get from the kernel buffer. */
      req_p->copyBackTo_p = &result_p[SSLTLS_DATA_OFFSET];
      req_p->copyBackFrom_p = res_p;
      /* the length the decrypted data is not known until after the operation is
       * run.  we will calculate it in the resultHandler.  set the copyBackSize
       * to zero as a safe thing to do.*/
      req_p->copyBackSize = 0;

      if (packetObj_p->contextHandle.inUse == N8_FALSE)
      {
         /* This sets up the offset for the start of the copy back of the IV. 
            This case can only occur if the cipher is DES, not ARC4 */
         memcpy(&postData_p->nextIV, 
                &(packet_p[5 - N8_DES_KEY_LENGTH + length]),
                N8_DES_KEY_LENGTH);
      }

      /* set up the post processing data */
      postData_p->macLength = packetObj_p->macLength;
      postData_p->computedMAC_p = computedMAC_p;
      postData_p->packetObj_p = packetObj_p;
      postData_p->verify_p = verify_p;
      postData_p->encryptedLength = length;
      if (request_p == NULL)
      {
         postData_p->copyData = N8_TRUE;
      }
      else
      {
         postData_p->copyData = N8_FALSE;
      }
      postData_p->resultPacket_p = result_p;

      /* tell the Queue Manager to copy back the command block results */
      req_p->copyBackCommandBlock = N8_TRUE;
      req_p->postProcessingData_p = (void *) postData_p;

      /* Generate the command blocks for the Decrypt */
      decryptFcn = (n8_SSLTLSFcn_t)packetObj_p->SSLTLScmdFcn;
      ret = decryptFcn(next_cb_p,
                       packetObj_p,
                       packet_p,
                       input_a,
                       res_a,
                       packetObj_p->decOpCode);
      CHECK_RETURN(ret);

      /* set up the return packet 
       * again, recall we cannot set the length in the result packet now and
       * will have to do it in the result handler. */
      memcpy(&result_p[SSLTLS_TYPE_OFFSET],
             &packet_p[SSLTLS_TYPE_OFFSET],
             sizeof(uint8_t) + sizeof(uint16_t));

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while (FALSE);

   /* clean up if necessary.  note that if there is no error condition, the post
    * data and request will be de-allocated in the result handler. */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* N8_SSLTLSDecryptVerify */

/*****************************************************************************
 * N8_SSLTLSDecryptVerify
 *****************************************************************************/
/** @ingroup n8_ssltls
 * @brief Decrypt and verify an entire SSL or TLS message.
 *
 * Decrypt an verify a SSL or TLS message.  If the verification fails, it will
 * be noted in the return value. Note that this call is now supported as a front
 * end to N8_SSLTLSDecryptVerifyMemory.
 *
 *  @param packetObj_p         RW:  Pre-initialized packet object.
 *  @param packet_p            RO:  A complete, encrypted SSL or TLS packet
 *                                  including the header.
 *  @param computedMAC_p       RW:  Pointer to result space for the MAC
 *                                  computation output.
 *  @param verify_p:           WO:  Pointer to a verify flag.  If the MAC's
 *                                  match, the verify flag will be set to N8_TRUE.
 *  @param result_p            RW:  Pointer to result space for the decrypted
 *                                  result. 
 *  @param request             RW:  Kernel request buffer used when mode =
 *                                  N8_PACKETMEMORY_REQUEST.
 *  @param event_p             RW:  Asynchronous event pointer.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error condition if raised.
 *
 * @par Errors
 *    N8_MALLOC_FAILED<br>
 *    N8_INVALID_INPUT_SIZE - the value in the packet header is too large or too
 *                            small.<br>
 *    N8_INCONSISTENT       - the version number in the packet header does not
 *                            match the designation in the packet object.<br>
 *    N8_INVALID_OBJECT     - packet object is not a valid object initialized
 *                            for SSL or TLS<br>
 *    N8_INVALID_VALUE      - the value of the type field in the packet header
 *                            is not one of the legal values for SSL or TLS.<br>
 *
 * @par Assumptions
 *    The result_p and computedMAC_p pointers refer to pre-allocated space of
 * sufficient size.  The calling program retains responsibility for freeing this
 * memory. 
 *****************************************************************************/
N8_Status_t 
N8_SSLTLSDecryptVerify(N8_Packet_t             *packetObj_p,
                       const N8_SSLTLSPacket_t *packet_p,
                       N8_Buffer_t             *computedMAC_p,
                       N8_Boolean_t            *verify_p,
                       N8_SSLTLSPacket_t       *result_p,
                       N8_Event_t              *event_p)
{
   N8_Status_t           retCode;
   N8_PacketMemoryMode_t saveMode;

   do
   {
      CHECK_OBJECT(packetObj_p, retCode);
      /* save the existing mode and set it */
      saveMode = packetObj_p->mode;
      packetObj_p->mode = N8_PACKETMEMORY_NONE;

      retCode = N8_SSLTLSDecryptVerifyMemory(packetObj_p,
                                             packet_p,
                                             computedMAC_p,
                                             verify_p,
                                             result_p,
                                             NULL,
                                             event_p);
      /* reset the mode */
      packetObj_p->mode = saveMode;

   } while (FALSE);

   return retCode;
}
/*****************************************************************************
 * N8_GetHashLen
 *****************************************************************************/
/** @ingroup n8_ssltls
 * @brief Compute the length of a hash.
 *
 * For a specified type of hash, return the length of the generated hash.
 *
 *  @param hash                RO:  Hash to be used
 *
 * @par Externals
 *    None
 *
 * @return
 *    Length of the hash.  Returns -1 if the hash is not supported.
 *
 * @par Errors
 *    See above.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
short int N8_GetHashLength(N8_HashAlgorithm_t hash)
{
   switch (hash)
   {
      case N8_MD5:
      case N8_HMAC_MD5:
         return MD5_HASH_RESULT_LENGTH;
      case N8_SHA1:
      case N8_HMAC_SHA1:
         return SHA1_HASH_RESULT_LENGTH;
      case N8_HMAC_MD5_96:
      case N8_HMAC_SHA1_96:
         return HMAC_96_HASH_RESULT_LENGTH;
      case N8_HASH_NONE:
         return 0;
      default:
         return -1;
   }
} /* N8_GetHashLength */
/*****************************************************************************
 * N8_ComputeEncryptedLength
 *****************************************************************************/
/** @ingroup n8_ssltls
 * @brief For a given message size, hash length and cipher, compute the
 * length of the encrypted message including padding.
 *
 *  @param size                RO:  size of the message
 *  @param hashLen             RO:  length of the hash
 *  @param cipher              RO:  cipher to use
 *
 * @par Externals
 *    None
 *
 * @return
 *    Length of the encrypted message (size + hash length + padding).
 *    Returns -1 if the cipher is not recognized.
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
short int N8_ComputeEncryptedLength(int size, int hashLen, N8_Cipher_t cipher)
{
   int el = -1;
   switch (cipher)
   {
      case N8_CIPHER_DES:
         el = (size + hashLen + 8) & ~0x07;
         break;
      case N8_CIPHER_ARC4:
         el = size + hashLen;
         break;
      default:
         break;
   }
   return el;
} /* N8_ComputeEncryptedLength */

/* local functions */
/**********************************************************************
 * resultHandlerSSLTLSEncrypt
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
static void resultHandlerSSLTLSEncrypt(API_Request_t* req_p)
{
   N8_Packet_t *packetObj_p = NULL;
   char *title = "resultHandlerSSLTLSEncrypt";

   /* get the packet object pointer from the request */
   packetObj_p = (N8_Packet_t *) req_p->postProcessingData_p;
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      /* copy the results back */
      if (req_p->copyBackFrom_p != NULL &&
          req_p->copyBackTo_p != NULL &&
          req_p->copyBackSize != 0)
      {
         memcpy(req_p->copyBackTo_p,
                req_p->copyBackFrom_p,
                req_p->copyBackSize);
      }

      /* Reload the IV in the cipher info if needed */
      if (packetObj_p->contextHandle.inUse == N8_FALSE)
      {
         int IVOffset = req_p->postProcessBuffer[0];
         memcpy(&(packetObj_p->cipherInfo.IV), 
                &(req_p->copyBackFrom_p[IVOffset]),
                N8_DES_KEY_LENGTH);
      }
   }
   else
   {
      /* an error has occured.  roll back the sequence number if we are not
       * using context memory. */

      /* if we are not using the context */
      if (packetObj_p->contextHandle.inUse == N8_FALSE)
      {
         /* decrement the sequence number to return it to its previous value */
         if (packetObj_p->cipherInfo.sequence_number[1] == 0)
         {
            /* decrement the sequence number ms */
            packetObj_p->cipherInfo.sequence_number[0]--;
         }
         /* decrement the sequence number ls */
         packetObj_p->cipherInfo.sequence_number[1]--;
      }
      RESULT_HANDLER_WARNING(title, req_p);
   }
   /* do not free the postProcessingData_p as it is a the packetObject
    * controlled by the user */

   /* do not free the request in the result handler.  it is done by the event
    * processor. */

} /* resultHandlerSSLTLSEncrypt */


/**********************************************************************
 * resultHandlerSSLTLSDecrypt
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
static void resultHandlerSSLTLSDecrypt(API_Request_t* req_p)
{
   char *title = "resultHandlerSSLTLSDecrypt";
   N8_Packet_t *packetObj_p = NULL;
   N8_SSL_Post_Decrypt_Data_t *postData_p = NULL;
   N8_SSLTLSPacket_t *resultPacket_p;
   EA_SSL30_DECRYPT_RESULT_CMD_BLOCK_t *cmd_p;
   int i;
   short int originalLength;
   N8_Buffer_t *buf_p;
   char padLength=0;
   char *data_p;

   postData_p =
   (N8_SSL_Post_Decrypt_Data_t *) req_p->postProcessingData_p; 
   packetObj_p = postData_p->packetObj_p;
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      /* get the packet object pointer from the request */
      resultPacket_p = postData_p->resultPacket_p;
      /* copy back the computed mac */
      /* isolate the command block */
      cmd_p = (EA_SSL30_DECRYPT_RESULT_CMD_BLOCK_t *)
              req_p->EA_CommandBlock_ptr;

      /* the copy back should come from the last command block */
      cmd_p = &cmd_p[req_p->numNewCmds - 1];

      /* If the computedMAC_p parameter from N8_EA_DV is NULL */
      /* then don't copy back the MAC value as the user doesn't want it */
      if (postData_p->computedMAC_p != NULL ) {
	      buf_p = postData_p->computedMAC_p;
	      for (i = 0; i < postData_p->macLength / sizeof(uint32_t); i++)
	      {
		 uint32_to_BE(cmd_p->mac[i], &buf_p[i*4]);
	      }
	}
      /* check the mac mismatch flag and set the verify value */
      *(postData_p->verify_p) =
      ((cmd_p->mismatch & EA_Result_MAC_Mismatch) == 0) ?
      N8_TRUE : N8_FALSE;

      /* set the length in the return packet. */
      /* data_p = (char *) &resultPacket_p[SSLTLS_DATA_OFFSET]; */
      data_p = (char *) req_p->copyBackFrom_p;
      if (packetObj_p->packetCipher != N8_CIPHER_ARC4)
      {
         padLength = data_p[postData_p->encryptedLength - 1] + 1;
      }
      originalLength = postData_p->encryptedLength - postData_p->macLength -
                       padLength;
      /* postData_p->returnLength = htons(originalLength); */
      SSLTLS_SET_LENGTH(resultPacket_p, originalLength);
      /* copy the results back */
      /* memcpy(req_p->copyBackTo_p, req_p->copyBackFrom_p,
       * req_p->copyBackSize); */
      if (postData_p->copyData == N8_TRUE)
      {
         memcpy(req_p->copyBackTo_p,
                req_p->copyBackFrom_p,
                postData_p->encryptedLength);
      }

      /* Reload the IV in the cipher info if needed */
      if (packetObj_p->contextHandle.inUse == N8_FALSE)
      {
         memcpy(&(packetObj_p->cipherInfo.IV), 
                &(postData_p->nextIV),
                N8_DES_KEY_LENGTH);
      }
   }
   else
   {
      /* an error has occured.  roll back the sequence number if we are not
       * using context memory. */

      /* if we are not using the context */
      if (packetObj_p->contextHandle.inUse == N8_FALSE)
      {
         /* decrement the sequence number to return it to its previous value */
         if (packetObj_p->cipherInfo.sequence_number[1] == 0)
         {
            /* decrement the sequence number ms */
            packetObj_p->cipherInfo.sequence_number[0]--;
         }
         /* decrement the sequence number ls */
         packetObj_p->cipherInfo.sequence_number[1]--;
      }
      RESULT_HANDLER_WARNING(title, req_p);
   }

   /* free the post processing data holder */
   /*N8_UFREE(req_p->postProcessingData_p);*/
   /* do not free the request in the result handler.  it is done by the event
    * processor. */
} /* resultHandlerSSLTLSDecrypt */


