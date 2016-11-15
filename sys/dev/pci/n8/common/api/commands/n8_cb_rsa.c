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

static char const n8_id[] = "$Id: n8_cb_rsa.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_cb_rsa.c
 *  @brief RSA Command Block Generator
 *
 * Generates all command blocks for RSA-related functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 12/17/03 bac   Added cb_rsaPublicDecrypt
 * 09/10/02 brr   Set command complete bit on last command block.
 * 05/07/02 bac   Fix Bug #758:  command blocks generated were incorrect if the
 *                modulus length was less than 2 BNC digits.  The commands
 *                blocks were restructured to behave correctly.
 * 05/09/02 brr   Modified allocateBNCConstants to use constant setup by driver.
 * 04/08/02 bac   Minor change to use precomputed key_p->pDigits and qDigits.
 * 03/26/02 brr   Allocate the data buffer as part of the API request. 
 * 02/21/02 brr   Perform BNC constant initialization once upon startup.
 * 02/20/02 brr   Removed references to the queue structure.
 * 12/12/01 bac   Use precomputes for RSA encrypt.  Created a new static method
 *                cb_exp_with_precomputes.  Fixes Bug #371.
 * 11/08/01 mel   Fixed bug #289 : Some calls use N8_SINGLE_CHIP in the 
 *                commands directory
 * 10/19/01 bac   Added support for SKS operations and RSA Decrypt for non-CRT.
 * 10/28/01 msz   Added locking allocateBNCConstants.
 * 10/08/01 bac   Changes for the case where len(p) != len(q) (BUG #180).
 *                Everywhere a data length was assumed to half the key length,
 *                it has been replaced with the specific dependency on the
 *                length of p or q.
 * 09/14/01 bac   Use new DBG_PRINT_PK_CMD_BLOCKS macros.
 * 09/05/01 bac   Minor formatting change.
 * 08/24/01 bac   Changed all methods to accept the pre-allocated command
 *                buffer. 
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon_driver_api.h.
 * 08/21/01 bac   Fixes to allow the use of odd key lengths (Bug #174).
 * 07/30/01 bac   Pass queue pointer to all length calculation macros.
 * 07/02/01 mel   Fixed comments.
 * 06/28/01 bac   Changed size of load on computeCX as it did not work for 'n',
 *                changed the interface to cb_rsaEncrypt and cb_rsaDecrypt to
 *                take physical addresses not N8_MemoryHandle_t.  Other minor
 *                naming/formatting fixes.
 * 06/27/01 bac   Removed calls to free request blocks on error.  This is
 *                handled in the caller.
 * 06/25/01 bac   Added static allocation of BNC constants, more conversion to
 *                physical addresses.
 * 06/19/01 bac   Corrected use of kernel memory.
 * 05/30/01 bac   Fixed printCommandBlock to display pleasingly for little
 *                endian machines. 
 * 05/03/01 bac   Added include for string.h
 * 04/30/01 bac   Added support for rsaDecrypt use with non-PKH command generation.
 * 04/24/01 bac   Rearranged N8DEBUG fences 
 * 04/11/01 bac   Standardization -- mainly support for _t suffix for types.
 * 04/05/01 bac   Changed return codes to be N8_Status instead of ints.
 * 04/05/01 bac   Removed unused debugging variable sz.
 * 04/05/01 bac   Changed all key length info to be in bytes, not digits.
 * 03/01/01 bac   Original version.
 ****************************************************************************/
/** @defgroup cb_rsa RSA Command Block Generator
 */

#include "n8_common.h"
#include "n8_pk_common.h"
#include "n8_pub_errors.h"
#include "n8_cb_rsa.h"
#include "n8_util.h"
#include "n8_driver_api.h"
#include "n8_semaphore.h"

/* #define ZERO_CMD_BLOCK(X) memset((X), 0, sizeof(PK_CMD_BLOCK_t)) */
#define ZERO_CMD_BLOCK(X)

/* globals */
static uint32_t        bnc_one_ga;
static uint32_t        bnc_2_D_ga;


/* predeclarations */
static N8_Boolean_t cb_RSAOperationSupported(const N8_RSAKeyObject_t *key_p);

/*****************************************************************************
 * cb_computeU
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief   Creates the command blocks to compute the value for u where
 * u = (p^-1) mod q  [ (multiplicative inverse of p) mod q ]
 *
 *  @param req_p                    RW:  Pointer to command
 *                                       blocks.  
 *  @param p                        RO:  Physical address of p.
 *  @param q                        RO:  Physical address of q.
 *  @param p_length_bytes           RO:  Length of parameter p in bytes
 *  @param q_length_bytes           RO:  Length of parameter q in bytes
 *  @param result_a                 WO:  Physical address of result
 *  @param key_length_bytes         RO:  Private key length.
 *
 * @return 
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_computeU(API_Request_t       *req_p,
                        const uint32_t       p,
                        const uint32_t       q,
                        uint32_t             result_a,
                        const int            p_length_bytes,
                        const int            q_length_bytes,
                        PK_CMD_BLOCK_t      *cb_p,
                        PK_CMD_BLOCK_t     **next_cb_pp)
{
  
   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;

   uint32_t p_length = BYTES_TO_PKDIGITS(p_length_bytes);
   uint32_t q_length = BYTES_TO_PKDIGITS(q_length_bytes);
   /* since p<q, then len(p) <= len(q).  we want the offsets to be in units of
    * the longer of the two, so we use q_length. */
   unsigned int offset[3];
   unsigned int i;
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      /*
       * BNC map:
       * 0:             p
       * 1              q
       * 2              p^-1 mod q
       */

      for (i = 0; i < sizeof(offset)/sizeof(unsigned int); i++)
      {
         offset[i] = i * q_length;
      }
      /* 1: Construct a command to load p */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;
      
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls = p; 
      ldst_wr_ptr->data_length = p_length_bytes;

      /* 2: Construct a command to load q */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[1];
      ldst_wr_ptr->data_addr_ls = q; 
      ldst_wr_ptr->data_length = q_length_bytes;

      /* 3: Construct a command for the operation p^-1 mod q */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Inverse_A_Mod_M;
      math_wr_ptr->r_offset = offset[2];
      math_wr_ptr->m_length_offset = 
         (q_length << PK_Cmd_Length_Shift) | offset[1]; /* q */
      math_wr_ptr->a_length_offset = 
         (p_length << PK_Cmd_Length_Shift) | offset[0]; /* p */
  
      /*
       * 4: Construct a command to store u
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R;
      ldst_wr_ptr->r_offset = offset[2];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = q_length_bytes;

      /* save next address for future use */
      *next_cb_pp = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);

      DBG_PRINT_PK_CMD_BLOCKS("Compute U",
                              (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                              N8_CB_COMPUTE_U_NUMCMDS);
   } while (FALSE);
  
   return ret;
} /* cb_computeU */

/*****************************************************************************
 * cb_computeDX
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief   Creates the command blocks to compute the value for dX where
 * dX = d mod ((X-1) mod X).  This is used to compute dp and dq.
 *
 * since (X-1) = additive_inverse(1), it can be computed as:
 *
 * dX = d mod(additive_inverse(1) mod X), which is more
 * straightforward to compute
 *
 *
 *  @param req_p                    RW:  Pointer to command
 *                                       blocks.  
 *  @param X                        RO:  Physical address of X.
 *  @param d                        RO:  Physical address of d.
 *  @param result_a                 WO:  Physical address of result
 *  @param key_length_bytes         RO:  Private key length.
 *
 * @return 
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_computeDX(API_Request_t *req_p,
                         const uint32_t X,
                         const uint32_t d,
                         uint32_t       result_a,
                         const int      key_length_bytes,
                         const int      X_length_bytes,
                         PK_CMD_BLOCK_t *cb_p,
                         PK_CMD_BLOCK_t **next_cb_pp,
                         const int      chip)
{
   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;

   uint32_t key_length = BYTES_TO_PKDIGITS(key_length_bytes);
   uint32_t X_length   = BYTES_TO_PKDIGITS(X_length_bytes);
   unsigned int offset[5];
   unsigned int i;
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      /*
       * BNC Layout
       *
       * 0 : result_a
       * 1 : X
       * 2 : 1
       * 3 : -1 mod X 
       * 4 : d
       * 5 : 
       */
      for (i = 0; i < sizeof(offset)/sizeof(unsigned int); i++)
      {
         offset[i] = i * key_length;
      }

      /* Construct a command to load X */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;

      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[1];
      ldst_wr_ptr->data_addr_ls = X; 
      ldst_wr_ptr->data_length = X_length_bytes;

      /* Construct a command to load a 1 into one BNC digit */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[2];
      ldst_wr_ptr->data_addr_ls = bnc_one_ga;
      ldst_wr_ptr->data_length = PK_Bytes_Per_BigNum_Digit;

      /* Construct a command to load d  */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[4];
      ldst_wr_ptr->data_addr_ls = d;
      ldst_wr_ptr->data_length = key_length_bytes;

      /*
       * Construct a command for the operation (additive_inverse(1) mod X) 
       */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Minus_A_Mod_M;
      math_wr_ptr->r_offset = offset[3];
      math_wr_ptr->m_length_offset = 
         (X_length << PK_Cmd_Length_Shift) | offset[1];
      math_wr_ptr->a_length_offset = (1 << PK_Cmd_Length_Shift) | offset[2];

      /*
       * Construct a command for the operation:
       * d mod ((X-1) mod X)
       */ 
      math_wr_ptr++;
      math_wr_ptr->opcode_si = PK_Cmd_A_Mod_M;
      math_wr_ptr->r_offset = offset[0];
      math_wr_ptr->m_length_offset = 
         (X_length << PK_Cmd_Length_Shift) | offset[3];
      math_wr_ptr->a_length_offset = 
         (key_length << PK_Cmd_Length_Shift) | offset[4];


      /*
       * Construct a command to store dX
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = X_length_bytes;

      /* save next address for future use */
      *next_cb_pp = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);

      DBG_PRINT_PK_CMD_BLOCKS("Compute DX", 
                              (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                              N8_CB_COMPUTE_DX_NUMCMDS);
   } while (FALSE);

   return ret;
} /* computeDX */

/*****************************************************************************
 * cb_computeCX
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief  Creates the command blocks to compute the value for cX where
 * cX = -X[0]^-1:  the additive inverse of the multiplicative inverse
 * of the least significant digit of X.
 * This is used to compute cp and cq.
 *
 *
 *  @param req_p                    RW:  Pointer to command
 *                                       blocks.  
 *  @param X                        RO:  Physical address of X.
 *  @param result_a                 WO:  Physical address of result
 *  @param X_length_bytes           RO:  X length in bytes.
 *
 * @return 
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_INVALID_OBJECT   -   command block pointer is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_computeCX(const API_Request_t *req_p,
                         const uint32_t       X,
                         const uint32_t       result_a,
                         const int            X_length_bytes,
                         PK_CMD_BLOCK_t      *cb_p,
                         PK_CMD_BLOCK_t     **next_cb_pp,
                         const N8_Unit_t      unitID,
			 int                  lastCmdBlock)
{
   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;

   uint32_t X_length = BYTES_TO_PKDIGITS(X_length_bytes);
   unsigned int offset[4];

   N8_Status_t ret = N8_STATUS_OK;
   unsigned int i;
   do
   {
      /*
       * Initialize the offset values.  These are to address temporary
       * storage in the BNC.  The offsets accomodate operand sizes up to
       * the key length
       *
       * The BNC layout will look like the following:
       *
       * -----------------------------------
       * 0: results             2 digits
       * -----------------------------------
       * 1: t = p[0]^-1         2 digits
       * -----------------------------------
       * 2: 2**128              2 digits
       * -----------------------------------
       * 3: p                   X_length
       * -----------------------------------
       */
      for (i = 0; i < sizeof(offset)/sizeof(unsigned int); i++)
      {
         offset[i] = i * 2;
      }
      /* Compute cX = -(p[0]^-1 mod 2^128) mod 2^128 */
      /* Command 1 */
      /* Construct a command to load p[0] to Slot 3 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;

      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[3];
      ldst_wr_ptr->data_addr_ls = X;
      ldst_wr_ptr->data_length = X_length_bytes;

      /* Command 2 */
      /* Construct a command to load a 2-digit 2^128 to Slot 2*/
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[2];
      ldst_wr_ptr->data_addr_ls = bnc_2_D_ga;
      ldst_wr_ptr->data_length = PKDIGITS_TO_BYTES(2);

      /* Command 3 */
      /* Construct a command for the operation t = p[0]^-1 mod 2^128 to Slot 1 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Inverse_A_Mod_M;
      math_wr_ptr->r_offset = offset[1];
      math_wr_ptr->m_length_offset = 
         (2 << PK_Cmd_Length_Shift) | offset[2];
      math_wr_ptr->a_length_offset = 
         (1 << PK_Cmd_Length_Shift) | (offset[3] + X_length - 1);

      /* Command 4 */
      /* Construct a command for the operation cX = -t mod 2^128  to Slot 0 */
      math_wr_ptr++;
      math_wr_ptr->opcode_si = PK_Cmd_Minus_A_Mod_M;
      math_wr_ptr->r_offset = offset[0];
      math_wr_ptr->m_length_offset = 
         (2 << PK_Cmd_Length_Shift) | offset[2];
      math_wr_ptr->a_length_offset = 
         (1 << PK_Cmd_Length_Shift) | (offset[1] + 1);

      /* Command 5 */
      /* Construct a command to store cX (second digit of previous result) */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R;
      if (lastCmdBlock == N8_TRUE)
      {
         ldst_wr_ptr->opcode_si |= PK_Cmd_SI_Mask;
      }
      ldst_wr_ptr->r_offset = offset[0] + 1;
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = PK_Bytes_Per_BigNum_Digit;

      /* save next address for future use */
      *next_cb_pp = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);

      DBG_PRINT_PK_CMD_BLOCKS("Compute CX",
                              cb_p,
                              N8_CB_COMPUTE_CX_NUMCMDS); 

   } while (FALSE);
  
   return ret;
  
} /* computeCX */


/*****************************************************************************
 * cb_computeRmodX
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief   Creates the command blocks to compute the value for R mod X.
 *
 *
 *  @param req_p                    RW:  Pointer to command
 *                                       blocks.  
 *  @param X                        RO:  Physical address of X.
 *  @param result_a                 WO:  Physical address of result
 *  @param key_length_bytes         RO:  Length of X in bytes
 *
 * @return 
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_computeRmodX(const API_Request_t *req_p,
                            const uint32_t      X,
                            const uint32_t      result_a,
                            const unsigned int  key_length_bytes,
                            PK_CMD_BLOCK_t      *cb_p,
                            PK_CMD_BLOCK_t      **next_cb_pp,
			    int                 lastCmdBlock)
{
   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;

   uint32_t key_length = BYTES_TO_PKDIGITS(key_length_bytes);
   uint32_t offset0 = 0;
   uint32_t offset1 = key_length;
   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      /* Compute R mod X */
      /* 1: Construct a command to load X */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset1;
      ldst_wr_ptr->data_addr_ls = X;
      ldst_wr_ptr->data_length = key_length_bytes;

      /* 2: Construct a command for the operation R mod X */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_R_Mod_M;
      math_wr_ptr->r_offset = offset0;
      math_wr_ptr->m_length_offset = 
         (key_length << PK_Cmd_Length_Shift) | offset1;

      /* 3: Construct a command to store R mod p  */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R;
      if (lastCmdBlock == N8_TRUE)
      {
         ldst_wr_ptr->opcode_si |= PK_Cmd_SI_Mask;
      } 
      ldst_wr_ptr->r_offset = offset0;
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = key_length_bytes;

      /* save next address for future use */
      *next_cb_pp = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);

      DBG_PRINT_PK_CMD_BLOCKS("Compute R mod X",
                              cb_p,
                              N8_CB_COMPUTE_RMODX_NUMCMDS);
   } while (FALSE);
 
   return ret;
} /* compute RmodX */

N8_Status_t cb_exponentiate(API_Request_t           *req_p,
                            const uint32_t           origMsg_a,
                            const uint32_t           modulus_a,
                            const unsigned int       modulus_length_bytes,
                            const uint32_t           exponent_a,
                            const unsigned int       exp_length_bytes,
                            const uint32_t           result_a,
                            PK_CMD_BLOCK_t          *cb_p,
                            const N8_Unit_t          unitID)
{
   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;
   uint32_t                modulus_length_digits;
   uint32_t                exp_length_digits;
   uint32_t                offset[7];
   unsigned int            i;
   N8_Status_t             ret = N8_STATUS_OK;

   do
   {
      modulus_length_digits = BYTES_TO_PKDIGITS(modulus_length_bytes);
      exp_length_digits = BYTES_TO_PKDIGITS(exp_length_bytes);

      DBG(("constructing exponentiate command blocks\n"));

      /*
       * Initialize the offset values.  These are to address temporary
       * storage in the BNC.  The offsets accomodate operand sizes up to
       * the key length
       *
       * The BNC layout will look like the following:
       *
       * -----------------------------------
       * 0: n, results
       * -----------------------------------
       * 1: R mod n , o * Rmodn mod n
       * -----------------------------------
       * 2: o (original message)
       * -----------------------------------
       * 3: exponent
       * -----------------------------------
       * 4: 2**128
       * -----------------------------------
       * 5: cn
       * -----------------------------------
       * 6: t
       * -----------------------------------
       *
       * Slots 0-2 are of width |n|, the modulus length in BNC digits.
       * Slot 3 is width |e|, the exponent length in BNC digits.
       * Slots 4-6 are of width 2.
       */

      for (i = 0; i < 4; i++)
      {
         offset[i] = i * modulus_length_digits;
      }
      for (i = 0; i < 3; i++)
      {
         offset[i+4] = offset[3] + exp_length_digits + i * 2;
      }

      /* 1) Compute R mod n. */

      /* Construct a command to load n to Slot 0 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls =  modulus_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      /* Construct a command for the operation R mod n to Slot 1 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_R_Mod_M;
      math_wr_ptr->r_offset = offset[1];
      math_wr_ptr->m_length_offset = (modulus_length_digits << PK_Cmd_Length_Shift) | offset[0];

      /*
       * 2) Compute cn = -(n[0]^-1 mod 2^128) mod 2^128
       * n is already loaded in Slot 0
       */

      /*
       * Construct a command to load a
       * 2-digit 2^128 to Slot 4
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[4];
      ldst_wr_ptr->data_addr_ls = bnc_2_D_ga;
      ldst_wr_ptr->data_length = PKDIGITS_TO_BYTES(2);

      /* Construct a command for the operation t = n[0]^-1 mod 2^128 to Slot 6 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Inverse_A_Mod_M;
      math_wr_ptr->r_offset = offset[6];
      math_wr_ptr->m_length_offset = 
         (2 << PK_Cmd_Length_Shift) | offset[4];
      math_wr_ptr->a_length_offset = 
         (1 << PK_Cmd_Length_Shift) | (offset[0] + modulus_length_digits - 1);
   
      /* Construct a command for the operation cn = -t mod 2^128 to Slot 5*/
      math_wr_ptr++;
      math_wr_ptr->opcode_si = PK_Cmd_Minus_A_Mod_M;
      math_wr_ptr->r_offset = offset[5];
      math_wr_ptr->m_length_offset = 
         (2 << PK_Cmd_Length_Shift) | offset[4];
      math_wr_ptr->a_length_offset = 
         (1 << PK_Cmd_Length_Shift) | (offset[6] + 1);
   
      /*
       * 3) Compute o * R mod n (mod n)
       */

      /* Construct a command to load the original message to Slot 2*/
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[2];
      ldst_wr_ptr->data_addr_ls = origMsg_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      /* Construct a command for the operation o * Rmodn mod n to Slot 1*/
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = offset[1];
      math_wr_ptr->m_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[0]; /* n */
      math_wr_ptr->a_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[2]; /* o */
      math_wr_ptr->b_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[1]; /* R mod n */

      /*
       * 4) Compute o^e mod n
       */

      /* Construct a command to load e to Slot 3 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[3];
      ldst_wr_ptr->data_addr_ls = exponent_a;
      ldst_wr_ptr->data_length = exp_length_bytes;

      /* Construct a command for the operation o^e mod n to Slot 0 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
      math_wr_ptr->r_offset = offset[0];
      math_wr_ptr->m_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[0]; /* n */
      math_wr_ptr->a_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[1]; /* o * R mod n */
      math_wr_ptr->b_length_offset = (exp_length_digits << PK_Cmd_Length_Shift) | offset[3]; /* exponent */
      math_wr_ptr->c_offset = offset[5] + 1; /* cn */

      /* Construct a command to store o^e mod n  */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      DBG_PRINT_PK_CMD_BLOCKS("rsa exponentiate",
                              req_p->PK_CommandBlock_ptr,
                              N8_CB_EXPONENTIATE_NUMCMDS);
   } while (FALSE);
   return ret;
} /* n8_cb_exponentiate */

static N8_Status_t cb_exp_with_precomputes(API_Request_t           *req_p,
                                           const uint32_t           origMsg_a,
                                           const uint32_t           modulus_a,
                                           const unsigned int       modulus_length_bytes,
                                           const uint32_t           exponent_a,
                                           const unsigned int       exp_length_bytes,
                                           const uint32_t           cn_a,
                                           const uint32_t           R_mod_n_a,
                                           const uint32_t           result_a,
                                           PK_CMD_BLOCK_t          *cb_p,
                                           const N8_Unit_t          unitID)
{
   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;
   uint32_t                modulus_length_digits;
   uint32_t                exp_length_digits;
   uint32_t                offset[6];
   int                     i;
   N8_Status_t             ret = N8_STATUS_OK;

   do
   {
      modulus_length_digits = BYTES_TO_PKDIGITS(modulus_length_bytes);
      exp_length_digits = BYTES_TO_PKDIGITS(exp_length_bytes);

      DBG(("constructing exponentiate with precomputes command blocks\n"));

      /*
       * Initialize the offset values.  These are to address temporary
       * storage in the BNC.  The offsets accomodate operand sizes up to
       * the key length
       */

      for (i = 0; i < sizeof(offset)/sizeof(unsigned int); i++)
      {
         offset[i] = i * modulus_length_digits;
      }

      /*
       * 1) Construct a command to load n to offset[1]
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[1];
      ldst_wr_ptr->data_addr_ls =  modulus_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      /*
       * 2) Load R mod n to offset[2]
       */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[2];
      ldst_wr_ptr->data_addr_ls =  R_mod_n_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      /*
       * 3) Load cn to offset[3]
       */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[3];
      ldst_wr_ptr->data_addr_ls =  cn_a;
      ldst_wr_ptr->data_length = PK_RSA_CN_Byte_Length;

      /*
       * Compute o * R mod n (mod n)
       */

      /*
       * 4) Construct a command to load the original message to offset[5] 
       */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[5];
      ldst_wr_ptr->data_addr_ls = origMsg_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      /*
       * 5) Construct a command for the operation o * Rmodn mod n to offset[2]
       */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = offset[2];
      math_wr_ptr->m_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[1];
      math_wr_ptr->a_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[5];
      math_wr_ptr->b_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[2];

      /*
       * Compute o^e mod n
       */

      /*
       * 6) Construct a command to load e to offset[4]
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[4];
      ldst_wr_ptr->data_addr_ls = exponent_a;
      ldst_wr_ptr->data_length = exp_length_bytes;

      /*
       * 7) Construct a command for the operation o^e mod n to offset[0]
       */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
      math_wr_ptr->r_offset = offset[0];
      math_wr_ptr->m_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[1];
      math_wr_ptr->a_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[2];
      math_wr_ptr->b_length_offset = (exp_length_digits << PK_Cmd_Length_Shift) | offset[4];
      math_wr_ptr->c_offset = offset[3];

      /*
       * 8) Construct a command to store o^e mod n from offset[0]
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      DBG_PRINT_PK_CMD_BLOCKS("rsa exponentiate with precomputes",
                              req_p->PK_CommandBlock_ptr,
                              N8_CB_RSA_ENCRYPT_NUMCMDS);
   } while (FALSE);
   return ret;
} /* cb_exponentiate_with_precomputes */

/*****************************************************************************
 * cb_rsaEncrypt
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief   Takes an incoming message and encrypts it via exponentiation with
 * the public key.
 *
 *  @param req_p                    RW:  Pointer to command
 *                                       blocks.  
 *  @param key                      RO:  Key object with precomputed values for
 *                                       speedier computation
 *  @param origMsg                  RO:  Physical address of original message.
 *  @param result_a                 WO:  Physical address of result
 *
 * @return 
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_rsaEncrypt(API_Request_t           *req_p,
                          const N8_RSAKeyObject_t *key_p,
                          const uint32_t           origMsg_a,
                          const uint32_t           result_a,
                          PK_CMD_BLOCK_t          *cb_p,
                          const N8_Unit_t          unitID)
{
   /* To compute an encryption of the plaintext using the recipients public key,
    * a simple exponentiation is required.
    * cipher_text = plain_text ** public_key mod n.
    */
   return cb_exp_with_precomputes(req_p,                   /* API request pointer */
                                  origMsg_a,               /* plain text */
                                  key_p->n,                /* modulus */
                                  key_p->privateKeyLength, /* modulus length */
                                  key_p->key,              /* public key */
                                  key_p->publicKeyLength,  /* public key length */
                                  key_p->cn,               /* precomputed cn */
                                  key_p->R_mod_n,          /* precomputed R mod n */
                                  result_a,                /* result buffer */
                                  cb_p,                    /* command block pointer */
                                  unitID);                 /* chip ID */
}


/*****************************************************************************
 * cb_rsaDecrypt
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief   Takes an incoming message and decrypts it by using the PKE RSA
 * private function
 *
 *  @param req_p                    RW:  Pointer to command
 *                                       blocks.  
 *  @param key                      RO:  Key object with precomputed values for
 *                                       speedier computation
 *  @param origMsg                  RO:  Physical address of original message.
 *  @param result_a                 WO:  Physical address of result
 *
 * @return 
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_rsaDecrypt(API_Request_t           *req_p,
                          const N8_RSAKeyObject_t *key_p,
                          const uint32_t           origMsg_a,
                          const uint32_t           result_a,
                          PK_CMD_BLOCK_t          *cb_p,
                          const N8_Unit_t          unitID)
{
   PK_RSA_CMD_BLOCK_t   *rsa_wr_ptr = NULL;
   PK_CMD_BLOCK_t       *math_wr_ptr = NULL;
   PK_LDST_CMD_BLOCK_t  *ldst_wr_ptr = NULL;
   uint32_t             key_length;
   uint32_t             half_key_length;
   uint32_t             key_length_bytes;
   uint32_t             sks_word;
   N8_Status_t          ret = N8_STATUS_OK;
   uint32_t             paramBlockOffset;
   unsigned int         offset[3];
   uint32_t             i;
   uint32_t             p_offset;
   uint32_t             q_offset;
   uint32_t             n_offset;
   uint32_t             dp_offset;
   uint32_t             dq_offset;
   uint32_t             cp_offset;
   uint32_t             cq_offset;
   uint32_t             rmodp_offset;
   uint32_t             rmodq_offset;
   uint32_t             u_offset;

   do
   {
      /* If the key type is N8_PRIVATE, we are to do a private decrypt without
       * the knowledge of p and q, thus we cannot utilize the Chinese Remainder
       * Theorem algorithm.  Instead we must simply compute the plaintext using:
       * plain = msg ** private_key mod n.
       */
      if (key_p->keyType == N8_PRIVATE)
      {
         return cb_exponentiate(req_p,                   /* API request pointer */
                                origMsg_a,               /* cipher text */
                                key_p->n,                /* modulus */
                                key_p->privateKeyLength, /* modulus length */
                                key_p->key,              /* private key */
                                key_p->privateKeyLength, /* private key length */
                                result_a,                /* result buffer */
                                cb_p,                   /* command block pointer */
                                unitID);
      }
      key_length_bytes = key_p->privateKeyLength;
      key_length = BYTES_TO_PKDIGITS(key_length_bytes);
      half_key_length = BYTES_TO_PKDIGITS(key_length_bytes/2);

      for (i = 0; i < sizeof(offset)/sizeof(unsigned int); i++)
      {
         offset[i] = i * key_length;
      }
      /* set up the offsets */
      paramBlockOffset = offset[2];
      p_offset     = paramBlockOffset + PK_RSA_P_Param_Offset(key_p);
      q_offset     = paramBlockOffset + PK_RSA_Q_Param_Offset(key_p);
      n_offset     = paramBlockOffset + PK_RSA_N_Param_Offset(key_p);
      dp_offset    = paramBlockOffset + PK_RSA_DP_Param_Offset(key_p);
      dq_offset    = paramBlockOffset + PK_RSA_DQ_Param_Offset(key_p);
      cp_offset    = paramBlockOffset + PK_RSA_CP_Param_Offset(key_p);
      cq_offset    = paramBlockOffset + PK_RSA_CQ_Param_Offset(key_p);
      rmodp_offset = paramBlockOffset + PK_RSA_R_MOD_P_Param_Offset(key_p);
      rmodq_offset = paramBlockOffset + PK_RSA_R_MOD_Q_Param_Offset(key_p);
      u_offset     = paramBlockOffset + PK_RSA_U_Param_Offset(key_p);

      /* 1: Construct a command to load the original message */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls = origMsg_a;
      ldst_wr_ptr->data_length = key_length_bytes;

      if (key_p->keyType != N8_PRIVATE_SKS)
      {
         /* 2: Construct a command to load the RSA parameter block */
         ldst_wr_ptr++;
         ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
         ldst_wr_ptr->r_offset = paramBlockOffset;
         ldst_wr_ptr->data_addr_ls = key_p->paramBlock;
         ldst_wr_ptr->data_length = PK_RSA_Param_Byte_Length(key_p);
         sks_word = PK_Cmd_N_Mask;
      }
      else
      {
         /* we are using the sks.  set the sks_word variable to the correct
          * offset */
         sks_word = key_p->SKSKeyHandle.sks_offset;
      }

      if (key_p->keyType == N8_PRIVATE_SKS ||
          cb_RSAOperationSupported(key_p))
      {
         /* 2: Construct a command for the RSA operation */
         rsa_wr_ptr = (PK_RSA_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
         ZERO_CMD_BLOCK(rsa_wr_ptr);
         rsa_wr_ptr->opcode_si = PK_Cmd_RSA_Private_Key_Op;
         rsa_wr_ptr->sks = ((key_length) << PK_Cmd_Key_Length_Shift) | sks_word;

         /* Set up the ldst pointer for the next command. */
         ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (rsa_wr_ptr + 1);
      }
      else
      {
         /* 3: o = i mod p */
         math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
         math_wr_ptr->opcode_si = PK_Cmd_A_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | p_offset;
         math_wr_ptr->a_length_offset = (key_length     << PK_Cmd_Length_Shift) | offset[0];

         /* 4: o = (o * R mod p) mod p */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | p_offset;
         math_wr_ptr->a_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | offset[1];
         math_wr_ptr->b_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | rmodp_offset;

         /* 5: dp = o ^ dp mod p */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
         math_wr_ptr->r_offset = dp_offset;
         math_wr_ptr->m_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | p_offset;
         math_wr_ptr->a_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | offset[1];
         math_wr_ptr->b_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | dp_offset;
         math_wr_ptr->c_offset = cp_offset;

      /* 6: o = i mod q */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_A_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | q_offset;
         math_wr_ptr->a_length_offset = (key_length     << PK_Cmd_Length_Shift) | offset[0];

      /* 7: o = o * R mod q (mod q) */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | q_offset;
         math_wr_ptr->a_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | offset[1]; 
         math_wr_ptr->b_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | rmodq_offset; 

         /* 8: dq = o ^ dq mod q */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
         math_wr_ptr->r_offset = dq_offset;
         math_wr_ptr->m_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | q_offset;
         math_wr_ptr->a_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | offset[1];
         math_wr_ptr->b_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | dq_offset;
         math_wr_ptr->c_offset = cq_offset;

      /* 9: o = dq - dp mod q */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_A_Minus_B_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | q_offset;
         math_wr_ptr->a_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | dq_offset;
         math_wr_ptr->b_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | dp_offset;

         /* 10: o = o * u mod q */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | q_offset;
         math_wr_ptr->a_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | offset[1];
         math_wr_ptr->b_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | u_offset;

         /* 11: o = o * p mod n */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_length     << PK_Cmd_Length_Shift) | n_offset;
         math_wr_ptr->a_length_offset = (key_p->qDigits << PK_Cmd_Length_Shift) | offset[1];
         math_wr_ptr->b_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | p_offset;

         /* 12: o = o + dp mod n */
         math_wr_ptr++;
         math_wr_ptr->opcode_si = PK_Cmd_A_Plus_B_Mod_M;
         math_wr_ptr->r_offset = offset[1];
         math_wr_ptr->m_length_offset = (key_length     << PK_Cmd_Length_Shift) | n_offset;
         math_wr_ptr->a_length_offset = (key_length     << PK_Cmd_Length_Shift) | offset[1];
         math_wr_ptr->b_length_offset = (key_p->pDigits << PK_Cmd_Length_Shift) | dp_offset;
      
         /* Set up the ldst pointer for the next command. */
         ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      }
      /* 3/13: Construct a command to store r */
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = offset[1];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = key_length_bytes;

      DBG_PRINT_PK_CMD_BLOCKS("rsa decrypt",
                              (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                              N8_CB_RSA_DECRYPT_NUMCMDS(key_p));

   } while (FALSE);

   return ret;

} /* rsaDecrypt */

/*****************************************************************************
 * cb_RSADecryptOperations
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief Calculate the number of operations required for an RSA Decrypt.
 *
 * The operations used to perform an RSA Decrypt depend upon the key type and
 * the lengths of p, q, and the private key.  If the type is N8_PRIVATE_SKS,
 * then the RSA Operation can be used and the data is taken from the SKS, saving
 * a load.  If it is not SKS and len(p) == len(q) AND len(p) + len(q) ==
 * len(private key), then the RSA Operation can be used.  If the above does not
 * hold, then this presents a situation where the RSA Operation does not work
 * and the command blocks must be generated by hand.
 *
 *  @param key_p               RO:  Pointer to the key object.
 *
 * @par Externals
 *    None
 *
 * @return
 *    number of commands necessary
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    The key pointer is valid.  We assume this has been checked by the API.
 *****************************************************************************/
unsigned int cb_RSADecryptOperations(const N8_RSAKeyObject_t *key_p)
{
   unsigned int ret = 0;
   const unsigned int USING_SKS_NUM_CMDS = 3;
   const unsigned int USING_RSA_OPERATION_NUM_CMDS = 4;
   const unsigned int USING_RSA_WORKAROUND_NUM_CMDS = 13;

   if (key_p->keyType == N8_PRIVATE_SKS)
   {
      ret = USING_SKS_NUM_CMDS;
   }
   else if (key_p->keyType == N8_PRIVATE)
   {
      ret = N8_CB_EXPONENTIATE_NUMCMDS;
   }
   else if (cb_RSAOperationSupported(key_p))
   {
      ret = USING_RSA_OPERATION_NUM_CMDS;
   }
   else
   {
      ret = USING_RSA_WORKAROUND_NUM_CMDS;
   }
   return ret;
} /* cb_RSADecryptOperations */

/*****************************************************************************
 * cb_rsaPublicDecrypt
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief  Creates the command blocks to compute a public decrypt for
 *         openssl. This command computes the pre-computes ComputeCX and
 *         ComputeRmodX and then performs the exponentiation
 *
 * @param req_p                RO: Pointer to request
 * @param modulus              RO: modulus (n)
 * @param modulus_length_bytes RO: length of modulus in bytes
 * @param origMsg_a            RO: Physical address of original message
 * @param result_a             RO: Physical address of result buffer
 * @param exponent_a           RO: Physical address of exponent (key)
 * @param exp_length_bytes     RO: length of exponent in bytes
 * @param cb_p                 WO: command block pointer
 * @param unitID               RO: execution unit identifier
 *
 * @return 
 *    returnResult - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_INVALID_OBJECT   -   command block pointer is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_rsaPublicDecrypt(const API_Request_t *req_p,
                                const uint32_t       modulus,
                                const unsigned int   modulus_length_bytes,
                                const uint32_t       origMsg_a,
                                const uint32_t       result_a,
                                const uint32_t       exponent_a,
                                const unsigned int   exp_length_bytes,
                                PK_CMD_BLOCK_t      *cb_p,
                                const N8_Unit_t      unitID)
{

   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;

   uint32_t                modulus_length_digits;
   uint32_t                exp_length_digits;
   uint32_t                offset[6];

   N8_Status_t             ret = N8_STATUS_OK;
   unsigned int i;
   do
   {
      /*
       * Initialize the offset values.  These are to address temporary
       * storage in the BNC.  The offsets accomodate operand sizes up to
       * the key length
       *
       * The BNC layout will look like the following:
       *
       * -----------------------------------
       * 0: results             2 digits
       * -----------------------------------
       * 1: t = p[0]^-1         2 digits
       * -----------------------------------
       * 2: 2**128              2 digits
       * -----------------------------------
       * 3: p                   modulus_length
       * -----------------------------------
       */

      modulus_length_digits = BYTES_TO_PKDIGITS(modulus_length_bytes);
      exp_length_digits = BYTES_TO_PKDIGITS(exp_length_bytes);

      /*
       * Initialize the offset values.  These are to address temporary
       * storage in the BNC.  The offsets accomodate operand sizes up to
       * the key length
       */
      for (i = 0; i < sizeof(offset)/sizeof(unsigned int); i++)
      {
         offset[i] = i * modulus_length_digits;
      }

      /* 1. Compute cX = -(p[0]^-1 mod 2^128) mod 2^128 */
      /* 2. Compute R mod X 				*/
      /* 3. Compute o * Rmodn mod n 			*/

      /*Note - offset and slot mean the same thing */


      /*
       * 1) Construct a command to load n to slot[1]
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[1];
      ldst_wr_ptr->data_addr_ls =  modulus;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      /* Command 2 */
      /* Construct a command to load a 2-digit 2^128 to Slot 2*/
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[2];
      ldst_wr_ptr->data_addr_ls = bnc_2_D_ga;
      ldst_wr_ptr->data_length = PKDIGITS_TO_BYTES(2);

      /* Command 3 */
      /* Construct a command for the operation t = n[0]^-1 mod 2^128 to Slot 3 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Inverse_A_Mod_M;
      math_wr_ptr->r_offset = offset[3];
      math_wr_ptr->m_length_offset = 
         (2 << PK_Cmd_Length_Shift) | offset[2];
      math_wr_ptr->a_length_offset = 
         (1 << PK_Cmd_Length_Shift) | (offset[1] + modulus_length_digits - 1);

      /* Command 4 */
      /* Construct a command for the operation cX = -t mod 2^128  to Slot 3 */
      math_wr_ptr++;
      math_wr_ptr->opcode_si = PK_Cmd_Minus_A_Mod_M;
      math_wr_ptr->r_offset = offset[3];
      math_wr_ptr->m_length_offset = 
         (2 << PK_Cmd_Length_Shift) | offset[2];
      math_wr_ptr->a_length_offset = 
         (1 << PK_Cmd_Length_Shift) | (offset[3] + 1);

      /* 5: Construct a command for the operation R mod X */
      /* math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1); */
      math_wr_ptr++;
      math_wr_ptr->opcode_si = PK_Cmd_R_Mod_M;
      math_wr_ptr->r_offset = offset[2];
      math_wr_ptr->m_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[1];

      /*
       * Compute o * R mod n (mod n)
       */

      /*
       * 6) Construct a command to load the original message to offset[0] 
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      /* ldst_wr_ptr++; */
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls = origMsg_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      /*
       * 7) Construct a command for the operation o * Rmodn mod n to offset[0]
       */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = offset[0];
      math_wr_ptr->m_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[1];
      math_wr_ptr->a_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[0];
      math_wr_ptr->b_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[2];

      /*
       * Compute o^e mod n
       */

      /*
       * 8) Construct a command to load e to offset[4]
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[4];
      ldst_wr_ptr->data_addr_ls = exponent_a;
      ldst_wr_ptr->data_length = exp_length_bytes;

      /*
       * 9) Construct a command for the operation o^e mod n to offset[0]
       */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
      math_wr_ptr->r_offset = offset[0];
      math_wr_ptr->m_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[1];
      math_wr_ptr->a_length_offset = 
         (modulus_length_digits << PK_Cmd_Length_Shift) | offset[0];
      math_wr_ptr->b_length_offset = (exp_length_digits << PK_Cmd_Length_Shift) | offset[4];
      /* take the least significant digit of the cn result */
      math_wr_ptr->c_offset = offset[3]+1;

      /*
       * 10) Construct a command to store o^e mod n from offset[0]
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = offset[0];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = modulus_length_bytes;

      DBG_PRINT_PK_CMD_BLOCKS("compute public precompute whaley-style",
                              req_p->PK_CommandBlock_ptr,
                              10);
   } while (FALSE);
   return ret;
} /* cb_compute_public_decrypt */

/**************************************************
 * Local Functions
 **************************************************/

/*****************************************************************************
 * allocateBNCConstants
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief Sets addresses for constants allocated in kernel space that are
 *        needed for various commands.
 *
 * Some command need one of two constants to be loaded into the BNC.  One is the
 * constant '1', in a single BNC digit.  This is 'bnc_one'.  The other is the
 * value 2^D where D is the size in bits of a BNC digit.  This changes whether
 * the actual hardware or FPGA is in use.  Note the values needed are:<br>
 * bnc_one = 0001 (for the case where D=4)<BR>
 * bnc_2_D = 0001 0000<BR>
 * We will take advantage of this by allocating two digits and setting the
 * values.  In actuality, bnc_one and bnc_2_D will point to the same memory but
 * the length used will determine the value.
 *
 *  @param bncAddress          RO:  address of BNC constants allocated by
 *                                  the driver.
 *
 * @par Externals
 *
 * @return
 *    Return codes - N8_STATUS_OK
 *
 * @par Errors
 *
 * @par Assumptions
 *****************************************************************************/
N8_Status_t allocateBNCConstants(unsigned long bncAddress)
{
   N8_Status_t ret = N8_STATUS_OK;

   bnc_one_ga = bncAddress;
   bnc_2_D_ga = bncAddress;

   return ret;
} /* allocateBNCConstants */

static N8_Boolean_t cb_RSAOperationSupported(const N8_RSAKeyObject_t *key_p)
{
   N8_Boolean_t ret = N8_FALSE;

   if ((key_p->pLength == key_p->qLength) &&
       ((key_p->pLength+key_p->qLength) == key_p->privateKeyLength) &&
       ((key_p->pDigits+key_p->qDigits) == key_p->privateKeyDigits))
   {
      ret = N8_TRUE;
   }
   return ret;
} /* RSAOperationSupported */
