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

static char const n8_id[] = "$Id: n8_user_util.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_user_util.c
 *  @brief Utility functions that are only appropriate for user space.
 *
 * Some user-space utilities either don't make sense for inclusion in the kernel
 * builds or rely on functions that are not available in the kernel.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 06/05/03 brr   Added functions N8_DeleteSizedBuffer & N8_DeleteBuffer.
 * 05/09/02 bac   Original version.
 ****************************************************************************/
/** @defgroup user_util User-space only utilities
 */

#include <stdio.h>
#include "n8_pub_buffer.h"
#include "n8_util.h"

/*****************************************************************************
 * N8_CreateSizedBufferFromATTR
 *****************************************************************************/
/** @ingroup user_util
 * @brief Create a sized buffer given data in an ATTR structure
 *
 *  @param buf_p               RW:  pointer to the buffer to be created
 *  @param attr_p              RO:  pointer to the ATTR data
 *
 * @par Externals
 *    None
 *
 * @return
 *    status
 *
 * @par Errors
 *    N8_STATUS_OK, N8_MALLOC_FAILED
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_CreateSizedBufferFromATTR(N8_SizedBuffer_t *buf_p,
                                         const ATTR *attr_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   unsigned int len;
   unsigned int i;
   do
   {
      len =
         PKDIGITS_TO_BYTES(attr_p->length * SIMON_MULTIPLIER);
      /* the data in an ATTR structure may be zero-padded to the left.  these
       * need to be stripped.  find the first non-zero byte and start copying
       * from there.
       */
      i = 0;
      while (i < len && attr_p->value[i] == 0x0)
      {
         i++;
      }
      buf_p->lengthBytes = len - i;
      CREATE_UOBJECT_SIZE(buf_p->value_p,
                          buf_p->lengthBytes,
                          ret);
      if (buf_p->lengthBytes == 0)
      {
         ret = N8_INVALID_VALUE;
         break;
      }
      memcpy(buf_p->value_p, &attr_p->value[i], buf_p->lengthBytes);
   } while (FALSE);
   return ret;
} /* N8_CreateSizedBufferFromATTR */

/*****************************************************************************
 * N8_CreateSizedBufferFromString
 *****************************************************************************/
/** @ingroup user_util
 * @brief Create a sized buffer given a string of hex digits
 *
 *  @param str_p               RO:  input string
 *  @param buf_p               RW:  output buffer
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_STATUS_OK, N8_MALLOC_FAILED
 *
 * @par Assumptions
 *    Input string only contains hex characters ([0-9][a-f][A-F])
 *****************************************************************************/
N8_Status_t N8_CreateSizedBufferFromString(const char *str_p,
                                           N8_SizedBuffer_t *buf_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   if (buf_p != NULL)
   {
      buf_p->lengthBytes = (strlen(str_p)+1) / 2;
      buf_p->value_p = N8_CreateBuffer(str_p);
      if (buf_p->value_p == NULL)
      {
         ret = N8_MALLOC_FAILED;
      }
   }
   else
   {
      ret = N8_INVALID_OBJECT;
   }
   return ret;
} /* N8_CreateSizedBufferFromString */

/*****************************************************************************
 * N8_DeleteSizedBuffer
 *****************************************************************************/
/** @ingroup user_util
 * @brief Delete a buffer allocated by N8_CreateSizeBuffer....
 *
 *  @param buf_p               RO:  buffer
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_STATUS_OK, N8_INVALID_OBJECT
 *
 * @par Assumptions
 *****************************************************************************/
N8_Status_t N8_DeleteSizedBuffer(N8_SizedBuffer_t *buf_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   if (buf_p != NULL)
   {
      N8_UFREE(buf_p->value_p);
   }
   else
   {
      ret = N8_INVALID_OBJECT;
   }
   return ret;
} /* N8_DeleteSizedBuffer */

/*****************************************************************************
 * N8_CreateBuffer
 *****************************************************************************/
/** @ingroup user_util
 * @brief Allocate and return a N8_Buffer_t of hex values given an input string
 * representing hex digits.
 *
 *  @param e                   RO:  input string
 *
 * @par Externals
 *    None
 *
 * @return
 *    Pointer to created buffer.  NULL if the malloc failed.
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    Input string only contains hex characters ([0-9][a-f][A-F])
 *****************************************************************************/
N8_Buffer_t *N8_CreateBuffer(const char *e)
{
   int i,j;
   unsigned int temp;
   int len;
   int size;
   N8_Buffer_t *expected;
   len = strlen(e);
   size = (len+1) / 2;
   expected = (N8_Buffer_t *) N8_UMALLOC(size);

   if (expected == NULL)
      return NULL;

   j = 0;
   for (i = 0; i < len; i+=2)
   {
      sscanf(&e[i], "%2x", &temp);
      expected[j++] = (unsigned char) (temp & 0xff);
   }
   return expected;
} /* N8_CreateBuffer */

/*****************************************************************************
 * N8_DeleteBuffer
 *****************************************************************************/
/** @ingroup user_util
 * @brief Delete a buffer allocated by N8_CreateBuffer
 *
 *  @param buf_p               RO:  buffer
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_STATUS_OK, N8_INVALID_OBJECT
 *
 * @par Assumptions
 *****************************************************************************/
N8_Status_t N8_DeleteBuffer(N8_Buffer_t *buf_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   if (buf_p != NULL)
   {
      N8_UFREE(buf_p);
   }
   else
   {
      ret = N8_INVALID_OBJECT;
   }
   return ret;
} /* N8_DeleteBuffer */

