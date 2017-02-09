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

static char const n8_id[] = "$Id: n8_dh.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_dh.c
 *  @brief Public DH functions.
 *
 * Implementation of all public DH functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 *
 * 05/16/03 brr   Fix Bug 902 by allocating a kernel buffer to hold key,
 *                modulus etc. to avoid copies in N8_DHCompute.
 * 05/30/03 brr   Set the structure ID in N8_DHInitializeKey when the key is set
 *                up so the error case frees memory correctly. (Bug 901)
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 04/09/02 bac   A DH key was trying to be freed even though it wasn't
 *                allocated.  BUG 686
 * 04/01/02 brr   Validate structureID in N8_DHFreeKey before deallocation.
 * 03/26/02 brr   Allocate the data buffer as part of the API request.
 * 02/28/02 brr   Do not include any QMgr include files.
 * 02/25/02 brr   Removed last references to validatedUnit.
 * 02/20/02 brr   Removed references to the queue structure.
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/08/01 mel   Added unit ID parameter to commend block calls.
 * 11/05/01 hml   Added the structure verification.
 * 10/02/01 bac   Added use of RESULT_HANDLER_WARNING in all result handlers.
 * 10/01/01 hml   Added multi-unit functionality.
 * 09/14/01 bac   Changes from code review.  Added precomputation of gRmodP and
 *                the use of it.
 * 09/13/01 mel   Added event parameter to N8_DHInitializeKey.
 * 08/29/01 bac   Added N8_DHFreeKey method.
 * 08/24/01 bac   Re-worked to correctly initialize the pre-computed values 
 *                and calculate the DHCompute correctly.  Per (BUG #125), 
 *                check valid modulus size at initialization.
 * 07/31/01 bac   Added call to N8_preamble for all public interfaces.
 * 07/23/01 bac   Created new result handler for DHInit and DHCompute.
 * 07/20/01 bac   Changed calls to create__RequestBuffer to pass the chip id.
 * 07/12/01 bac   Added copyright notice.
 * 07/12/01 bac   General cleanup.
 * 05/22/01 mel   Original version.
 ****************************************************************************/
/** @defgroup dh DH Functions.
 */

#include "n8_common.h"
#include "n8_pub_errors.h"
#include "n8_pk_common.h"
#include "n8_enqueue_common.h"
#include "n8_util.h"
#include "n8_API_Initialize.h"
#include "n8_cb_dh.h"


/**********************************************************************
 * N8_DHInitializeKey
 ***********************************************************************/
/** @ingroup dh
 * @brief Initializes the specified DHKeyObject so that it can be used
 * in Diffie-Hellman operations. The KeyMaterial object (structure)
 * contains the appropriate group generator g, modulus p,  and modulus size.
 *
 * Description:
 * This call pre-computes various constant values from the supplied
 * parameters and initializes DHKeyObject with these values.  By
 * pre-computing these values, Diffie-Hellman computations can be done
 * faster than if these constants must be computed on each operation. 
 * Once a DHKeyObject has been initialized, it can be used repeatedly
 * in multiple DH operations. It does not ever need to be re-initialized,
 * unless the actual key value(s) change (i.e., unless the key itself
 * changes).
 * 
 * @param key_p         RW: The caller allocated DHKeyObject, initialized
 *                          by this call with the appropriate DH key 
 *                          material and pre-computed DH constants 
 *                          depending on the value of the KeyType parameter
 * @param material_p    RO: Pointer to the key material to use in initializing
 *                          DHKeyObject. 
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   DH key or key material object is NULL<BR>
 *
 * @par Assumptions:
 *      None.
 **********************************************************************/
N8_Status_t N8_DHInitializeKey(N8_DH_KeyObject_t     *key_p,
                               N8_DH_KeyMaterial_t   *material_p,
                               N8_Event_t            *event_p)
{
   N8_Status_t     ret = N8_STATUS_OK;
   int             nBytes;
   API_Request_t  *req_p = NULL;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(material_p, ret);

      key_p->unitID = material_p->unitID;

      /* check the modulus size to ensure it is 1 <= ms <= 512.  return
       * N8_INVALID_SIZE if not. */
      if (material_p->modulusSize < N8_DH_MIN_MODULUS_SIZE ||
          material_p->modulusSize > N8_DH_MAX_MODULUS_SIZE)
      {
         ret = N8_INVALID_KEY_SIZE;
         break;
      }

      /* Allocate space for the initialized key.  we must compute 'R mod p' and 'cp'
       * for use in subsequent DHComputes. */
      key_p->modulusLength = material_p->modulusSize;
      nBytes = (NEXT_WORD_SIZE(key_p->modulusLength) +    /* g */
                NEXT_WORD_SIZE(key_p->modulusLength) +    /* p */
                NEXT_WORD_SIZE(key_p->modulusLength) +    /* R mod p */
                NEXT_WORD_SIZE(key_p->modulusLength) +    /* g R mod p */
                NEXT_WORD_SIZE(PK_DH_CP_Byte_Length));/* cp */

      /* Allocate a kernel buffer to hold data accessed by the NSP2000 */
      key_p->kmem_p = N8_KMALLOC_PK(nBytes);
      CHECK_OBJECT(key_p->kmem_p, ret);
      memset(key_p->kmem_p->VirtualAddress, 0, nBytes);

      /* Compute virtual addresses within the kernel buffer */
      key_p->g        = (N8_Buffer_t *) key_p->kmem_p->VirtualAddress;
      key_p->p        = key_p->g        + NEXT_WORD_SIZE(key_p->modulusLength);
      key_p->R_mod_p  = key_p->p        + NEXT_WORD_SIZE(key_p->modulusLength);
      key_p->gR_mod_p = key_p->R_mod_p  + NEXT_WORD_SIZE(key_p->modulusLength);
      key_p->cp       = key_p->gR_mod_p + NEXT_WORD_SIZE(key_p->modulusLength);

      /* Compute physical addresses within the kernel buffer */
      key_p->g_a      = key_p->kmem_p->PhysicalAddress;
      key_p->p_a      = key_p->g_a      + NEXT_WORD_SIZE(key_p->modulusLength);
      key_p->RmodP_a  = key_p->p_a      + NEXT_WORD_SIZE(key_p->modulusLength);
      key_p->gRmodP_a = key_p->RmodP_a  + NEXT_WORD_SIZE(key_p->modulusLength);
      key_p->cp_a     = key_p->gRmodP_a + NEXT_WORD_SIZE(key_p->modulusLength);

      /* Set up the memory for p and g and copy from the key material */
      memcpy(key_p->p, material_p->p, material_p->modulusSize);
      memcpy(key_p->g, material_p->g, material_p->modulusSize);

      /* Set the structure ID */
      key_p->structureID = N8_DH_STRUCT_ID;

      /* allocate user-space command buffer */
      ret = createPKRequestBuffer(&req_p, key_p->unitID,
                                  N8_CB_PRECOMPUTE_DHVALUES_NUMCMDS,
                                  NULL, 0);

      CHECK_RETURN(ret);

      ret = cb_precomputeDHValues(req_p, key_p->g_a, key_p->p_a, 
		                  key_p->RmodP_a, key_p->gRmodP_a, key_p->cp_a,
                                  key_p->modulusLength,
                                  req_p->PK_CommandBlock_ptr,
                                  key_p->unitID);
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
   } while(FALSE);

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      /* free the request */
      freeRequest(req_p);
      /* free the key also, if it has been allocated */
      if (key_p != NULL)
      {
         /* ignore the return code as we want to return the original anyway */
         (void) N8_DHFreeKey(key_p);
      }
   }
  
   return ret;
} /* N8_DHInitializeKey */

/**********************************************************************
 * N8_DHCompute
 ***********************************************************************/
/** @ingroup dh
 * @brief Computes a Diffie-Hellman exponentiation as required to compute
 * Diffie-Hellman public values from a secret value and a Diffie-Hellman
 * key (or group).  
 *
 * Description:
 * The key or group is specified by DHKeyObject which must have been
 * previously initialized with the appropriate group generator g, modulus p,
 * and modulus size by a call to N8_DHInitializeKey. The DH secret value
 * used as the exponent is specified in XValue, and must be of the same
 * size as the modulus size in DHKeyObject. GValue specifies the value
 * to be exponentiated and must also be the same size as the modulus. 
 * GValue may be null, in which case the generator g from DHKeyObject is
 * used as the GValue. The result of the calculation is returned in
 * GXValue and is the same size as the modulus.  The value computed is
 * GXValue = ( GValue ** XValue ) mod p. Thus this routine computes a
 * modular exponentiation.
 * This call can be used to compute from a private x value the value
 * X = g**x mod p that is the public X value sent to the other respondent
 * in a Diffie-Hellman exchange.  (In this case the GValue is set to null,
 * and x is used for XValue.)  When the respondent's corresponding Y value
 * (Y = g**y mod p, y = the respondent's private y) is received, this call
 * can then be used to compute the common shared secret XY = g**(xy) = YX by
 * using X for the GValue and the received Y as the XValue. 
 * 
 * Parameters:
 * @param key_p         RO:     The caller supplied DHKeyObject containing
 *                              the appropriate Diffie-Hellman values
 *                              and pre-computed DH constants.
 * @param gValue        RO:     The value to be raised to a power. 
 *                              If null, then the generator g value from
 *                              DHKeyObject is used. GValue must be the
 *                              same size as the modulus p from DHKeyObject.
 * @param xValue        RO:     The exponent. Must be supplied. XValue
 *                              must be the same size as the
 *                              modulus p from DHKeyObject
 * @param gxValue       WO:     GValue raised to the XValue power, mod p.  
 *                              GXValue will be the same
 *                              size as the modulus p from DHKeyObject.
 * @param event_p       RW:     On input, if null the call is synchronous 
 *                              and no event is returned. The operation 
 *                              is complete when the call returns. If 
 *                              non-null, then the call is asynchronous; 
 *                              an event is returned that can be used to 
 *                              determine when the operation completes.
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
 *          N8_INVALID_KEY      -   The DHKeyObject is not a valid key
 *                                  object 
 *                                  for this operation.
 *
 * @par Assumptions:
 **********************************************************************/
N8_Status_t N8_DHCompute(N8_DH_KeyObject_t  *key_p,
                         N8_Buffer_t        *gValue_p,
                         N8_Buffer_t        *xValue_p,
                         N8_Buffer_t        *gxValue_p,
                         N8_Event_t         *event_p)
{
   N8_Status_t    ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;
   unsigned int   nBytes;

   N8_Buffer_t    *g_p;
   N8_Buffer_t    *x_p;
   N8_Buffer_t    *res_p;

   uint32_t        g_a;
   uint32_t        x_a;
   uint32_t        res_a;

   unsigned int    g_len;
   unsigned int    x_len;
   unsigned int    res_len;
   
   N8_Boolean_t       useShortCommandBlock = N8_FALSE;
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      
      CHECK_OBJECT(key_p, ret);
      CHECK_OBJECT(xValue_p, ret);
      CHECK_OBJECT(gxValue_p, ret);
      CHECK_STRUCTURE(key_p->structureID, N8_DH_STRUCT_ID, ret);

      /* compute the lengths of all of the parameters as a convenience */
      g_len      = key_p->modulusLength;
      x_len      = key_p->modulusLength;
      res_len    = key_p->modulusLength;

      nBytes = (NEXT_WORD_SIZE(x_len) +
                NEXT_WORD_SIZE(res_len));

      /* gValue_p is allowed to be NULL.  if so, use the g from the key object. */
      if (gValue_p == NULL)
      {
         useShortCommandBlock = N8_TRUE;

         /* allocate user-space buffer */
         ret = createPKRequestBuffer(&req_p,
                                     key_p->unitID,
                                     N8_CB_COMPUTE_G_XMODP_NUMCMDS_SHORT,
                                     resultHandlerGeneric, nBytes); 
      }
      else
      {
         nBytes += NEXT_WORD_SIZE(g_len);

         /* allocate user-space buffer */
         ret = createPKRequestBuffer(&req_p,
                                     key_p->unitID,
                                     N8_CB_COMPUTE_G_XMODP_NUMCMDS_LONG,
                                     resultHandlerGeneric, nBytes); 
      }

      CHECK_RETURN(ret);

      /* set up the addressing for the pointers */
      x_p =  (N8_Buffer_t *) ((int)req_p + req_p->dataoffset);
      x_a =                  req_p->qr.physicalAddress + req_p->dataoffset;
      res_p    = x_p + NEXT_WORD_SIZE(x_len);
      res_a    = x_a + NEXT_WORD_SIZE(x_len);

      memcpy(x_p, xValue_p, x_len);
      
      req_p->copyBackSize = x_len;
      req_p->copyBackFrom_p = res_p;
      req_p->copyBackTo_p = gxValue_p;

      if (useShortCommandBlock == N8_TRUE)
      {
         ret = cb_computeGXmodp_short(req_p,
                                      x_a,
                                      key_p->p_a,
                                      key_p->cp_a,
                                      key_p->gRmodP_a,
                                      res_a,
                                      key_p->modulusLength,
                                      req_p->PK_CommandBlock_ptr);
      }
      else
      {
         /* gValue_p is not NULL, copy the user-space */
         /* data into our kernel space buffers        */
         g_p    = res_p + NEXT_WORD_SIZE(res_len);
         g_a    = res_a + NEXT_WORD_SIZE(res_len);
         memcpy(g_p, gValue_p, g_len);

         ret = cb_computeGXmodp_long(req_p,
                                     g_a,
                                     x_a,
                                     key_p->p_a,
                                     key_p->cp_a,
                                     key_p->RmodP_a,
                                     res_a,
                                     key_p->modulusLength,
                                     req_p->PK_CommandBlock_ptr);
      }

      CHECK_RETURN(ret);

      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);

   } while (FALSE);
   DBG(("DH computed\n"));

   /*
    * Deallocate the request if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
  
   return ret;
} /* N8_DHCompute */


/*****************************************************************************
 * N8_DHFreeKey
 *****************************************************************************/
/** @ingroup dh
 * @brief Free an initialized Diffie-Hellman key object
 *
 * At initialization, the key object has several components allocated.  This
 * method will release those resources.  This method should be called when the
 * key is no longer used.  It is the user's responsibility to avoid memory leaks
 * by invoking this method.
 *
 *  @param key_p               RW:  Pointer to key object to be freed.
 *
 * @par Externals
 *      None
 *
 * @return
 *    Status:  N8_STATUS_OK -- no errors<br>
 *             N8_INVALID_KEY -- the key passed was not a valid key.
 *
 * @par Errors
 *    No errors are reported except as noted above.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_DHFreeKey(N8_DH_KeyObject_t *key_p)
{
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      
      if ((key_p == NULL) || (key_p->structureID != N8_DH_STRUCT_ID))
      {
         ret = N8_INVALID_KEY;
         break;
      }
      if (key_p->kmem_p != NULL)
      {
         N8_KFREE(key_p->kmem_p);
         key_p->kmem_p = NULL;
      }

      key_p->structureID = 0;
   } while (FALSE);
   return ret;
} /* N8_DHFreeKey */
