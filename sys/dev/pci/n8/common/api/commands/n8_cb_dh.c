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

static char const n8_id[] = "$Id: n8_cb_dh.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_cb_dsa.c
 *  @brief DSA Command Block Generator
 *
 * Generates all command blocks for DSA-related functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/16/03 brr   Eliminate obsolete include file.
 * 09/10/02 brr   Set command complete bit on last command block.
 * 08/01/02 bac   Fixed a bug where the incorrect lengths were being used in the
 *                data loads and stores.  The exact length of p should be used,
 *                not the full length of the required BNC digits (i.e. should
 *                not be multiples of 16 bytes).
 * 02/20/02 brr   Removed references to the queue structure.
 * 11/08/01 mel   Added unit ID parameter to commend block calls (bug #289).
 * 09/14/01 bac   Added computation of gRmodP in cb_precomputeDHValues.
 *                Changed cb_computeGXmodp to be cb_computeGXmodp_short and
 *                cb_computeGXmodp_long.  The former uses the precomputed value
 *                for gRmodP and the latter computes it.  Other changes from the
 *                code review.
 * 09/05/01 bac   cb_computeCX signature changed.
 * 08/24/01 bac   Revamp the two public routines for complete implementation.
 * 08/01/01 bac   Added queue_p to length calculations.
 * 07/19/01 bac   Fixed non-compilation problems.
 * 07/12/01 bac   Changed signatures for cb_computeGXmodp to accept uint32_t not
 *                pointers.
 * 05/22/01 mel   Original version.
 ****************************************************************************/
/** @defgroup cb_dh Diffie-Hellman Command Block Generator
 */

#include "n8_common.h"
#include "n8_pk_common.h"
#include "n8_pub_errors.h"
#include "n8_cb_dh.h"
#include "n8_util.h"

/*****************************************************************************
 * cb_precomputeDHValues
 *****************************************************************************/
/** @ingroup cb_dh
 * @brief Generate precompute values for Diffie-Hellman
 *
 * A series of DH computations can be made using the shared insecure values g
 * and n.  For each new DH computation a random large integer x is chosen.  The
 * DH computation is R= g^x mod p.  In order to do an exponentiation in the
 * NSP2000 hardware, the values presented are:
 *  a = g*(R mod p) mod p
 *  b = x
 *  c = cp (as computed by cb_computeCX).
 *
 * To facilitate the easy computation of g**x mod n, the values for a and c are
 * precomputed and stored in the DH Object for re-use.
 *
 *  @param req_p               RO:  pointer to the request structure
 *  @param g_a                 RO:  physical address for the g value
 *  @param p_a                 RO:  physical address for the p value
 *  @param gRmodP_a            RO:  physical address for the result gRmodP (a)
 *  @param RmodP_a             RO:  physical address for the result gRmodP (a)
 *  @param cp_a                RO:  physical address for the result cp
 *  @param modulusLength       RO:  length of p in bytes
 *  @param cmdBuf_p            RW:  pointer to command buffer to be generated
 * @par Externals
 *    None
 *
 * @return
 *    Status code
 *
 * @par Errors
 *    TBD
 *
 * @par Assumptions
 *    All result areas are of sufficient size.
 *****************************************************************************/
N8_Status_t cb_precomputeDHValues(const API_Request_t *req_p,
                                  const uint32_t g_a,
                                  const uint32_t p_a,
                                  const uint32_t RmodP_a,
                                  const uint32_t gRmodP_a,
                                  const uint32_t cp_a,
                                  const unsigned int modulusLengthBytes,
                                  PK_CMD_BLOCK_t *cmdBuf_p,
                                  const N8_Unit_t unitID)
{
   N8_Status_t  ret;
   unsigned int index;
   PK_CMD_BLOCK_t       *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t  *ldst_wr_ptr;
   unsigned int slot[4];
   unsigned int i;
   uint32_t keyDigits = BYTES_TO_PKDIGITS(modulusLengthBytes);

   PK_CMD_BLOCK_t    *nextCommandBlock = NULL;
   do
   {
      /* Initialize the slot values.  These are to address temporary 
         storage in the BNC.  The slots accomodate operand sizes up to
         the key length */
      for (i = 0; i < sizeof(slot)/sizeof(unsigned int); i++)
      {
         slot[i] = i * keyDigits;
      }

      /* 1) compute cp */
      index = 0;
      ret = cb_computeCX(req_p, p_a, cp_a, modulusLengthBytes, &cmdBuf_p[index], &nextCommandBlock,
                         unitID,
			 N8_FALSE);
      CHECK_RETURN(ret);

      
      /* 2) compute R mod P */
      index += N8_CB_COMPUTE_CX_NUMCMDS;
      ret = cb_computeRmodX(req_p, p_a, RmodP_a, modulusLengthBytes, &cmdBuf_p[index], &nextCommandBlock, FALSE);
      CHECK_RETURN(ret);

      /* 3) compute g R mod P */
      index += N8_CB_COMPUTE_RMODX_NUMCMDS;

      /* 3.1) Construct a command to load R mod p to slot 1 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) &cmdBuf_p[index];
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[1];
      ldst_wr_ptr->data_addr_ls = RmodP_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 3.2) Construct a command to load g to slot 2 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[2];
      ldst_wr_ptr->data_addr_ls = g_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 3.3) Construct a command to load p to slot 3 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[3];
      ldst_wr_ptr->data_addr_ls = p_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 3.4) Construct a command for the operation (g * Rmodp) mod p and place in
       * slot 0 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = slot[0];
      math_wr_ptr->m_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[3]; 
      math_wr_ptr->a_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[1];
      math_wr_ptr->b_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[2];

      /* 3.5) Construct a command to store the result */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = slot[0];
      ldst_wr_ptr->data_addr_ls = gRmodP_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      DBG_PRINT_PK_CMD_BLOCKS("Precompute DH Values",
                              (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                              N8_CB_PRECOMPUTE_DHVALUES_NUMCMDS);
   } while (FALSE);

   return ret;
} /* cb_precomputeDHValues */

/*****************************************************************************
 * cb_computeGXmodp_long
 *****************************************************************************/
/** @ingroup cb_dh
 * @brief Creates the command blocks for the computation of G**X mod p.  This
 * version <b>does not</b> use the precomputes for the stored value of g.
 *
 * Creates the command blocks to compute the value for g^x mod p.  We are given
 * g, x, p, cp, and r mod p.  To perform the exponentiation we need the
 * following:
 * a = (g * (r mod p)) mod p
 * b = x
 * c = cp
 * So, the first thing we need to do is compute (g * (r mod p)) mod p given g
 * and (r mod p).  Once this is done, we simply need to construct the
 * exponentiation command.
 *
 *  @param req_p               RO:  Pointer to the API request.
 *  @param g_a                 RO:  Physical address of g.
 *  @param x_a                 RO:  Physical address of x.
 *  @param p_a                 RO:  Physical address of p.
 *  @param cp_a                RO:  Physical address of cp.
 *  @param RmodP_a             RO:  Physical address of R mod P.
 *  @param result_a            RO:  Physical address of the result buffer.
 *  @param modulusLengthBytes  RO:  Length of the modulus in bytes.
 *  @param cmdBuf_p            RW:  Pointer to command buffer to be generated
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status of command.  N8_STATUS_OK upon success.
 *
 * @par Errors
 *    N8_MALLOC
 *
 * @par Assumptions
 *    <description of assumptions><br>
 *****************************************************************************/
N8_Status_t cb_computeGXmodp_long(const API_Request_t *req_p,
                                  const uint32_t g_a,
                                  const uint32_t x_a,
                                  const uint32_t p_a,
                                  const uint32_t cp_a,
                                  const uint32_t RmodP_a,
                                  const uint32_t result_a,
                                  const unsigned int modulusLengthBytes,
                                  PK_CMD_BLOCK_t *cmdBuf_p)
{
   PK_CMD_BLOCK_t         *math_wr_ptr = NULL;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr = NULL;

   unsigned int i;
   unsigned int slot[5];
   uint32_t keyDigits = BYTES_TO_PKDIGITS(modulusLengthBytes);

   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      CHECK_OBJECT(req_p, ret);

      /* Initialize the slot values.  These are to address temporary 
         storage in the BNC.  The slots accomodate operand sizes up to
         the key length */
      for (i = 0; i < sizeof(slot)/sizeof(int); i++)
      {
         slot[i] = i * keyDigits;
      }

      /* Compute g R mod p */
      /* 1) Construct a command to load R mod p to slot 1 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[1];
      ldst_wr_ptr->data_addr_ls = RmodP_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 2) Construct a command to load g to slot 2 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[2];
      ldst_wr_ptr->data_addr_ls = g_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 3) Construct a command to load p to slot 3 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[3];
      ldst_wr_ptr->data_addr_ls = p_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 4) Construct a command for the operation (g * Rmodp) mod p and place in
       * slot 0 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = slot[0];
      math_wr_ptr->m_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[3]; 
      math_wr_ptr->a_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[1];
      math_wr_ptr->b_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[2];


      /* 5) Construct a command to load x to slot 1 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[1];
      ldst_wr_ptr->data_addr_ls = x_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;
      
      /* 6) Construct a command to load cp to slot 2 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[2];
      ldst_wr_ptr->data_addr_ls = cp_a;
      ldst_wr_ptr->data_length = PK_DH_CP_Byte_Length;

      /* 7) Construct a command for the operation g^x mod p and leave in 
       * slot 4 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
      math_wr_ptr->r_offset = slot[4];
      math_wr_ptr->m_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[3];
      math_wr_ptr->a_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[0];
      math_wr_ptr->b_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[1];
      math_wr_ptr->c_offset = slot[2];

      /* 8) Construct a command to store the result */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = slot[4];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      DBG_PRINT_PK_CMD_BLOCKS("Compute GX mod p (long)\n",
                              (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                              N8_CB_COMPUTE_G_XMODP_NUMCMDS_LONG);
   } while (FALSE);
 
   return ret;
} /* compute GXmodp (long) */

/*****************************************************************************
 * cb_computeGXmodp_short
 *****************************************************************************/
/** @ingroup cb_dh
 * @brief Creates the command blocks for the computation of G**X mod p.  This
 * version uses the precomputed value of gRmodp for simplification and
 * acceleration. 
 *
 * Creates the command blocks to compute the value for g^x mod p.  We are given
 * x, p, cp, and g R mod p.  To perform the exponentiation we need the
 * following:
 * a = (g * (r mod p)) mod p
 * b = x
 * c = cp
 * Since we are using the precomputed value for g R mod p, we simply need to
 * load it and the other values and then call the exponentiation routine.
 *
 *  @param req_p               RO:  Pointer to the API request.
 *  @param x_a                 RO:  Physical address of x.
 *  @param p_a                 RO:  Physical address of p.
 *  @param cp_a                RO:  Physical address of cp.
 *  @param gRmodP_a            RO:  Physical address of g R mod P.
 *  @param result_a            RO:  Physical address of the result buffer.
 *  @param modulusLengthBytes  RO:  Length of the modulus in bytes.
 *  @param cmdBuf_p            RW:  Pointer to command buffer to be generated
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status of command.  N8_STATUS_OK upon success.
 *
 * @par Errors
 *    N8_MALLOC
 *
 * @par Assumptions
 *    <description of assumptions><br>
 *****************************************************************************/
N8_Status_t cb_computeGXmodp_short(const API_Request_t *req_p,
                                   const uint32_t x_a,
                                   const uint32_t p_a,
                                   const uint32_t cp_a,
                                   const uint32_t gRmodP_a,
                                   const uint32_t result_a,
                                   const unsigned int modulusLengthBytes,
                                   PK_CMD_BLOCK_t *cmdBuf_p)
{
   PK_CMD_BLOCK_t         *math_wr_ptr = NULL;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr = NULL;

   unsigned int i;
   unsigned int slot[5];
   uint32_t keyDigits = BYTES_TO_PKDIGITS(modulusLengthBytes);

   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      CHECK_OBJECT(req_p, ret);

      /* Initialize the slot values.  These are to address temporary 
         storage in the BNC.  The slots accomodate operand sizes up to
         the key length */
      for (i = 0; i < sizeof(slot)/sizeof(int); i++)
      {
         slot[i] = i * keyDigits;
      }

      /* 1) Load the precomputed g R mod p into slot 0 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[0];
      ldst_wr_ptr->data_addr_ls = gRmodP_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 2) Construct a command to load x to slot 1 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[1];
      ldst_wr_ptr->data_addr_ls = x_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;
      
      /* 3) Construct a command to load cp to slot 2 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[2];
      ldst_wr_ptr->data_addr_ls = cp_a;
      ldst_wr_ptr->data_length = PK_DH_CP_Byte_Length;

      /* 4) Construct a command to load p to slot 3 */
      ldst_wr_ptr++;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot[3];
      ldst_wr_ptr->data_addr_ls = p_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      /* 5) Construct a command for the operation g^x mod p and leave in 
       * slot 4 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
      math_wr_ptr->r_offset = slot[4];
      math_wr_ptr->m_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[3];
      math_wr_ptr->a_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[0];
      math_wr_ptr->b_length_offset = (keyDigits << PK_Cmd_Length_Shift) | slot[1];
      math_wr_ptr->c_offset = slot[2];

      /* 6) Construct a command to store the result */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = slot[4];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = modulusLengthBytes;

      DBG_PRINT_PK_CMD_BLOCKS("Compute GX mod p (short)\n",
                              (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                              N8_CB_COMPUTE_G_XMODP_NUMCMDS_SHORT);
   } while (FALSE);
 
   return ret;
} /* compute GXmodp (short) */

