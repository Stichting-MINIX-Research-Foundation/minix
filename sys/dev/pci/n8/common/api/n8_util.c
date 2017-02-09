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

static char const n8_id[] = "$Id: n8_util.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_util.c
 *  @brief N8 utilities
 *
 * Loose collection of utility functions used across the N8 methods.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/18/03 brr   Eliminate duplicate assignments & unused parameter passage.
 * 05/19/03 brr   Eliminate obsolete functions freeRequest & addBlockToFree.
 * 05/15/03 brr   Eliminate RNG error codes since they are not propigated to
 *                user space.
 * 04/25/03 brr   Queue callback events if either the usrData or usrCallback
 *                is not NULL so events without callbacks can N8_EventPoll.
 * 04/21/03 brr   Allocate PK requests from seperate pool.
 * 03/10/03 brr   Added support for API callbacks.
 * 08/05/02 bac   Fixed n8_printBuffer to line wrap correctly.
 * 07/14/02 bac   Modified freeRequest to only free those requests that were not
 *                allocated using N8_RequestAllocate.
 * 07/08/02 brr   Conditionally perform N8_QMgrDequeue to support device
 *                polling.
 * 06/10/02 hml   Added initializeEARequestBuffer.
 * 05/15/02 brr   Removed functions that handled chained requests.
 * 05/09/02 bac   Moved N8_CreateSizedBufferFromATTR, N8_CreateBuffer, and
 *                N8_CreateSizedBufferFromString elsewhere.
 * 05/07/02 msz   Handle a pend on synchronous requests.
 * 04/20/02 brr   Clear the API request & cmd blocks with memset.
 * 03/26/02 hml   Added n8_validateUnit.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 03/25/02 brr   Moved N8_CreateSizedBufferFromATTR from n8_test_util.c
 * 03/22/02 brr   Do not zero command blocks since KMALLOC now does so.
 * 03/08/02 brr   Zero command blocks since KMALLOC no longer zero's memory.
 * 03/06/02 brr   Fix print functions to compile in user & kernel space.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/26/02 brr   KMALLOC API requests.
 * 02/25/02 brr   Removed last references to the queue pointer.
 * 02/22/02 spm   Converted printk's to DBG's.
 * 01/31/02 brr   Removed separate memory allocation for the command blocks.
 * 01/23/02 brr   Removed obsolete addBlockToFree & removeBlockFromFreeList.
 * 01/23/02 brr   Modified functions to reduce the number if memory allocations.
 * 01/12/02 bac   Fixed a bug in _addBlockToFree to correctly mark entries if
 *                they are KERNEL.  Also changed n8_handleEvent to scan for the
 *                last request in the list.
 * 11/27/01 bac   Added n8_handleEvent method.
 * 11/11/01 bac   Added n8_sizedBufferCmp prototype.
 * 11/06/01 dkm   Added check for error ret from QMgr_get_control_struct.
 * 10/30/01 bac   Added n8_incrLength64 and n8_decrLength64
 * 10/11/01 hml   Added n8_strdup.
 * 10/08/01 hml   Added printHWError
 * 09/20/01 bac   Cleaned up signatures w.r.t. unsigned and const modifiers.
 * 09/18/01 bac   Modified createEARequestBuffer to accept the number of
 *                commands, allocate the command buffer, and set the numNewCmds
 *                field.
 * 09/14/01 bac   Support for command block printing.
 * 08/30/01 msz   Eliminated numCmdBlksProcessed from API_req_t
 * 08/27/01 msz   Renamed FcnCalledOnRequestCompletionOrError to callback
 * 08/24/01 bac   Modified createPKRequestBuffer to accept the number of
 *                commands, allocate the command buffer, and set the numNewCmds
 *                field.
 * 07/30/01 bac   Set the queue_p in the [PK|EA]RequestBuffer.
 * 07/20/01 bac   Added chip id to calls to create[PK|EA]RequestBuffer.
 * 07/02/01 mel   Fixed comments.
 * 06/25/01 bac   Bug fixes to free list management.
 * 06/19/01 bac   Added prototype for addMemoryStructToFree and updated
 *                freeRequest to handle N8_MemoryHandle_t entries
 * 06/17/01 bac   Added removeBlockFromFreeList
 * 05/19/01 bac   Removed free of postProcessingData.  Decision should live in
 *                the handler.
 * 05/19/01 bac   Fixed memory leak by freeing the free list when done
 *                and freeing postProcessingData_p when done.
 * 05/18/01 bac   Converted to N8_xMALLOC and N8_xFREE
 * 05/03/01 bac   Added include for string.h
 * 04/30/01 bac   Fixed a comment.
 * 04/26/01 bac   Added test to freeRequest to ensure the pointer is
 *                not null.
 * 04/24/01 bac   Original version.
 ****************************************************************************/
/** @defgroup api_util API Utility Functions
 */
#include "n8_pub_buffer.h"
#include "n8_pub_request.h"
#include "n8_util.h"
#include "n8_driver_api.h"
#include <opencrypto/cryptodev.h>

extern NSPdriverInfo_t  nspDriverInfo;
extern N8_Status_t N8_EventWait(N8_Event_t *events_p, const int count, int *ready_p);

/*****************************************************************************
 * printN8Buffer
 *****************************************************************************/
/** @ingroup api_util
 * @brief Prints N8_Buffer_t
 *
 * Given an N8_Buffer_t and the size, print the buffer as hex digits.
 *
 *  @param p                   RW:  pointer to buffer to print
 *  @param size                RW:  size of buffer
 *
 * @par Externals
 *    None
 *
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
void printN8Buffer(N8_Buffer_t *p, const unsigned int size)
{
   int i;
   unsigned char  *ptr = p;

   for (i=0; i < size; i++)
   {
      N8_PRINT( "%02x", (*ptr++) & 0xff);
      if (((i+1) % PK_Bytes_Per_BigNum_Digit) == 0)
      {
         N8_PRINT( "\n");
      }
   }
   N8_PRINT( "\n");
} /* printN8Buffer */

/*****************************************************************************
 * n8_displayBuffer
 *****************************************************************************/
/** @ingroup n8_bn
 * @brief Display a big number.
 *
 * Print a readable big number representation to STDOUT.
 *
 *  @param buf_p               RO:  the big number to display.
 *  @param length              RO:  length of big number in bytes.
 *  @param name                RO:  name to identify the big number.
 *
 * @par Externals
 *    None
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    Name is not null.
 *****************************************************************************/
void n8_displayBuffer(N8_Buffer_t *buf_p, const uint32_t length, const char *name)
{
   if (length != 0 && buf_p != NULL)
   {
      N8_PRINT( "%s  length=%d mem=0x%x\n", name, length, (unsigned int) buf_p);
      printN8Buffer(buf_p, length);
   }
} /* n8_displayBuffer */
/*****************************************************************************
 * createPKRequestBuffer
 *****************************************************************************/
/** @ingroup api_util
 * @brief Allocate and initialize request buffer
 *
 * Initialize a PK Request Buffer before use.
 *
 *  @param req_pp              RW:  Pointer-to-pointer of structure to be initialized
 *  @param numCmds             RO:  number of commands in this request
 *  @param callbackFcn         RO:  pointer to callback function
 *  @param dataBytes           RO:  number of bytes to allocate for data
 *
 * @par Externals
 *    gRequestIndex            RW:  counter is incremented
 *
 * @par Errors
 *    None<br>
 *
 * @par Assumptions
 *    Allocated space will be freed by the caller.
 *****************************************************************************/
N8_Status_t createPKRequestBuffer(API_Request_t **req_pp,
                                  const N8_Unit_t chip,
                                  const unsigned int numCmds,
                                  const void *callbackFcn,
                                  const unsigned int dataBytes)
{
   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p;
   N8_MemoryHandle_t  *kmem_p;
   do
   {
      kmem_p = N8_AllocateBufferPK(sizeof(API_Request_t) + 
                           (numCmds * sizeof(PK_CMD_BLOCK_t)) + dataBytes);
   
      if (kmem_p == NULL)
      {
         ret = N8_MALLOC_FAILED;
         break;
      }
      req_p = (API_Request_t *)kmem_p->VirtualAddress;
      *req_pp = req_p;
      memset(*req_pp, 0x0, sizeof(API_Request_t) + (numCmds * sizeof(PK_CMD_BLOCK_t)) + dataBytes);
      (*req_pp)->qr.requestStatus     = N8_QUEUE_REQUEST_SENT_FROM_API;
      (*req_pp)->qr.requestError      = N8_QUEUE_REQUEST_OK;
      (*req_pp)->qr.unit              = N8_PKP;
      (*req_pp)->qr.chip              = chip;
      (*req_pp)->qr.callback          = callbackFcn;
      (*req_pp)->qr.physicalAddress   = kmem_p->PhysicalAddress;
      (*req_pp)->qr.synchronous       = N8_FALSE;

      /* set ALL nonzero values in the API Request */
      (*req_pp)->numNewCmds           = numCmds;
      (*req_pp)->copyBackCommandBlock = N8_FALSE;
      (*req_pp)->PK_CommandBlock_ptr = (PK_CMD_BLOCK_t *)
                                        ((int)(*req_pp) + sizeof(API_Request_t));
      (*req_pp)->dataoffset           = sizeof(API_Request_t) + 
                                              (numCmds * sizeof(PK_CMD_BLOCK_t));
   }
   while (FALSE);
   return ret;
} /* createPKRequestBuffer */

/*****************************************************************************
 * initializeEARequestBuffer
 *****************************************************************************/
/** @ingroup api_util
 * @brief Initialize request buffer
 *
 * Initialize a EA Request Buffer before use.
 *
 *  @param req_p               RW:  Pointer of structure to be initialized
 *  @param numCmds             RO:  number of commands in this request
 *  @param callbackFcn         RO:  pointer to callback function
 *  @param dataBytes           RO:  number of bytes to allocate for data
 *
 * @par Externals
 *    None
 *
 * @par Errors
 *    None<br>
 *
 * @par Assumptions
 *    Allocated space will be freed by the caller.
 *****************************************************************************/

void initializeEARequestBuffer(API_Request_t      *req_p,
                               N8_MemoryHandle_t  *kmem_p,
                               const N8_Unit_t     chip,
                               const unsigned int  numCmds,
                               const void         *callbackFcn,
                               N8_Boolean_t        userRequest)
{
      memset(req_p, 0x0, sizeof(API_Request_t) + (numCmds * sizeof(EA_CMD_BLOCK_t)));
      req_p->qr.requestStatus     = N8_QUEUE_REQUEST_SENT_FROM_API;
      req_p->qr.requestError      = N8_QUEUE_REQUEST_OK;
      req_p->qr.unit              = N8_EA;
      req_p->qr.chip              = chip;
      req_p->qr.callback          = callbackFcn;
      req_p->qr.physicalAddress   = kmem_p->PhysicalAddress;
      /* req_p->qr.synchronous       = N8_FALSE; */
      req_p->userRequest          = userRequest;

      /* set ALL nonzero values in the API Request */
      req_p->EA_CommandBlock_ptr    = (EA_CMD_BLOCK_t *)
                                         ((int)(req_p) + sizeof(API_Request_t));
      req_p->numNewCmds             = numCmds;
      /* req_p->copyBackCommandBlock   = N8_FALSE; */
      if (userRequest == N8_TRUE)
      {
         req_p->dataoffset          = sizeof(API_Request_t) + 
                                          N8_COMMAND_BLOCK_BYTES + N8_CTX_BYTES;
      }
      else
      {
         req_p->dataoffset          = sizeof(API_Request_t) + 
                                          (numCmds * sizeof(EA_CMD_BLOCK_t));
      }
} /* initializeEARequestBuffer */

/*****************************************************************************
 * createEARequestBuffer
 *****************************************************************************/
/** @ingroup api_util
 * @brief Allocate and initialize request buffer
 *
 * Initialize a EA Request Buffer before use.
 *
 *  @param req_pp              RW:  Pointer-to-pointer of structure to be initialized
 *  @param numCmds             RO:  number of commands in this request
 *  @param callbackFcn         RO:  pointer to callback function
 *  @param dataBytes           RO:  number of bytes to allocate for data
 *
 * @par Externals
 *    None
 *
 * @par Errors
 *    None<br>
 *
 * @par Assumptions
 *    Allocated space will be freed by the caller.
 *****************************************************************************/

N8_Status_t createEARequestBuffer(API_Request_t **req_pp,
                                  const N8_Unit_t chip,
                                  const unsigned int numCmds,
                                  const void *callbackFcn,
                                  const unsigned int dataBytes)
{
   N8_Status_t ret = N8_STATUS_OK;
   N8_MemoryHandle_t  *kmem_p;
   do
   {
      kmem_p = N8_KMALLOC(sizeof(API_Request_t) +
                           (numCmds * sizeof(EA_CMD_BLOCK_t)) + dataBytes);
 
      if (kmem_p == NULL)
      {
         ret = N8_MALLOC_FAILED;
         break;
      }
      *req_pp = (API_Request_t *)kmem_p->VirtualAddress;

      initializeEARequestBuffer(*req_pp, 
                                 kmem_p,
                                 chip, 
                                 numCmds, 
                                 callbackFcn,
                                 N8_FALSE); 
   }
   while (FALSE);
   return ret;
} /* createEARequestBuffer */

N8_Status_t n8_handleEvent(N8_Event_t *e_p,
                           void *hReq_p,
                           N8_Unit_t unit,
                           N8_QueueStatusCodes_t requestStatus)
{
   N8_Status_t ret = N8_STATUS_OK;
   QMgrRequest_t *req_p;
   
   req_p = (QMgrRequest_t *)hReq_p;
   
   if (e_p != NULL)
   {
      /* This is an asynchronous request                                */
      e_p->state =  (void *) req_p;
      e_p->status = requestStatus;
      e_p->unit = unit;
#ifdef SUPPORT_CALLBACK_THREAD
      if ((e_p->usrCallback != NULL) ||
          (e_p->usrData != NULL))
      {
         ret = queueEvent(e_p);
      }
#endif
   }                                                          
   else
   {
      /* This is an synchronous request.  Therefore we don't need an    */
      /* event, we know that the request has completed.                 */
#ifdef SUPPORT_DEVICE_POLL
      N8_QMgrDequeue();
#endif
      if ( req_p->requestError != N8_QUEUE_REQUEST_ERROR )
      {
         /* Do the callback if needed. */
         if ( req_p->callback != NULL )
         {
            req_p->callback( req_p );
         }
         /* Free the request if the event has completed successfully. */
         /* If not the error handling should free the request.        */
         if (((API_Request_t *)req_p)->userRequest != N8_TRUE)
         {
            /* Don't delete "user allocated/ pooled" requests */
            freeRequest( (API_Request_t *)req_p );
         }
      }
      else
      {
         ret = N8_HARDWARE_ERROR;
      }
   }
   return ret;
} /* n8_handleEvent */

void printCommandBlock(uint32_t *u, unsigned int size)
{
   int k;
   int num = size / 4;
   for (k = 0; k < num; k++)
   {
      N8_PRINT( "%08x", u[k]);
      if ((k+1) % 8)
      {
         N8_PRINT( "|");
      }
      else
      {
         if (k < (num - 1))
         {
            N8_PRINT(( "\n     "));
         }
         else
         {
            N8_PRINT(( "\n"));
         }
      }
   }
} /* printCommandBlock */

void printPKCommandBlocks(const char *title, PK_CMD_BLOCK_t *block_p, uint32_t num_blocks)
{
   int i = 0;
   char * ptr;

   N8_PRINT( "%s\n", title);
   for(i = 0; i < num_blocks; i++)
   {
      ptr = (char *) &block_p[i];
      N8_PRINT( "%3d: ", i+1);
      printCommandBlock((uint32_t *) &block_p[i], sizeof(PK_CMD_BLOCK_t));
   }
} /* printPKCommandBlocks */

void printEACommandBlocks(const char *title, EA_CMD_BLOCK_t *block_p, uint32_t num_blocks)
{
   int i = 0;
   char * ptr;

   N8_PRINT( "%s\n", title);
   for(i = 0; i < num_blocks; i++)
   {
      ptr = (char *) &block_p[i];
      N8_PRINT( "%3d: ", i+1);
      printCommandBlock((uint32_t *) &block_p[i], sizeof(EA_CMD_BLOCK_t));
   }
} /* printEACommandBlocks */

static void printErrorCode(uint32_t errorCode, uint32_t errors[], const char *errorNames[], unsigned int num)
{
   int i;
   for (i = 0; i < num; i++)
   {
      if (errorCode & errors[i])
      {
         N8_PRINT( "   %s\n", errorNames[i]);
      }
   }
} /* printError */

void printHWError(uint32_t errCode, N8_Component_t unit)
{
   uint32_t PKErrors[] =
   {
      PK_Status_SKS_Offset_Error,     
      PK_Status_Length_Error,         
      PK_Status_Opcode_Error,         
      PK_Status_Q_Align_Error,        
      PK_Status_Read_Data_Error,      
      PK_Status_Write_Data_Error,     
      PK_Status_Read_Opcode_Error,    
      PK_Status_Access_Error,         
      PK_Status_Reserved_Error,       
      PK_Status_Timer_Error,          
      PK_Status_Prime_Error,          
      PK_Status_Invalid_B_Error,      
      PK_Status_Invalid_A_Error,      
      PK_Status_Invalid_M_Error,      
      PK_Status_Invalid_R_Error,      
      PK_Status_PKE_Opcode_Error
   };
   const char *PKErrorNames[] =
   {
      "PK_Status_SKS_Offset_Error",     
      "PK_Status_Length_Error",         
      "PK_Status_Opcode_Error",         
      "PK_Status_Q_Align_Error",        
      "PK_Status_Read_Data_Error",      
      "PK_Status_Write_Data_Error",     
      "PK_Status_Read_Opcode_Error",    
      "PK_Status_Access_Error",         
      "PK_Status_Reserved_Error",       
      "PK_Status_Timer_Error",          
      "PK_Status_Prime_Error",          
      "PK_Status_Invalid_B_Error",      
      "PK_Status_Invalid_A_Error",      
      "PK_Status_Invalid_M_Error",      
      "PK_Status_Invalid_R_Error",      
      "PK_Status_PKE_Opcode_Error"
   };
   uint32_t EAErrors[] =
   {
      EA_Status_Q_Align_Error,
      EA_Status_Cmd_Complete,
      EA_Status_Opcode_Error,
      EA_Status_CMD_Read_Error,
      EA_Status_CMD_Write_Error,
      EA_Status_Data_Read_Error,
      EA_Status_Data_Write_Error,
      EA_Status_EA_Length_Error,
      EA_Status_Data_Length_Error,
      EA_Status_EA_DES_Error,
      EA_Status_DES_Size_Error,
      EA_Status_DES_Parity_Error,
      EA_Status_ARC4_Error,
      EA_Status_MD5_Error,
      EA_Status_SHA1_Error,
      EA_Status_Access_Error
    };
   const char *EAErrorNames[] =
   {
      "EA_Status_Q_Align_Error",
      "EA_Status_Cmd_Complete",
      "EA_Status_Opcode_Error",
      "EA_Status_CMD_Read_Error",
      "EA_Status_CMD_Write_Error",
      "EA_Status_Data_Read_Error",
      "EA_Status_Data_Write_Error",
      "EA_Status_EA_Length_Error",
      "EA_Status_Data_Length_Error",
      "EA_Status_EA_DES_Error",
      "EA_Status_DES_Size_Error",
      "EA_Status_DES_Parity_Error",
      "EA_Status_ARC4_Error",
      "EA_Status_MD5_Error",
      "EA_Status_SHA1_Error",
      "EA_Status_Access_Error"
   };

   switch (unit)
   {
      case N8_PKP:
         DBG(( "PK Errors encountered:\n"));
         printErrorCode(errCode, PKErrors, PKErrorNames,
                        sizeof(PKErrors) / sizeof(uint32_t));
         break;
      case N8_RNG:
         DBG(( "RNG Errors encountered:\n"));
         break;
      case N8_EA:
         DBG(( "EA Errors encountered:\n"));
         printErrorCode(errCode, EAErrors, EAErrorNames,
                        sizeof(EAErrors) / sizeof(uint32_t));
      default:
         break;
   }
 
} /* printHWError */

/*****************************************************************************
 * n8_strdup
 *****************************************************************************/
/** @ingroup api_util
 * @brief Net Octave portable implementation of strdup
 *
 * Some compilers don't see strdup as an ansi function, so we wrote
 * our own.
 *
 * @param str_p RO: The string to duplicate.
 *
 * @par Externals:
 *    None.
 *
 * @return
 *    This function returns a pointer to the newly allocated copy of
 *    the string, or NULL if the input pointer is NULL or the
 *    N8_UMALLOC fails.
 *
 * @par Errors:
 *    See above.
 * 
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    The caller of this function takes the responsibility of
 *    freeing the string.
 *****************************************************************************/
char *n8_strdup(const char *str_p)
{
   char *ret_p;
   int   nBytes;

   if (str_p == NULL)
   {
      return(NULL);
   }

   nBytes = strlen(str_p);

   ret_p = N8_UMALLOC(nBytes + 1);

   if (ret_p == NULL)
   {
      return(NULL);
   }

   memcpy(ret_p, str_p, nBytes + 1);
   return(ret_p);
} /* n8_strdup */

/*****************************************************************************
 * n8_incrLength64
 *****************************************************************************/
/** @ingroup api_util
 * @brief Given a 64 bit length field, split across two 32 bit quantities,
 * properly increment by a given length.
 *
 *  @param hi_word_p           RW:  pointer to higher order word of length
 *  @param lo_word_p           RW:  pointer to lower order word of length
 *  @param length              RO:  increment amount
 *
 * @par Externals
 *    None
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
void n8_incrLength64(uint32_t *hi_word_p,
                     uint32_t *lo_word_p,
                     const uint32_t  length)
{
   uint32_t incrementLength;
   /* accumulate the length */
   incrementLength = *lo_word_p + length;
   if (incrementLength < *lo_word_p)
   {
      (*hi_word_p)++;
   }
   *lo_word_p = incrementLength;
} /* n8_incrLength64 */

/*****************************************************************************
 * n8_decrLength64
 *****************************************************************************/
/** @ingroup api_util
 * @brief Given a 64 bit length field, split across two 32 bit quantities,
 * properly decrement by a given length.
 *
 *  @param hi_word_p           RW:  pointer to higher order word of length
 *  @param lo_word_p           RW:  pointer to lower order word of length
 *  @param length              RO:  decrement amount
 *
 * @par Externals
 *    None
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
void n8_decrLength64(uint32_t *hi_word_p,
                     uint32_t *lo_word_p,
                     const uint32_t  length)
{
   uint32_t decrementLength;
   /* accumulate the length */
   decrementLength = *lo_word_p - length;
   if (decrementLength > *lo_word_p)
   {
      (*hi_word_p)--;
   }
   *lo_word_p = decrementLength;
} /* n8_decrLength64 */

/*****************************************************************************
 * n8_sizedBufferCmp
 *****************************************************************************/
/** @ingroup api_util
 * @brief Performs comparison between two N8_SizedBuffer_t.
 *
 * Compare the values of a and b.  Return<br>
 *
 *  @param a_p                 RO:  pointer to first N8_SizedBuffer_t
 *  @param b_p                 RO:  pointer to second N8_SizedBuffer_t
 *
 * @par Externals
 *    None
 *
 * @return
 * -1: if a <  b         <br>
 *  0: if a == b         <br>
 *  1: if a >  b         <br>
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
int n8_sizedBufferCmp(const N8_SizedBuffer_t *a_p,
                      const N8_SizedBuffer_t *b_p)
{
   if ((a_p == NULL) || (b_p == NULL))
   {
      if (a_p != NULL)
      {
         return -1;
      }
      else if (b_p != NULL)
      {
         return 1;
      }
      else
      {
         return 0;
      }
   }

   if (a_p->lengthBytes == b_p->lengthBytes)
   {
      int cmp;
      cmp = memcmp(a_p->value_p, b_p->value_p, a_p->lengthBytes);
      if (cmp == 0)
      {
         return 0;
      }
      else if (cmp > 0)
      {
         return 1;
      }
      else
      {
         return -1;
      }
   }
   else if (a_p->lengthBytes < b_p->lengthBytes)
   {
      return -1;
   }
   else
   {
      return 1;
   }
} /* n8_sizeBufferCmp */

/**********************************************************************
 * resultHandlerHashEnd
 *
 * Description:
 * This function is called by the Public Key Request Queue handler when
 * either the request is completed successfully and all the commands
 * copied back to the command blocks allocated by the API, or when
 * the Simon has encountered an error with one of the commands such 
 * that the Simon has locked up.
 *
 * Copy the results from the kernel buffer to the buffer originally
 * supplied by the user.  The kernel buffer must be the virtual address
 * of the N8_MemoryHandle_t. 
 *
 * Note this function will have to be NON-BLOCKING or it will lock up the
 * queue handler!
 *
 * NOTE: This function is also used by N8_HashCompleteMessage.
 * 
 *
 **********************************************************************/
/*****************************************************************************
 * resultHandlerGeneric
 *****************************************************************************/
/** @ingroup api_util
 * @brief Generic result handler -- a callback function for API requests.
 *
 * This function is called by the Queue Manager when either the request is
 * completed successfully or terminated upon an error.  This generic version
 * checks for success and if found will copy results back to the user space
 * using the generic mechanism copyBackFrom_p -> copyBackTo_p a length of
 * copyBackSize bytes.
 *
 *  @param req_p               RW:  API request structure for the request which
 *                                  has just completed.
 *
 * @par Externals
 *    None
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    The request is freed elsewhere.  No unusual processing must be done.  If
 *    such processing is required, a custom result handler must be written.
 *****************************************************************************/
void resultHandlerGeneric(API_Request_t* req_p)
{
   const char *title = "resultHandlerGeneric";
   if (req_p->qr.requestError == N8_QUEUE_REQUEST_OK)
   {
      DBG(("%s call-back with success\n", title));
      /* perform the copy back if necessary */
      if (req_p->copyBackSize != 0 &&
          req_p->copyBackFrom_p != NULL &&
	  (req_p->copyBackTo_p != NULL || req_p->copyBackTo_uio != NULL))
      {
	 if (req_p->copyBackTo_p != NULL) {
#if N8DEBUG
	     int size = req_p->copyBackSize;
	     int i;
	     if (size > 20) size=20;
	     DBG(("memcpy %d bytes\n",req_p->copyBackSize));
	     printf("result: ");
	     for (i=0; i<size; i+=4) {
	        printf("%08x ", *(uint32_t *)
				(&req_p->copyBackFrom_p[i]));
	     }
	     printf("\n");
#endif
	     memcpy(req_p->copyBackTo_p, req_p->copyBackFrom_p,
		    req_p->copyBackSize);
	 } else {
#if N8DEBUG
	     DBG(("cuio_copyback %d bytes\n",req_p->copyBackSize));
	     DBG(("result: %08x|%08x|%08x|%08x\n",
		      *(((uint32_t *)req_p->copyBackFrom_p)),
		      *(((uint32_t *)req_p->copyBackFrom_p)+1),
		      *(((uint32_t *)req_p->copyBackFrom_p)+2),
		      *(((uint32_t *)req_p->copyBackFrom_p)+3)));
#endif
	     cuio_copydata(req_p->copyBackTo_uio, 0, req_p->copyBackSize, 
		     req_p->copyBackFrom_p);
	 }
      }
   }
   else
   {
      RESULT_HANDLER_WARNING(title, req_p);
   }
   /* do not free the request here */
} /* resultHandlerGeneric */

/*****************************************************************************
 * n8_verifyunit
 *****************************************************************************/
/** @ingroup util
 * @brief Check to see if a specified unit ID is valid.  Does not resolve
 *        N8_ANY_UNIT, but will allow it.
 *
 *  @param chip       RO:  Chip number to verify.
 *
 *
 * @return
 *    An N8_Boolean_t indicating whether or not the unit is valid.
 *
 * @par Errors
 *    N8_STATUS_OK, N8_MALLOC_FAILED
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Boolean_t n8_validateUnit(N8_Unit_t chip)
{
   if (chip == N8_ANY_UNIT || chip < nspDriverInfo.numChips)
   {
      return (N8_TRUE);
   }
   else
   {
      return (N8_FALSE);
   }
} /* n8_validateUnit */
