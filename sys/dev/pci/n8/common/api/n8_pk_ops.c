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

static char const n8_id[] = "$Id: n8_pk_ops.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_pk_ops.c
 *  @brief Implementation of PKP Base Operations
 *
 * The set of routines implemented here allow direct user access to PKP
 * operations on the chip.  They include:
 * N8_ModAdd
 * N8_ModSubtract
 * N8_ModMultiply
 * N8_Modulus
 * N8_ModAdditiveInverse
 * N8_ModMultiplicativeInverse
 * N8_ModR
 * N8_ModExponentiation
 *****************************************************************************/
/*****************************************************************************
 * Revision history:
 * 06/11/02 bac   Changed return codes to be N8_INVALID_VALUE when (a >= b)
 *                instead of N8_INVALID_INPUT_SIZE.  (Bug #794)
 * 05/20/02 brr   Free the request for all error conditions.
 * 05/07/02 msz   New interface for QUEUE_AND_CHECK for new synchronous support.
 * 05/07/02 bac   Original version.
 ****************************************************************************/
/** @defgroup pkops PKP Base Operations
 */


#include "n8_pub_pk.h"
#include "n8_cb_pk_ops.h"
#include "n8_cb_rsa.h"
#include "n8_enqueue_common.h"
#include "n8_API_Initialize.h"
#include "n8_util.h"

/* Static methods */

/*****************************************************************************
 * stripLeadingZeros
 *****************************************************************************/
/** @ingroup pkops
 * @brief Given a N8_SizedBuffer_t whose value may or may not have leading
 * zeros, return one guaranteed not to have leading zeros.
 *
 * <More detailed description of the function including any unusual algorithms
 * or suprising details.>
 *
 *  @param in_p                RO:  Pointer to original buffer
 *  @param out_p               RW:  Pointer to output buffer
 *
 * @par Externals
 *    None
 *
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    in_p and out_p are non-null
 *****************************************************************************/
static void stripLeadingZeros(const N8_SizedBuffer_t *in_p,
                              N8_SizedBuffer_t *out_p)
{
   int i;
   out_p->lengthBytes = in_p->lengthBytes;
   out_p->value_p = in_p->value_p;

   i = 0;
   while ((in_p->value_p[i] == 0x0) && (i < in_p->lengthBytes))
   {
      i++;
   }
   if (i >= in_p->lengthBytes)
   {
      /* all of the input was zero.  return a single byte of zero. */
      out_p->lengthBytes = 1;
      out_p->value_p = in_p->value_p;
   }
   else
   {
      out_p->lengthBytes = in_p->lengthBytes - i;
      out_p->value_p = &(in_p->value_p[i]);
   }
} /* stripLeadingZeros */

/*****************************************************************************
 * verifyParameter
 *****************************************************************************/
/** @ingroup pkops
 * @brief Preliminary checking of parameter
 *
 * Ensures parameter is non-null and the length is within bounds.
 *
 *  @param op_p                RO:  pointer to operand
 *  @param minLength           RO:  minimum length
 *  @param maxLength           RO:  maximum length
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_INVALID_OBJECT if operand is null<br>
 *    N8_INVALID_INPUT_SIZE if length is out of bounds
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t verifyParameter(const N8_SizedBuffer_t *op_p,
                                   unsigned int minLength,
                                   unsigned int maxLength)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      if (op_p == NULL || op_p->value_p == NULL)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      if (op_p->lengthBytes < minLength)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      if (op_p->lengthBytes > maxLength)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      if (op_p->lengthBytes > N8_PK_MAX_SIZE)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
   } while (N8_FALSE);

   return ret;
} /* verifyParameter */

/*****************************************************************************
 * verify_x_lt_y
 *****************************************************************************/
/** @ingroup pkops
 * @brief Verify that the value of x is less than that of y.
 *
 * Test to ensure the value of operand x is less than operand y.
 *
 *  @param x_p                 RO:  pointer to operand x
 *  @param y_p                 RO:  pointer to operand y
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status of comparison
 *
 * @par Errors
 *    N8_STATUS_OK if the relation x < y is true
 *    N8_INVALID_OBJECT if x or y are null
 *    N8_INVALID_INPUT_SIZE if not (x < y)
 *
 * @par Assumptions
 *    <description of assumptions><br>
 *****************************************************************************/
static N8_Status_t verify_x_lt_y(const N8_SizedBuffer_t *x_p,
                                 const N8_SizedBuffer_t *y_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   N8_SizedBuffer_t stripped_x;
   N8_SizedBuffer_t stripped_y;
   
   do
   {
      if (x_p == NULL || x_p->value_p == NULL)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      if (y_p == NULL || y_p->value_p == NULL)
      {
         ret = N8_INVALID_OBJECT;
         break;
      }
      if (x_p->lengthBytes == 0)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      /* Either x or y could have leading zeros.  In order to correctly compare
       * them, we must strip off the leading zeros before proceeding. */
      stripLeadingZeros(x_p, &stripped_x);
      stripLeadingZeros(y_p, &stripped_y);

      if (stripped_x.lengthBytes > N8_PK_MAX_SIZE)
      {
         ret = N8_INVALID_INPUT_SIZE;
         break;
      }
      if (stripped_x.lengthBytes > stripped_y.lengthBytes)
      {
         ret = N8_INVALID_VALUE;
         break;
      }
      /* if the lengths x and y are equal, ensure x < y */
      if (stripped_x.lengthBytes == stripped_y.lengthBytes)
      {
         int cmp = memcmp(stripped_x.value_p, stripped_y.value_p,
                          stripped_x.lengthBytes);

         if (cmp < 0)
         {
            ret = N8_STATUS_OK; /* x < y */
         }
         else
         {
            ret = N8_INVALID_VALUE; /* x >= y */
         }
      }
   } while (N8_FALSE);

   return ret;
} /* verify_x_lt_y */

/*****************************************************************************
 * n8_pk_gen_command
 *****************************************************************************/
/** @ingroup pkops
 * @brief Perform modular arithmetic.  The actual operation is specified by the
 * shifted_opcode argument. 
 *
 * This command generator may be used for operations requiring zero, one, or two
 * operands, e.g.
 * result = R mod m (zero operands)
 * result = a mod m (one operand)
 * result = (a <operation> b) mod m (two operands)
 *
 *  @param a_p                 RO:  a operand
 *  @param b_p                 RO:  b operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param shifted_opcode      RO:  opcode for the operation to be used.  The
 *                                  opcode is to be shifted left appropriate for
 *                                  insertion into command block as is.  This
 *                                  allows the use of the #defines already used
 *                                  elsewhere. 
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static N8_Status_t n8_pk_gen_command(const N8_SizedBuffer_t *a_p,
                                     const N8_SizedBuffer_t *b_p,
                                     const N8_SizedBuffer_t *modulus_p,
                                     N8_SizedBuffer_t *result_p,
                                     const N8_Unit_t unitID,
                                     const uint32_t shifted_opcode,
                                     N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;

   N8_Buffer_t   *k_a_op_p = NULL;
   N8_Buffer_t   *k_b_op_p = NULL;
   N8_Buffer_t   *k_modulus_p = NULL;
   N8_Buffer_t   *k_result_p = NULL;

   uint32_t       k_a_op_a;
   uint32_t       k_b_op_a;
   uint32_t       k_modulus_a;
   uint32_t       k_result_a;

   unsigned int   nBytes;
   unsigned int   numCommands;
   unsigned int   a_length_bytes;
   unsigned int   b_length_bytes;
   unsigned int   max_length_bytes;
   
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);
      /* ensure the modulus_p is not null before accessing it. */
      CHECK_OBJECT(modulus_p, ret);
      ret = verifyParameter(modulus_p, 1, modulus_p->lengthBytes);
      CHECK_RETURN(ret);
      ret = verifyParameter(result_p, modulus_p->lengthBytes, N8_PK_MAX_SIZE);
      CHECK_RETURN(ret);

      /* set the length of the result to the length of the modulus */
      result_p->lengthBytes = modulus_p->lengthBytes;
      if (a_p != NULL)
      {
         max_length_bytes = N8_MAX(modulus_p->lengthBytes, a_p->lengthBytes);
      }
      else
      {
         max_length_bytes = modulus_p->lengthBytes;
      }
      
      nBytes = NEXT_WORD_SIZE(max_length_bytes) * 4;

      numCommands = N8_CB_PK_OP_NUMCMDS;
      if (b_p == NULL)
      {
         numCommands--;
      }
      if (a_p == NULL)
      {
         numCommands--;
      }
      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  unitID, 
                                  numCommands,
                                  resultHandlerGeneric,
                                  nBytes);
      CHECK_RETURN(ret);
      /* set the components of the kernel memory allocated:
       * k_a_op_p
       * k_b_op_p
       * k_modulus_p
       * k_result_p
       */
      k_a_op_p = (N8_Buffer_t *) ((int) req_p + req_p->dataoffset);
      k_a_op_a = req_p->qr.physicalAddress + req_p->dataoffset;

      k_b_op_p = k_a_op_p + NEXT_WORD_SIZE(max_length_bytes);
      k_b_op_a = k_a_op_a + NEXT_WORD_SIZE(max_length_bytes);

      k_modulus_p = k_b_op_p + NEXT_WORD_SIZE(max_length_bytes);
      k_modulus_a = k_b_op_a + NEXT_WORD_SIZE(max_length_bytes);

      k_result_p = k_modulus_p + NEXT_WORD_SIZE(max_length_bytes);
      k_result_a = k_modulus_a + NEXT_WORD_SIZE(max_length_bytes);
      
      req_p->copyBackTo_p = result_p->value_p;
      req_p->copyBackFrom_p = k_result_p;
      req_p->copyBackSize = modulus_p->lengthBytes;

      /* copy the data into the kernel buffers */
      memcpy(k_modulus_p, modulus_p->value_p, modulus_p->lengthBytes);
      if (a_p != NULL)
      {
         memcpy(k_a_op_p, a_p->value_p, a_p->lengthBytes);
         a_length_bytes = a_p->lengthBytes;
      }
      else
      {
         k_a_op_a = 0x0;
         a_length_bytes = 0;
      }
      if (b_p != NULL)
      {
         memcpy(k_b_op_p, b_p->value_p, b_p->lengthBytes);
         b_length_bytes = b_p->lengthBytes;
      }
      else
      {
         k_b_op_a = 0x0;
         b_length_bytes = 0;
      }

      ret = cb_pk_op(req_p,                      /* request pointer */
                     shifted_opcode,             /* opcode, pre-shifted */
                     k_a_op_a,                   /* physical address of A */
                     a_length_bytes,             /* A length */
                     k_b_op_a,                   /* physical address of B */
                     b_length_bytes,             /* B length */
                     k_modulus_a,                /* physical address of modulus */
                     modulus_p->lengthBytes,     /* modulus length */
                     max_length_bytes,           /* max operand or modulus length */
                     k_result_a,                 /* physical address of results */
                     req_p->PK_CommandBlock_ptr, /* pointer to command block area */
                     NULL);                      /* return pointer of next
                                                  * command block area, or NULL */
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
      
   } while (N8_FALSE);
   
   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
  
   return ret;
   
} /* n8_pk_gen_command */ 


/* Public methods */

/*****************************************************************************
 * Two operand operations 
 *****************************************************************************/

/*****************************************************************************
 * N8_ModAdd
 *****************************************************************************/
/** @ingroup pkops
 * @brief Perform modular addition.
 *
 * result = (a + b) mod m
 *
 *  @param a_p                 RO:  a operand
 *  @param b_p                 RO:  b operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_ModAdd(const N8_SizedBuffer_t *a_p,
                      const N8_SizedBuffer_t *b_p,
                      const N8_SizedBuffer_t *modulus_p,
                      N8_SizedBuffer_t *result_p,
                      const N8_Unit_t unitID,
                      N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      ret = verify_x_lt_y(a_p, modulus_p);
      CHECK_RETURN(ret);
      ret = verify_x_lt_y(b_p, modulus_p);
      CHECK_RETURN(ret);
      ret = n8_pk_gen_command(a_p, b_p, modulus_p, result_p, unitID,
                              PK_Cmd_A_Plus_B_Mod_M,
                              event_p);
   } while (N8_FALSE);
   return ret;
} /* N8_ModAdd */

/*****************************************************************************
 * N8_ModSubtract
 *****************************************************************************/
/** @ingroup pkops
 * @brief Perform modular subtraction.
 *
 * result = (a - b) mod m
 *
 *  @param a_p                 RO:  a operand
 *  @param b_p                 RO:  b operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_ModSubtract(const N8_SizedBuffer_t *a_p,
                           const N8_SizedBuffer_t *b_p,
                           const N8_SizedBuffer_t *modulus_p,
                           N8_SizedBuffer_t *result_p,
                           const N8_Unit_t unitID,
                           N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      ret = verify_x_lt_y(a_p, modulus_p);
      CHECK_RETURN(ret);
      ret = verify_x_lt_y(b_p, modulus_p);
      CHECK_RETURN(ret);
      ret = n8_pk_gen_command(a_p, b_p, modulus_p, result_p, unitID,
                              PK_Cmd_A_Minus_B_Mod_M,
                              event_p);
   } while (N8_FALSE);
   return ret;
} /* N8_ModSubtract */


/*****************************************************************************
 * N8_ModMultiply
 *****************************************************************************/
/** @ingroup pkops
 * @brief Perform modular multiplication.
 *
 * result = (a * b) mod m
 *
 *  @param a_p                 RO:  a operand
 *  @param b_p                 RO:  b operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_ModMultiply(const N8_SizedBuffer_t *a_p,
                           const N8_SizedBuffer_t *b_p,
                           const N8_SizedBuffer_t *modulus_p,
                           N8_SizedBuffer_t *result_p,
                           const N8_Unit_t unitID,
                           N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      ret = verify_x_lt_y(a_p, modulus_p);
      CHECK_RETURN(ret);
      ret = verify_x_lt_y(b_p, modulus_p);
      CHECK_RETURN(ret);
      ret = n8_pk_gen_command(a_p, b_p, modulus_p, result_p, unitID,
                              PK_Cmd_AB_Mod_M,
                              event_p);
   } while (N8_FALSE);
   return ret;
   
} /* N8_ModMultiply */

/*****************************************************************************
 * N8_Modulus
 *****************************************************************************/
/** @ingroup pkops
 * @brief Calculate a mod m
 *
 * result = a mod m
 *
 *  @param a_p                 RO:  a operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_Modulus(const N8_SizedBuffer_t *a_p,
                       const N8_SizedBuffer_t *modulus_p,
                       N8_SizedBuffer_t *result_p,
                       const N8_Unit_t unitID,
                       N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      ret = verifyParameter(a_p, 1, N8_PK_MAX_SIZE);
      CHECK_RETURN(ret);
      ret = n8_pk_gen_command(a_p, NULL, modulus_p, result_p, unitID,
                              PK_Cmd_A_Mod_M,
                              event_p);
   } while (N8_FALSE);
   return ret;
} /* N8_Modulus */ 

/*****************************************************************************
 * N8_ModAdditiveInverse
 *****************************************************************************/
/** @ingroup pkops
 * @brief Calculate the additive inverse of a mod m.
 *
 * result = -a mod m
 *
 *  @param a_p                 RO:  a operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_ModAdditiveInverse(const N8_SizedBuffer_t *a_p,
                                  const N8_SizedBuffer_t *modulus_p,
                                  N8_SizedBuffer_t *result_p,
                                  const N8_Unit_t unitID,
                                  N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      ret = verify_x_lt_y(a_p, modulus_p);
      CHECK_RETURN(ret);
      ret = n8_pk_gen_command(a_p, NULL, modulus_p, result_p, unitID,
                              PK_Cmd_Minus_A_Mod_M,
                              event_p);
   } while (N8_FALSE);
   return ret;
   
} /* N8_ModAdditiveInverse */ 

/*****************************************************************************
 * N8_ModMultiplicativeInverse
 *****************************************************************************/
/** @ingroup pkops
 * @brief Calculate the multiplicative inverse of a mod m.
 *
 * result = a**-1 mod m
 *
 *  @param a_p                 RO:  a operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    No test is performed to ensure a and the modulus are relatively prime.
 * The request will be submitted to the hardware.  If they are not relatively
 * prime, a N8_HARDWARE_ERROR will be generated and returned.
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_ModMultiplicativeInverse(const N8_SizedBuffer_t *a_p,
                                        const N8_SizedBuffer_t *modulus_p,
                                        N8_SizedBuffer_t *result_p,
                                        const N8_Unit_t unitID,
                                        N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      ret = verify_x_lt_y(a_p, modulus_p);
      CHECK_RETURN(ret);
      ret = n8_pk_gen_command(a_p, NULL, modulus_p, result_p, unitID,
                              PK_Cmd_Inverse_A_Mod_M,
                              event_p);
   } while (N8_FALSE);
   return ret;
   
} /* N8_ModMultiplicativeInverse */ 

/*****************************************************************************
 * N8_ModR
 *****************************************************************************/
/** @ingroup pkops
 * @brief Perform R mod m
 *
 * Calculate the value R mod m where R is 2 ** (128 * digit_length(m)).
 * The value of R is calculated by the hardware
 *
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  unit (chip) specifier
 *  @param event_p             RW:  event
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_ModR(const N8_SizedBuffer_t *modulus_p,
                    N8_SizedBuffer_t *result_p,
                    const N8_Unit_t unitID,
                    N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   ret = n8_pk_gen_command(NULL, NULL, modulus_p, result_p, unitID,
                           PK_Cmd_R_Mod_M,
                           event_p);
   return ret;
} /* N8_ModR */ 

/*****************************************************************************
 * N8_ModExponentiate
 *****************************************************************************/
/** @ingroup pkops
 * @brief Perform modular expontiation.
 *
 * Results = a ** b mod m.  Uses Montgomery's algorithm.
 *
 *  @param a_p                 RO:  a operand
 *  @param b_p                 RO:  b operand
 *  @param modulus_p           RO:  modulus
 *  @param result_p            RW:  results
 *  @param unitID              RO:  chip specifier
 *  @param event_p             RW:  event structure
 *
 * @par Externals
 *    None
 *
 *    Status.  N8_STATUS_OK if successful.  Error condition otherwise.
 *
 * @par Errors
 *    None generated directly
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t N8_ModExponentiate(const N8_SizedBuffer_t *a_p,
                               const N8_SizedBuffer_t *b_p,
                               const N8_SizedBuffer_t *modulus_p,
                               N8_SizedBuffer_t *result_p,
                               const N8_Unit_t unitID,
                               N8_Event_t *event_p)
{
   N8_Status_t ret = N8_STATUS_OK;
   API_Request_t *req_p = NULL;

   N8_Buffer_t   *k_a_op_p = NULL;
   N8_Buffer_t   *k_b_op_p = NULL;
   N8_Buffer_t   *k_modulus_p = NULL;
   N8_Buffer_t   *k_result_p = NULL;

   uint32_t       k_a_op_a;
   uint32_t       k_b_op_a;
   uint32_t       k_modulus_a;
   uint32_t       k_result_a;

   unsigned int   nBytes;
   unsigned int   b_length_bytes;
   char          *p;
   do
   {
      ret = N8_preamble();
      CHECK_RETURN(ret);

      ret = verify_x_lt_y(a_p, modulus_p);
      CHECK_RETURN(ret);
      ret = verifyParameter(b_p, 1, N8_PK_MAX_SIZE);
      CHECK_RETURN(ret);
      ret = verifyParameter(modulus_p, 1, modulus_p->lengthBytes);
      CHECK_RETURN(ret);
      ret = verifyParameter(result_p, modulus_p->lengthBytes, N8_PK_MAX_SIZE);
      CHECK_RETURN(ret);

      /* set the length of the result to the length of the modulus */
      result_p->lengthBytes = modulus_p->lengthBytes;
      nBytes = NEXT_WORD_SIZE(modulus_p->lengthBytes) * 4;

      /* allocate user-space buffer */
      ret = createPKRequestBuffer(&req_p, 
                                  unitID, 
                                  N8_CB_EXPONENTIATE_NUMCMDS,
                                  resultHandlerGeneric,
                                  nBytes);
      CHECK_RETURN(ret);
      /* set the components of the kernel memory allocated:
       * k_a_op_p
       * k_b_op_p
       * k_modulus_p
       * k_result_p
       */
      k_a_op_p = (N8_Buffer_t *) ((int) req_p + req_p->dataoffset);
      k_a_op_a = req_p->qr.physicalAddress + req_p->dataoffset;

      k_b_op_p = k_a_op_p + NEXT_WORD_SIZE(modulus_p->lengthBytes);
      k_b_op_a = k_a_op_a + NEXT_WORD_SIZE(modulus_p->lengthBytes);

      k_modulus_p = k_b_op_p + NEXT_WORD_SIZE(modulus_p->lengthBytes);
      k_modulus_a = k_b_op_a + NEXT_WORD_SIZE(modulus_p->lengthBytes);

      k_result_p = k_modulus_p + NEXT_WORD_SIZE(modulus_p->lengthBytes);
      k_result_a = k_modulus_a + NEXT_WORD_SIZE(modulus_p->lengthBytes);
      
      req_p->copyBackTo_p = result_p->value_p;
      req_p->copyBackFrom_p = k_result_p;
      req_p->copyBackSize = modulus_p->lengthBytes;

      /* copy the data into the kernel buffers */
      memcpy(k_modulus_p, modulus_p->value_p, modulus_p->lengthBytes);
      /* create a buffer that is modulus length long and filled with leading
       * zeros. */
      memset(k_a_op_p, 0, modulus_p->lengthBytes);
      p = k_a_op_p + modulus_p->lengthBytes - a_p->lengthBytes;
      memcpy(p, a_p->value_p, a_p->lengthBytes);

      memcpy(k_b_op_p, b_p->value_p, b_p->lengthBytes);
      b_length_bytes = b_p->lengthBytes;

      ret = cb_exponentiate(req_p,                      /* request pointer */
                            k_a_op_a,                   /* physical address of A */
                            k_modulus_a,                /* physical address of
                                                         * modulus */ 
                            modulus_p->lengthBytes,     /* modulus length */
                            k_b_op_a,                   /* physical address of B */
                            b_length_bytes,             /* B length */
                            k_result_a,                 /* physical address of
                                                         * results */ 
                            req_p->PK_CommandBlock_ptr, /* pointer to command
                                                         * block area */ 
                            unitID);                    /* unit specifier */
      CHECK_RETURN(ret);
      QUEUE_AND_CHECK(event_p, req_p, ret);
      HANDLE_EVENT(event_p, req_p, ret);
      
   } while (N8_FALSE);
   
   /*
    * Clean up if we arrived from an error condition.
    */
   if (ret != N8_STATUS_OK)
   {
      freeRequest(req_p);
   }
  
   return ret;
} /* N8_ModExponentiate */
