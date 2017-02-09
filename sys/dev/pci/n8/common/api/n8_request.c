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

static char const n8_id[] = "$Id: n8_request.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_request.c
 *  @briefs file contains the N8_RequestAllocate and N8_RequestFree API calls. 
 *
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 04/24/03 jpw   Change N8_KMALLOC_PK in N8_RequestAllocate to N8_KMALLOC
 *		  so we always use the EA segment for N8_RequestAlllocate
 * 07/14/02 bac   Fixed a bug where pointer arithmetic was incorrect when
 *                calculating *bufferVirt_pp.
 * 06/14/02 hml   Removed N8_PacketRequestSet.
 * 06/10/02 hml   First completed version.
 * 06/04/02 hml   Original version.
 ****************************************************************************/
/** @defgroup Request
 */
#include "n8_util.h"
#include "n8_enqueue_common.h"
#include "n8_pub_errors.h"
#include "n8_pub_common.h"
#include "n8_pub_request.h"
#include "n8_API_Initialize.h"

/*****************************************************************************
 * N8_RequestAllocate
 *****************************************************************************/
/** @ingroup Request
 * @brief Allocates a request buffer of contiguous kernel memory of the requested 
 *        data size from the request pool.
 *
 *  @param dataBytes     RO: Number of bytes in the data area.
 *  @param request_p     WO: Pointer to return handle.
 *  @param bufferVirt_pp WO: Pointer in which to return the virtual address of
 *                           the data area of the request.
 *
 * @par Externals:
 *    requestPoolCtl_gp   The bank control for the default pool.
 *
 * @return
 *    returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *      N8_INVALID_OBJECT  One of the parameters is invalid.
 *      N8_NO_MORE_RESOURCE The resource is exhausted.
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_RequestAllocate(const unsigned int   nDataBytes,
                               N8_RequestHandle_t  *request_p, 
                               N8_Buffer_t        **bufferVirt_pp)
{
   N8_Status_t         ret = N8_STATUS_OK;
   N8_MemoryHandle_t  *kmem_p = NULL;
   int                 allocBytes;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* Parameter checking */
      CHECK_OBJECT(request_p, ret);
      CHECK_OBJECT(bufferVirt_pp, ret);

      if (nDataBytes == 0)
      {
         /* This is ok, but there is nothing much to do */
         *bufferVirt_pp = NULL;
         break;
      }

      if (nDataBytes > N8_MAX_REQUEST_DATA_BYTES)
      {
         ret = N8_INVALID_VALUE;
         break;
      }
      /* We are allocating memory for the complete request. The memory struct
         be added on by the KMALLOC, but we need to add the memory for the API
         request and the command blocks and the context. */
      allocBytes = nDataBytes + 
         sizeof(API_Request_t) + N8_COMMAND_BLOCK_BYTES + N8_CTX_BYTES;

      /* Allocate the buffer */
      kmem_p = N8_KMALLOC(allocBytes); 
      CHECK_OBJECT(kmem_p, ret);

      /* Return the data to the user */
      *request_p = (N8_RequestHandle_t) kmem_p;
      *bufferVirt_pp = (N8_Buffer_t *)((int)kmem_p->VirtualAddress + 
                                       sizeof(API_Request_t) + 
                                       N8_COMMAND_BLOCK_BYTES +
                                       N8_CTX_BYTES);
   } while (FALSE);

   return ret;
} /* N8_RequestAllocate */

/*****************************************************************************
 * N8_RequestFree
 *****************************************************************************/
/** @ingroup Request
 * @brief Frees a previously allocated request of contiguous kernel memory.
 *
 *  @param requestHandle     RO: Handle of request to free.
 *
 * @return
 *    returns N8_STATUS_OK if successful or Error value.
 * @par Errors
 *      N8_INVALID_OBJECT  One of the parameters is invalid.
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_RequestFree(N8_RequestHandle_t requestHandle)
{
   N8_Status_t         ret = N8_STATUS_OK;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* Parameter checking */
      CHECK_OBJECT(requestHandle, ret);

      /* Free the buffer */
      N8_KFREE((N8_MemoryHandle_t *) requestHandle);
   } while (FALSE);

   return ret;
} /* N8_RequestFree */

