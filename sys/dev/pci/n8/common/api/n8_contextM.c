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

static char const n8_id[] = "$Id: n8_contextM.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_contextM.c
 *  @brief Contains Crypto Context management Interface.
 *
 * Managing the context memory is left to the user. 
 * SAPI provides interfaces to allocate and initialize a context entry, and to
 * free an entry.  The user must allocate and initialize a context entry before
 * it can be used (presumably when an SSL connection is opened), specify which
 * context entry to use in the calls that require context memory, and finally
 * free the  context entry when finished (presumably when an SSL connection is
 * closed). Limited error checking is done, and if the user makes errors such as
 * specifying the wrong context entry for a call (e.g., specifying a context
 * entry containing an ARC4 key for encrypting an SSL packet on a connection
 * established with DES in the cipher suite), incorrect results will be
 * produced without a warning or error occurring.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/20/03 brr   Remove obsolete functions resultHandlerFreeContext & 
 *                n8_numCommandsToLoadContext
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 03/29/02 hml   Now validate the unit and the context in 
 *                N8_Read/WriteContext. (BUGS 657, 658).
 * 03/26/02 hml   Enhanced N8_AllocateContext and N8_FreeContext to retrieve
 *                the status from the lower level calls. (Bugs 637,642).
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 03/22/02 hml   Removed unneeded sessionID param from call to 
 *                N8_ContextMemAlloc.
 * 03/18/02 brr   Removed obsolete references to queue_p & validateContextHandle.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/18/02 brr   Support queue selection.
 * 02/15/02 brr   Moved context memory management to the driver.
 * 01/25/02 bac   Changed allocation search algorithm to always start at the
 *                index after the one last assigned.  This should be a much
 *                better search than simply starting at the beginning every
 *                time, which clusters the allocated contexts from 0..n.
 * 01/22/02 bac   Added function n8_numCommandsToLoadContext.
 * 01/22/02 bac   Conditionally removed the clearing of context memory when
 *                N8_FreeContext is called.
 * 12/07/01 brr   Removed userID to reduce memory consumption until supported.
 * 12/07/01 mel   Fixed bug #404: added checks for aligned address.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/12/01 hml   Fixed bug for freeing lock not held.
 * 11/08/01 mel   Fixed bug #289 : by passing unitID parameter
 * 11/08/01 mel   Fixed comments (bug #292).
 * 11/08/01 mel   Fixed bug #299 : N8_ReadContext does not check length parameter.
 *                Fixed bug #300 : N8_WriteContext does not check length parameter.
 * 11/07/01 hml   Ensured that validateContextHandle only release a semaphore
 *                if it has acquired it (BUG 236).
 * 11/05/01 hml   Added some error checking and the structure verification.
 * 10/11/01 brr   Fixed memory leak in N8_ReadContext.
 * 10/02/01 bac   Added use of RESULT_HANDLER_WARNING in all result handlers.
 * 10/01/01 hml   Completed multichip support.
 * 09/20/01 hml   Added multichip support.
 * 09/20/01 bac   The interface to the command block generators changed and now
 *                accept the command block buffer.  All calls to cb_ea methods
 *                changed herein.
 * 09/18/01 bac   Reformat to standardize, changed interaction with cb_ea
 *                functions.
 * 09/13/01 mel   Added event parameter to N8_FreeContext.
 * 08/29/01 mel   Fixed bug #191 : bad DBG print statement.
 * 08/27/01 mel   Added Write/Read context APIs.
 * 08/20/01 hml   Fixed bug in allocate context getting wrong semaphore.
 * 08/08/01 hml   Added use of semaphore to protect the context table.
 * 08/06/01 bac   Fixed a bug in the result handler where it was freeing a
 *                request though it should not.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/30/01 bac   Pass chip id to createEARequestBuffer.
 * 06/25/01 bac   Small bug fix for allocationg ccm_register_gp.
 * 06/20/01 mel   Corrected use of kernel memory.
 * 05/21/01 bac   Converted to use N8_ContextHandle_t.
 * 05/04/01 bac   Removed include of n8_define as it duplicated n8_util
 *                definitions.
 * 04/16/01 mel   Original version.
 ****************************************************************************/
/** @defgroup Context Context Management
 */
 

#include "n8_cb_ea.h" /* contains encryption/authentication qeueu declarations */

#include "n8_util.h"
#include "n8_API_Initialize.h"
#include "n8_common.h"
#include "n8_malloc_common.h"

 
/*****************************************************************************
 * N8_AllocateContext
 *****************************************************************************/
/** @ingroup Context
 * @brief Allocates a context entry for future use.
 *
 * The context index of the allocated context memory entry is returned in
 * contextIndex.  The context memory entry must then be initialized by a call
 * to Encrypt Initialize or Packet Initialize before it can be used in
 * encrypt/decrypt or packet level operations.  This call can fail if all
 * context entries have been allocated.  To avoid running out of context entries,
 * N8_FreeContext should be called when a context entry is no longer needed.
 *
 * @param contextHandle_p WO: Unsigned integer >= 0 and less than 
 * 2 power of 18 specifying the context entry in context memory allocated to
 * the caller and whose contents are initialized with Context.
 *
 * @return 
 *    contextHandler_p - context entry in allocated context memory.
 *    ret          - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Locks:
 *    This function acquires the context list semaphore from the queue structure.
 *
 * @par Errors:
 *    N8_NO_MORE_RESOURCE - There are no more context memory entries available
 *                          for allocation.  The call may succeed at a later
 *                          time if context entries are freed.
 *    N8_MALLOC_FAILED    - for some reasons malloc is failed
 *   
 *****************************************************************************/
N8_Status_t N8_AllocateContext(N8_ContextHandle_t *contextHandle_p, 
                               N8_Unit_t unitID)

{

   N8_Status_t     ret = N8_STATUS_OK; /* the return status: OK or ERROR */
   unsigned int    index;
   N8_Unit_t       validatedUnit = unitID;

   DBG(("N8_AllocateContext\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(contextHandle_p, ret);

      ret = N8_ContextMemAlloc(&validatedUnit, &index);

      if (ret == N8_STATUS_OK)
      {
         contextHandle_p->index = index;  /* return allocated context index */
         contextHandle_p->inUse = N8_TRUE;
         contextHandle_p->unitID = validatedUnit;
         /* set the structure pointer to the correct state */
         contextHandle_p->structureID = N8_CONTEXT_STRUCT_ID;

         DBG(("Context index %d\n", index));
      }
   } while(FALSE);

   DBG(("N8_AllocateContext - FINISHED\n"));

   return ret;
} /* N8_AllocateContext */


/*****************************************************************************
 * N8_FreeContext
 *****************************************************************************/
/** @ingroup Context
 * @brief Frees previously allocated context entry and allows for its later re-use.
 *
 * ContextIndex specifies the context memory entry to be deallocated.  The contents
 * of the entry are cleared to zero when this call is made. Any further calls made
 * with this ContextIndex value will fail (until the entry is re-allocated by an
 * N8_AllocateContext call). Context memory entries must be freed when no
 * longer needed to avoid exhausting the supply. Note that by adding the 
 * MEMORY_IN_PROGRESS state we are able to avoid having to use the context
 * lock in the callback function.
 *
 * @param ctxHndl RO: A previously allocated context entry value as returned
 * from N8_AllocateContext that is to be freed.
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 * N8_INVALID_PARAMETER     - The value of contextIndex is less than 0 or more
 *                            than (2 power of 18) - 1.
 * N8_UNALLOCATED_CONTEXT   - contextIndex does not specify an allocated context
 *                            entry index.
 *   
 * @par Locks:
 *    This function acquires the context list semaphore from the queue structure.
 *    Note that we make sure the lock is released before we queue the request.
 *
 * @par Assumptions:
 *  Limited error checking is done, and if the user makes errors such as
 *  specifying the wrong context entry for a call (e.g., specifying a context
 *  entry containing an ARC4 key for encrypting an SSL packet on a connection
 *  established with DES in the cipher suite), incorrect results will be
 *  produced without a warning or error occurring    
 *****************************************************************************/
N8_Status_t N8_FreeContext(N8_ContextHandle_t ctxHndl,
                           N8_Event_t        *event_p)

{
   N8_Status_t     ret = N8_STATUS_OK;    /* the return status: OK or ERROR */

   DBG(("N8_FreeContext\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      
      if (ctxHndl.inUse != N8_TRUE)
      {
         ret = N8_UNALLOCATED_CONTEXT;
         break;
      }
      
      if (ctxHndl.structureID != N8_CONTEXT_STRUCT_ID)
      {
         ret = N8_INVALID_PARAMETER;
         break;
      }

      ret = N8_ContextMemFree(ctxHndl.unitID, ctxHndl.index);

      DBG(("ctxHndl.index  = %d\n", ctxHndl.index));

   } while(FALSE);

   if (event_p != NULL)
   {
      N8_SET_EVENT_FINISHED(event_p, N8_EA);
   }

   DBG(("N8_FreeContext - FINISHED\n"));

   return ret;

} /* N8_FreeContext */

/*****************************************************************************
 * N8_WriteContext
 *****************************************************************************/
/** @ingroup Context
 * @brief Writes a context entry for future use.
 *
 * Writes ContextLength number of bytes from Context into the crypto controller
 * context memory entry denoted by ContextHandle.  Up to 512 bytes may be loaded
 * into a context entry; if less than 512 bytes are supplied the remaining bytes
 * will be set to bytes of 0. The content / format of the bytes depends on the
 * intended use, and is specified in the Crypto Controller specification [CCH].
 * However this routine simply treats them as  uninterpreted  values; no checking
 * or processing of the bytes written to the context entry is done. All previous
 * contents of the context entry are overwritten and lost. It is important to note
 * that the context information is treated by the hardware as a sequence of 32-bit
 * values; the byte values written to a hardware context by this call are written
 * by the hardware as a series of 32-bit quantities, and these 32-bit values must
 * be in the appropriate big endian or little endian format depending on the byte
 * order of the host processor. A context read from the hardware via N8_ReadContext
 * is read in this same way, and is always in the proper format to be written back
 * via N8_WriteContext
 *
 * @param contextHandle     RO:     The handle of a previously allocated context
 *                                  entry as returned by N8_AllocateContext.
 
 * @param context_p         RO:     The bytes to be written to the context entry
 * @param contextLength     RO:     Length of Context in bytes, from 0 to 512
 *                                  bytes inclusive.  A  length of 0 is legal,
 *                                  but no bytes will actually be written
 * @param event_p           RW:     On input, if null the call is synchronous 
 *                                  and no event is returned. The operation is 
 *                                  complete when the call returns. If non-null, 
 *                                  then the call is asynchronous; an event is 
 *                                  returned that can be used to determine when 
 *                                  the operation completes
 *
 *
 * @return 
 *    ret          - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Locks:
 *    This function acquires the context list semaphore from the queue structure.
 *
 * @par Errors:
 *    N8_NO_MORE_RESOURCE - There are no more context memory entries available
 *                          for allocation.  The call may succeed at a later
 *                          time if context entries are freed.
 *    N8_MALLOC_FAILED    - for some reasons malloc is failed
 *    N8_INVALID_INPUT_SIZE  - context length is more than 512
 *    N8_UNALIGNED_ADDRESS - passed address (context_p) is not 32-bit aligned 
 *   
 *****************************************************************************/
N8_Status_t N8_WriteContext(N8_ContextHandle_t contextHandle,
                            N8_Buffer_t        *context_p, 
                            uint32_t           contextLength, 
                            N8_Event_t         *event_p )

{

   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
   API_Request_t  *req_p = NULL;      /* request buffer */
   N8_Boolean_t             unitValid;

   DBG(("N8_WriteContext\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(context_p, ret);

      if ((int)context_p % 4)
      {
         ret = N8_UNALIGNED_ADDRESS;
         break;
      }

      if (contextLength > EA_CTX_Record_Byte_Length)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      CHECK_STRUCTURE(contextHandle.structureID, 
                      N8_CONTEXT_STRUCT_ID, 
                      ret);

      unitValid = n8_validateUnit(contextHandle.unitID);
      if (!unitValid)
      {
         ret= N8_INVALID_UNIT;
         break;
      }

      ret = N8_ContextMemValidate(contextHandle.unitID, contextHandle.index);
      CHECK_RETURN(ret);                               

      /* allocate request buffer */
      ret = createEARequestBuffer(&req_p,
                                  contextHandle.unitID,
                                  N8_CB_EA_WRITECONTEXT_NUMCMDS,
                                  resultHandlerGeneric,
                                  CONTEXT_ENTRY_SIZE);
      CHECK_RETURN(ret);                               

      /* create "write buffer to context memory" command */
      ret = cb_ea_writeContext(req_p,
                               req_p->EA_CommandBlock_ptr, 
                               contextHandle.index,
                               context_p,
                               contextLength);
      CHECK_RETURN(ret);
      /* nothing needs to be done in the result handler.  it will be called only
       * to print a debug messag if an error occurs. */

      /* send command to write to specified context memory */
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

   } while(FALSE);

   DBG(("N8_WriteContext - FINISHED\n"));

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* N8_WriteContext */


/*****************************************************************************
 * N8_ReadContext
 *****************************************************************************/
/** @ingroup Context
 * @brief Reads a context entry.
 *
 * Copies ContextLength number of bytes from the crypto controller context
 * memory context entry specified by ContextHandle  into  Context.  The contents
 * of the context memory entry are unchanged. Up to 512 bytes [This value should
 * really be some sort of configuration constant or value defined that the user
 * can determine at run time.] may be read from a context entry. If the
 * specified context entry has not previously been initialized with
 * N8_EncryptInitialize or N8_PacketInitialize or written with an N8_WriteContext
 * call, then the bytes returned are undefined. The actual content / format of
 * the bytes returned depends on how the context entry was used, and is
 * specified in the Crypto Controller specification [CCH]. However
 * N8_ReadContext simply treats them as  uninterpreted values, suitable for
 * reloading at a later time using N8_WriteContext. No checking or processing
 * of the bytes read from the context entry is done. It is important to note
 * that the context information is treated by the hardware as a sequence of
 * 32-bit values; the byte values read from a hardware context and returned
 * by this call are returned as a series of 32-bit quantities, and these
 * 32-bit values will be returned in either big endian or little endian format
 * depending on the byte order of the host processor.
 *
 * @param contextHandle     RO:     The handle of a previously allocated context
 *                                  entry as returned by N8_AllocateContext.
 
 * @param context_p         RO:     The bytes to be written to the context entry
 * @param contextLength     RO:     Length of Context in bytes, from 0 to 512
 *                                  bytes inclusive.  A  length of 0 is legal,
 *                                  but no bytes will actually be written
 * @param event_p           RW:     On input, if null the call is synchronous 
 *                                  and no event is returned. The operation is 
 *                                  complete when the call returns. If non-null, 
 *                                  then the call is asynchronous; an event is 
 *                                  returned that can be used to determine when 
 *                                  the operation completes
 *
 * @return 
 *    ret          - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Locks:
 *    This function acquires the context list semaphore from the queue structure.
 *
 * @par Errors:
 *    N8_NO_MORE_RESOURCE - There are no more context memory entries available
 *                          for allocation.  The call may succeed at a later
 *                          time if context entries are freed.
 *    N8_MALLOC_FAILED    - for some reasons malloc is failed
 *    N8_INVALID_INPUT_SIZE  - context length is more than 512
 *    N8_UNALIGNED_ADDRESS - passed address (context_p) is not 32-bit aligned 
 *   
 *****************************************************************************/
N8_Status_t N8_ReadContext(N8_ContextHandle_t contextHandle,
                           N8_Buffer_t       *context_p, 
                           uint32_t           contextLength, 
                           N8_Event_t        *event_p )

{

   N8_Status_t              ret = N8_STATUS_OK;      /* return status*/
   API_Request_t           *req_p = NULL;            /* request buffer */
   N8_Buffer_t             *contextMemory_p = NULL;
   uint32_t                 contextMemory_a;
   N8_Boolean_t             unitValid;

   DBG(("N8_ReadContext\n"));

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(context_p, ret);

      if ((int)context_p % 4)
      {
         ret = N8_UNALIGNED_ADDRESS;
         break;
      } 

      if (contextLength > EA_CTX_Record_Byte_Length)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }

      CHECK_STRUCTURE(contextHandle.structureID, 
                      N8_CONTEXT_STRUCT_ID, 
                      ret);

      unitValid = n8_validateUnit(contextHandle.unitID);
      if (!unitValid)
      {
         ret= N8_INVALID_UNIT;
         break;
      }

      ret = N8_ContextMemValidate(contextHandle.unitID, contextHandle.index);
      CHECK_RETURN(ret);                               

      /* allocate request buffer */
      ret = createEARequestBuffer(&req_p,
                                  contextHandle.unitID,
                                  N8_CB_EA_READCONTEXT_NUMCMDS,
                                  resultHandlerGeneric,
                                  CONTEXT_ENTRY_SIZE);
      CHECK_RETURN(ret);

      contextMemory_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      contextMemory_a =                 req_p->qr.physicalAddress + req_p->dataoffset;

      req_p->copyBackTo_p   = context_p;
      req_p->copyBackFrom_p = contextMemory_p;
      req_p->copyBackSize   = contextLength;

      /* create "write buffer to context memory" command */
      ret = cb_ea_readContext(req_p,
                              req_p->EA_CommandBlock_ptr,
                              contextHandle.index,
                              contextMemory_a,
                              contextLength); 
      CHECK_RETURN(ret);

      /* send command to write to specified context memory */
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

   } while(FALSE);

   DBG(("N8_ReadContext - FINISHED\n"));

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
   return ret;
} /* N8_ReadContext */


