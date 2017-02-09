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

static char const n8_id[] = "$Id: n8_buffer.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_buffer.c
 *  @briefs file contains the N8_BufferAllocate and N8_BufferFree API calls. 
 *
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 03/29/02 hml   Use new style KMALLOC and deleted a user space include file.
 * 02/12/02 hml   First functioning version.  Added calls to N8_preamble.
 * 02/06/02 hml   Original version.
 ****************************************************************************/
/** @defgroup Buffer
 */

#include "n8_malloc_common.h"
#include "n8_util.h"
#include "n8_pub_buffer.h"
#include "n8_pub_errors.h"
#include "n8_pub_common.h"
#include "n8_API_Initialize.h"

/*****************************************************************************
 * N8_BufferAllocate
 *****************************************************************************/
/** @ingroup Buffer
 * @brief Allocates a buffer of contiguous kernel memory of the requested 
 *        size.
 *
 *  @param bufferHandle_p     WO: Pointer to return area for handle.
 *  @param virtualAddress_pp  WO: Pointer to return area for virtual address.
 *  @param nBytes             RO: Number of bytes in desired buffer.
 *
 * @return
 *    returns N8_STATUS_OK if successful or Error value.
 * @par Errors
 *      N8_INVALID_OBJECT  One of the parameters is invalid.
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_BufferAllocate(N8_BufferHandle_t *bufferHandle_p, 
                              N8_Buffer_t **virtualAddress_pp,
                              const unsigned int nBytes)
{
   N8_Status_t         ret = N8_STATUS_OK;
   N8_MemoryHandle_t  *kmem_p = NULL;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* Parameter checking */
      CHECK_OBJECT(bufferHandle_p, ret);
      CHECK_OBJECT(virtualAddress_pp, ret);

      if (nBytes == 0)
      {
         /* This is ok, but there is nothing to do */
         break;
      }

      /* Allocate the buffer */
      kmem_p = N8_KMALLOC(nBytes); 
      CHECK_OBJECT(kmem_p, ret);

      /* Return the data to the user */
      *bufferHandle_p = (N8_BufferHandle_t) kmem_p;
      *virtualAddress_pp = (N8_Buffer_t *) kmem_p->VirtualAddress;
   } while (FALSE);

   return ret;
}

/*****************************************************************************
 * N8_BufferFree
 *****************************************************************************/
/** @ingroup Buffer
 * @brief Frees a previously allocated buffer of contiguous kernel memory.
 *
 *  @param bufferHandle     RO: Handle of buffer to free.
 *
 * @return
 *    returns N8_STATUS_OK if successful or Error value.
 * @par Errors
 *      N8_INVALID_OBJECT  One of the parameters is invalid.
 * @par Assumptions
 *    None<br>
 *****************************************************************************/
N8_Status_t N8_BufferFree(N8_BufferHandle_t bufferHandle)
{
   N8_Status_t         ret = N8_STATUS_OK;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      /* Parameter checking */
      CHECK_OBJECT(bufferHandle, ret);

      /* Free the buffer */
      N8_KFREE((N8_MemoryHandle_t *) bufferHandle);
   } while (FALSE);

   return ret;
}

