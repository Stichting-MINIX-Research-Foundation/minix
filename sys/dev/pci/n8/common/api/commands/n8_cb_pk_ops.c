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

static char const n8_id[] = "$Id: n8_cb_pk_ops.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_cb_pk_ops.c
 *  @brief PKP Operations
 *
 * Command block generation for individual PKP operations
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 09/10/02 brr   Set command complete bit on last command block.
 * 05/20/02 bac   Corrected calculation of number of commands generated.
 * 05/07/02 bac   Original version.
 ****************************************************************************/
/** @defgroup cb_pkp PKP Operations
 */

#include "n8_cb_pk_ops.h"
#include "n8_pk_common.h"
#include "n8_util.h"

/*****************************************************************************
 * cb_pk_op
 *****************************************************************************/
/** @ingroup cb_pkp
 * @brief Perform generic PKP math for two operand operations
 *
 * For operations of the form:
 * result = R mod n, or
 * result = a mod n, or
 * result = a (op) b mod n
 *
 *  @param req_p               RW:  Pointer to API request structure
 *  @param shifted_opcode      RO:  Opcode to use, pre-shifted to the left for
 *                                  direct insertion into the command block
 *  @param a_a                 RO:  "a" operand physical address
 *  @param a_length_bytes      RO:  Length of "a"
 *  @param b_a                 RO:  "b" operand physical address
 *  @param b_length_bytes      RO:  Length of "b"
 *  @param modulus_a           RO:  modulus operand physical address
 *  @param mod_length_bytes    RO:  Length of modulus
 *  @param max_length_bytes    RO:  Max length of operands or modulus
 *  @param result_a            RW:  results buffer physical address
 *  @param cb_p                RW:  Pointer to command block
 *  @param next_cb_pp          RW:  Returned pointer to next command block.  May
 *                                  be NULL
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  N8_STATUS_OK on success
 *
 * @par Errors
 *    None
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t cb_pk_op(API_Request_t     *req_p,
                     const uint32_t     shifted_opcode,
                     const uint32_t     a_a,
                     const unsigned int a_length_bytes,
                     const uint32_t     b_a,
                     const unsigned int b_length_bytes,
                     const uint32_t     modulus_a,
                     const unsigned int mod_length_bytes,
                     const unsigned int max_length_bytes,
                     const uint32_t     result_a,
                     PK_CMD_BLOCK_t    *cb_p,
                     PK_CMD_BLOCK_t   **next_cb_pp)
{
   N8_Status_t ret = N8_STATUS_OK;
   PK_CMD_BLOCK_t         *math_wr_ptr;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr;
   uint32_t max_length_digits = BYTES_TO_PKDIGITS(max_length_bytes);
   uint32_t mod_length_digits = BYTES_TO_PKDIGITS(mod_length_bytes);
   uint32_t a_length_digits   = BYTES_TO_PKDIGITS(a_length_bytes);
   uint32_t b_length_digits   = BYTES_TO_PKDIGITS(b_length_bytes);
   uint32_t offset[4];
   unsigned int i;
   unsigned int numCommands = N8_CB_PK_OP_NUMCMDS; /* set to maximum and
                                                    * decrement if commands are
                                                    * skipped.  */
   do
   {
      for (i = 0; i < sizeof(offset)/sizeof(uint32_t); i++)
      {
         offset[i] = i * max_length_digits;
      } /* for i */

      /* initialize the load/store pointer to the begining of the
       * command block */ 
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cb_p;
      
      /* load a to 0, if non-zero*/
      if (a_length_bytes != 0)
      {
         ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
         ldst_wr_ptr->r_offset = offset[0];
         ldst_wr_ptr->data_addr_ls = a_a; 
         ldst_wr_ptr->data_length = a_length_bytes;
         ldst_wr_ptr++;
      }
      else
      {
         numCommands--;
      }
      
      /* load b to 1*/
      if (b_length_bytes != 0)
      {
         ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
         ldst_wr_ptr->r_offset = offset[1];
         ldst_wr_ptr->data_addr_ls = b_a; 
         ldst_wr_ptr->data_length = b_length_bytes;
         ldst_wr_ptr++;
      }
      else
      {
         numCommands--;
      }

      /* load modulus to 2 */
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = offset[2];
      ldst_wr_ptr->data_addr_ls = modulus_a; 
      ldst_wr_ptr->data_length = mod_length_bytes;

      /* perform operation with results going to 3 */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = shifted_opcode;
      math_wr_ptr->a_length_offset = 
         (a_length_digits << PK_Cmd_Length_Shift) | offset[0];
      math_wr_ptr->b_length_offset = 
         (b_length_digits << PK_Cmd_Length_Shift) | offset[1];
      math_wr_ptr->m_length_offset = 
         (mod_length_digits << PK_Cmd_Length_Shift) | offset[2]; 
      math_wr_ptr->r_offset = offset[3];

      /* store results */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = offset[3];
      ldst_wr_ptr->data_addr_ls = result_a;
      ldst_wr_ptr->data_length = mod_length_bytes;

      /* save next address for future use */
      if (next_cb_pp != NULL)
      {
         *next_cb_pp = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      }

      DBG_PRINT_PK_CMD_BLOCKS("pk operation",
                              cb_p,
                              numCommands);
      
   } while (N8_FALSE);

   return ret;
} /* cb_pk_op */
