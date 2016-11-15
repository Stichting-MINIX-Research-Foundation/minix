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

static char const n8_id[] = "$Id: n8_cb_dsa.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_cb_dsa.c
 *  @brief DSA Command Block Generator
 *
 * Generates all command blocks for DSA-related functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 09/10/02 brr   Set command complete bit on last command block.
 * 02/20/02 brr   Removed references to the queue structure.
 * 11/12/01 hml   Fixed some debugging code.
 * 10/31/01 bac   Added support for SKS.
 * 09/14/01 bac   Use new DBG_PRINT_PK_CMD_BLOCKS macros.
 * 08/24/01 bac   Changed all methods to accept the pre-allocated command
 *                buffer. 
 * 07/30/01 bac   Pass queue pointer to all length calculation macros.
 * 07/12/01 bac   Removed unused variables.
 * 07/13/01 mel   Fixed bug in cb_dsaVerify:
 *                command loaded wrong number of gR_mod_p bytes.
 * 06/28/01 mel   Fixed bug (kernel memory usage).
 *                Added comments.
 * 06/28/01 bac   Minor typo corrections.
 * 06/26/01 bac   Even more on conversion to use physical memory.
 * 06/25/01 bac   More on conversion to use physical memory.
 * 06/20/01 mel   Corrected use of kernel memory.
 * 05/22/01 mel   Original version.
 ****************************************************************************/
/** @defgroup cb_dsa DSA Command Block Generator
 */

#include "n8_common.h"
#include "n8_pk_common.h"
#include "n8_pub_errors.h"
#include "n8_cb_dsa.h"
#include "n8_util.h"

/* #define ZERO_CMD_BLOCK(X) memset((X), 0, sizeof(PK_CMD_BLOCK_t)) */
#define ZERO_CMD_BLOCK(X)

/*****************************************************************************
 * cb_computeGRmodX
 *****************************************************************************/
/** @ingroup cb_dsa
 * @brief  Creates the command blocks to compute the value for gR mod X
 *
 *
 *  @param req_p                    RW:  Pointer to command
 *                                       blocks.  
 *  @param key                      RO:  pointer to phisical addresses of DSAObject.
 *  @param modulusDigits            RO:  number of digits in modulus p.
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_INVALID_OBJECT   -   command block pointer is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/

N8_Status_t cb_computeGRmodX(API_Request_t *req_p,
                             const int modulusDigits,
                             uint32_t g_a,
                             uint32_t X_a,
                             uint32_t res_a,
                             PK_CMD_BLOCK_t *cmdBuf_p,
                             PK_CMD_BLOCK_t **next_cmdBuf_p)
{
   PK_CMD_BLOCK_t         *math_wr_ptr = NULL;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr = NULL;

   uint32_t             slot0, slot1, slot2, slot3;

   N8_Status_t ret = N8_STATUS_OK;

   do
   {
      CHECK_OBJECT(req_p, ret);

      /* Initialize the slot values.  These are to address temporary 
         storage in the BNC.  The slots accomodate operand sizes up to
         the key length */
      slot0 = 0;
      slot1 = modulusDigits;
      slot2 = modulusDigits * 2;
      slot3 = modulusDigits * 3;

      /* Compute gR mod p */
      /* 1) Construct a command to load p */
      /*
       * slot0    slot1   slot2   slot3 
       *            p
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cmdBuf_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot1;
      ldst_wr_ptr->data_addr_ls = X_a;
      ldst_wr_ptr->data_length = PK_DSA_P_Byte_Length(modulusDigits);

      /* 2) Construct a command to load g */
      /*
       * slot0    slot1   slot2   slot3 
       *            p       g
       */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot2;
      ldst_wr_ptr->data_addr_ls = g_a;
      ldst_wr_ptr->data_length = PK_DSA_G_Byte_Length(modulusDigits);

      /* 3) Construct a command for the operation R mod p */
      /*
       * slot0    slot1   slot2   slot3 
       *            p       g      R mod p
       */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_R_Mod_M;
      math_wr_ptr->r_offset = slot3;
      math_wr_ptr->m_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot1;

      /* 4) Construct a command for the operation g * Rmodp mod p */
      /*
       * slot0    slot1   slot2   slot3 
       * gRmodp     p       g      R mod p
       */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (math_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = slot0;
      math_wr_ptr->m_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot1;
      math_wr_ptr->a_length_offset = (PK_DSA_G_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot2;
      math_wr_ptr->b_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot3;

      /* 5) Construct a command to store gR mod p  */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R;
      ldst_wr_ptr->r_offset = slot0;
      ldst_wr_ptr->data_addr_ls = res_a;
      ldst_wr_ptr->data_length = PK_DSA_GR_MOD_P_Byte_Length(modulusDigits);

      /* save next address for future use */
      *next_cmdBuf_p = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);

      DBG(("Compute gR mod X\n"));
/*      DBG_PRINT_CMD_BLOCKS("Compute gR mod X",
                           (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                           N8_CB_COMPUTE_GRMODX_NUMCMDS);  */
   } while (FALSE);
 
   return ret;
} /* computeGRmodX */

/*****************************************************************************
 * cb_dsaSign
 *****************************************************************************/
/** @ingroup cb_dsa
 * @brief  Creates the command blocks to compute DSA signature
 *
 *
 *  @param req_p        RW:  Pointer to command
 *                           blocks.  
 *  @param key          RO:  The previously initialized DSAKeyObject
 *                           containing the DSA key
 *                           materials to be used.
 *  @param n_a          RO:  Physical address of random number n.
 *  @param paramBlock_a RO:  Physical address of parameters block.
 *  @param msgHash_a    RO:  Physical address of hash.
 *  @param rValue_a     RO:  Physical address of r value.
 *  @param sValue_a     RO:  Physical address of s value.
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_INVALID_OBJECT   -   command block pointer is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_dsaSign(API_Request_t *req_p,
                       const N8_DSAKeyObject_t *key_p,
                       uint32_t n_a,
                       uint32_t paramBlock_a,
                       uint32_t msgHash_a,
                       uint32_t rValue_a,
                       uint32_t sValue_a,
                       PK_CMD_BLOCK_t *cmdBuf_p)
{
   PK_RSA_CMD_BLOCK_t     *math_wr_ptr = NULL;
   PK_LDST_CMD_BLOCK_t    *ldst_wr_ptr = NULL;
   uint32_t                modulusDigits;
   uint32_t                sks_word;
   N8_Status_t             ret = N8_STATUS_OK;

   do
   {
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(key_p, ret);

      modulusDigits = BYTES_TO_PKDIGITS(key_p->modulusLength);

      DBG(("constructing sign command blocks\n"));

      /*  1) Construct a command to load random number n */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cmdBuf_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = PK_DSA_N_BNC_Offset;
      ldst_wr_ptr->data_addr_ls = n_a;
      ldst_wr_ptr->data_length = PK_DSA_N_Byte_Length;

      /* 2) Construct a command to load hash e1 */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = PK_DSA_E1_BNC_Offset;
      ldst_wr_ptr->data_addr_ls = msgHash_a;
      ldst_wr_ptr->data_length = PK_DSA_E1_Byte_Length;

      if (key_p->keyType == N8_PRIVATE_SKS)
      {
         /* we are using the sks.  set the sks_word variable to the correct
          * offset.  We do not need to load the DSA parameter block as it is
          * already in the SKS. */
         sks_word = key_p->SKSKeyHandle.sks_offset;
      }
      else
      {
         /* 3) Construct a command to load the DSA parameter block */
         ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
         ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
         ldst_wr_ptr->r_offset = PK_DSA_P_BNC_Offset;
         ldst_wr_ptr->data_addr_ls = paramBlock_a;
         ldst_wr_ptr->data_length = PK_DSA_Param_Byte_Length(modulusDigits);

         sks_word = PK_Cmd_N_Mask;
      }

      /* 4) Construct a command for the DSA operation */
      math_wr_ptr = (PK_RSA_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_DSA_Sign_Op;
      math_wr_ptr->sks = sks_word | (modulusDigits << PK_Cmd_Key_Length_Shift);
      
      /* 5) Construct a command to store r */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R;
      ldst_wr_ptr->r_offset = PK_DSA_R_BNC_Offset;
      ldst_wr_ptr->data_addr_ls = rValue_a;
      ldst_wr_ptr->data_length = PK_DSA_R_Byte_Length;

      /* 6) Construct a command to store s */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = PK_DSA_S_BNC_Offset;
      ldst_wr_ptr->data_addr_ls = sValue_a;
      ldst_wr_ptr->data_length = PK_DSA_S_Byte_Length;
      

      DBG_PRINT_PK_CMD_BLOCKS("DSA sign",
                              req_p->PK_CommandBlock_ptr,
                              N8_CB_DSA_SIGN_NUMCMDS(key_p));
   } while (FALSE);
   return ret;
}

/*****************************************************************************
 * cb_dsaVerify
 *****************************************************************************/
/** @ingroup cb_dsa
 * @brief  Creates the command blocks to perform DSA verify operation
 *
 *                   1) Compute e = e1 mod q
 *                   2) Compute invs = s^1 mod q
 *                   3) Compute u1 = invs * e mod q
 *                   4) Compute u2 = invs * r mod q
 *                   5) Compute v1 = g^u1 mod p
 *                   6) Compute w = y^u2 mod p
 *                   7) Compute v3 = v1 * w mod p
 *                   8) Compute v = v3 mod q
 *
 *
 *                         Big Number Cache slot allocation
 *
 *               1       2       3       4       5       6       7       8
 *   -------------------------------------------------------------------------
 *   step 1)     q               e1
 *               .                       e
 *   step 2)     .               s
 *               .      invs
 *   step 3)     .       .               u1
 *   step 4)     .       .       r       .
 *               .       .       u2      .
 *   step 5)     .      p[0]     .       .     2^128
 *               .      p[0]^-1  .       .       .
 *               .     -p[0]^-1  .       .       .
 *               .       .       .       .       p                       g
 *               .       .       .       .       .             Rmodp     .
 *               .       .       .       .       .    gRmodp             .
 *               .       .       .       .       .              v1        
 *   step 6)     .       .       .               .     Rmodp     .       y
 *               .       .       .               .    yRmodp     .
 *               .       .       .               .               .       w  
 *   step 7)     .                               .              v3
 *   step 8)     v
 *
 *
 *
 *  @param req_p        RW:  Pointer to command
 *                           blocks.  
 *  @param key          RO:  The previously initialized DSAKeyObject
 *                           containing the DSA key
 *                           materials to be used.
 *  @param q_a          RO:  Physical address of q value.
 *  @param cp_a         RO:  Physical address of computed cp.
 *  @param gR_mod_p_a   RO:  Physical address of computed gRmodp.
 *  @param p_a          RO:  Physical address of p value.
 *  @param publicKey_a  RO:  Physical address of public key.
 *  @param mh_a         RO:  Physical address of hash.
 *  @param r_a          RO:  Physical address of r value.
 *  @param s_a          RO:  Physical address of s value.
 *  @param res_a        WO:  Physical address of result.
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors
 *          N8_INVALID_OBJECT   -   command block pointer is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None.<br>
 *****************************************************************************/
N8_Status_t cb_dsaVerify(API_Request_t *req_p,
                         const N8_DSAKeyObject_t *key_p,
                         uint32_t q_a, 
                         uint32_t cp_a, 
                         uint32_t gR_mod_p_a, 
                         uint32_t p_a, 
                         uint32_t publicKey_a, 
                         uint32_t mh_a,
                         uint32_t r_a,
                         uint32_t s_a,
                         uint32_t res_a,
                         PK_CMD_BLOCK_t *cmdBuf_p)
{
   PK_CMD_BLOCK_t       *math_wr_ptr = NULL;
   PK_LDST_CMD_BLOCK_t  *ldst_wr_ptr = NULL;

   uint32_t             modulusDigits;
   uint32_t             slot1, slot2, slot3, slot4; 
   uint32_t             slot5, slot6, slot7, slot8;

   N8_Status_t          ret = N8_STATUS_OK;

   do
   {
      CHECK_OBJECT(req_p, ret);
      
      modulusDigits = BYTES_TO_PKDIGITS(key_p->modulusLength);
      
      /* Initialize the slot values.  These are to address temporary 
         storage in the BNC.  Slots 1-4 hold 2-digit operands. 
         Slots 5-8 hold operands up to the key size in length. */
      slot1 = 0;                                               /* q */
      slot2 = slot1 + PK_DSA_N_BNC_Length;  /* invs */
      slot3 = slot2 + PK_DSA_N_BNC_Length;  /* e1 */
      slot4 = slot3 + PK_DSA_N_BNC_Length;  /*  */
      slot5 = slot4 + modulusDigits;
      slot6 = slot5 + modulusDigits;
      slot7 = slot6 + modulusDigits;
      slot8 = slot7 + modulusDigits;

      /*
       Compute e = e1 mod q 
       */
      /* 1) Construct a command to load q */
      /*      1       2       3       4       5       6       7       8 
              
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) cmdBuf_p;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot1;
      ldst_wr_ptr->data_addr_ls = (unsigned int) q_a;
      ldst_wr_ptr->data_length = PK_DSA_Q_Byte_Length;


      /* 2)Construct a command to load e1 */
      /*      1       2       3       4       5       6       7       8 
              q             
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot3;
      ldst_wr_ptr->data_addr_ls = mh_a;
      ldst_wr_ptr->data_length = PK_DSA_E1_Byte_Length;

      /* 3) Construct a command for the operation e1 mod q */
      /*      1       2       3       4       5       6       7       8 
              q              e1    
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_A_Mod_M;
      math_wr_ptr->r_offset = slot4;
      math_wr_ptr->m_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot1;
      math_wr_ptr->a_length_offset = (PK_DSA_E1_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot3;
   
      /* Compute invs = s^-1 mod q */
      /* 4) Construct a command to load s */
      /*      1       2       3       4       5       6       7       8 
              q               e1   e1 mod q
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) math_wr_ptr + 1;
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot3;
      ldst_wr_ptr->data_addr_ls = s_a;
      ldst_wr_ptr->data_length = PK_DSA_S_Byte_Length;
   
      /* 5) Construct a command for the operation invs = s^-1 mod q */
      /*      1       2       3       4       5       6       7       8 
              q               s    e1 mod q
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Inverse_A_Mod_M;
      math_wr_ptr->r_offset = slot2;
      math_wr_ptr->m_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot1;
      math_wr_ptr->a_length_offset = (PK_DSA_S_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot3;
   
      /* Compute u1 = invs * e mod q */
      /* 6) Construct a command for the operation u1 = invs * e mod q */
      /*      1       2       3       4       5       6       7       8 
              q      invs     s    e1 mod q
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (math_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = slot4;
      math_wr_ptr->m_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot1;
      math_wr_ptr->a_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot2;
      math_wr_ptr->b_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot4;
   
      /* Compute u2 = invs * r mod q */
      /* 7) Construct a command to load r */
      /*      1       2       3       4       5       6       7       8 
              q      invs     s       u1
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot3;
      ldst_wr_ptr->data_addr_ls = r_a;
      ldst_wr_ptr->data_length = PK_DSA_R_Byte_Length;
   
      /* 8) Construct a command for the operation u2 = invs * r mod q */
      /*      1       2       3       4       5       6       7       8 
              q      invs     r       u1
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = slot3;
      math_wr_ptr->m_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot1;
      math_wr_ptr->a_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot2;
      math_wr_ptr->b_length_offset = (PK_DSA_R_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot3;

      /* Compute v1 = g^u1 mod p */
      
      /* 9) Construct a command to load cp = -t mod 2^128 */
      /*      1       2       3       4       5       6       7       8 
              q      invs     u2      u1
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot2;
      ldst_wr_ptr->data_addr_ls = (unsigned int) cp_a;
      ldst_wr_ptr->data_length = PK_DSA_CP_Byte_Length;   
   
      /* 10) Construct a command to load g * R mod p */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot6;
      ldst_wr_ptr->data_addr_ls = gR_mod_p_a;
      ldst_wr_ptr->data_length = PK_DSA_GR_MOD_P_Byte_Length(modulusDigits);  
   
      /* 11) Construct a command to load p */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1           gR_mod_p
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot5;
      ldst_wr_ptr->data_addr_ls = (unsigned int) p_a;
      ldst_wr_ptr->data_length = PK_DSA_P_Byte_Length(modulusDigits);
   
      /* 12) Construct a command for the operation v1 = g^u1 mod p */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1      p     gR_mod_p
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
      math_wr_ptr->r_offset = slot7;
      math_wr_ptr->m_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot5;
      math_wr_ptr->a_length_offset = (PK_DSA_GR_MOD_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot6;
      math_wr_ptr->b_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot4;
      math_wr_ptr->c_offset = slot2;

   
      /* Compute w = y^u2 mod p */
      /* 13) Construct a command to load y */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1      p     gR_mod_p  v1
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Load_R;
      ldst_wr_ptr->r_offset = slot8;
      ldst_wr_ptr->data_addr_ls = (unsigned int) publicKey_a;
      ldst_wr_ptr->data_length = PK_DSA_Y_Byte_Length(modulusDigits); 

      /* 14) Construct a command for the operation R mod p */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1      p     gR_mod_p  v1    publicKey
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (ldst_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_R_Mod_M;
      math_wr_ptr->r_offset = slot6;
      math_wr_ptr->m_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot5;
   
      /* 15) Construct a command for the operation yR mod p */
      /*      1       2       3       4       5       6      7       8 
              q      cp       u2      u1      p     R_mod_p  v1    publicKey(y)
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (math_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = slot6;
      math_wr_ptr->m_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot5;
      math_wr_ptr->a_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot8;
      math_wr_ptr->b_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot6;
   
      /* 16) Construct a command for the operation w = y^u2 mod p */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1      p     yR_mod_p  v1    publicKey(y)
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (math_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_Exp_G_Mod_M;
      math_wr_ptr->r_offset = slot8;
      math_wr_ptr->m_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot5;
      math_wr_ptr->a_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot6;
      math_wr_ptr->b_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot3;
      math_wr_ptr->c_offset = slot2; 
   
      /* Compute v3 = v1 * w mod p */
      /* 17) Construct a command for the operation v3 = v1 * w mod p */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1      p     yR_mod_p  v1      w
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (math_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_AB_Mod_M;
      math_wr_ptr->r_offset = slot7;
      math_wr_ptr->m_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot5;
      math_wr_ptr->a_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot7;
      math_wr_ptr->b_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot8;
   
      /* Compute v = v3 mod q */
      /* 18) Construct a command for the operation v = v3 mod q */
      /*      1       2       3       4       5       6       7       8 
              q      cp       u2      u1      p     yR_mod_p  v3      w
      */
      math_wr_ptr = (PK_CMD_BLOCK_t *) (math_wr_ptr + 1);
      math_wr_ptr->opcode_si = PK_Cmd_A_Mod_M;
      math_wr_ptr->r_offset = slot1;
      math_wr_ptr->m_length_offset = (PK_DSA_Q_BNC_Length << 
                                      PK_Cmd_Length_Shift) | slot1;
      math_wr_ptr->a_length_offset = (PK_DSA_P_BNC_Length(modulusDigits) << 
                                      PK_Cmd_Length_Shift) | slot7;
   
      /* 19) Construct a command to store v  */
      /*      1       2       3       4       5       6       7       8 
              v      cp       u2      u1      p     yR_mod_p  v3      w
      */
      ldst_wr_ptr = (PK_LDST_CMD_BLOCK_t *) (math_wr_ptr + 1);
      ldst_wr_ptr->opcode_si = PK_Cmd_Store_R | PK_Cmd_SI_Mask;
      ldst_wr_ptr->r_offset = slot1;
      ldst_wr_ptr->data_addr_ls = res_a;
      ldst_wr_ptr->data_length = PK_DSA_Q_Byte_Length;
   

      DBG_PRINT_PK_CMD_BLOCKS("DSA verify",
                              (PK_CMD_BLOCK_t *) req_p->PK_CommandBlock_ptr,
                              N8_CB_DSA_VERIFY_NUMCMDS);
   } while (FALSE);

   return ret;

} /* cb_dsaVerify */

/*****************************************************************************
 * cb_DSASignOperations
 *****************************************************************************/
/** @ingroup cb_rsa
 * @brief Calculate the number of operations required for an DSA Sign.
 *
 * The operations used to perform an DSA Sign depend upon the key type.  If the
 * type is N8_PRIVATE_SKS, then the DSA Sign Operation will use the parameter
 * block taken from the SKS, saving a load.  If it is not SKS then the parameter
 * block must be manually loaded and used.
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
unsigned int cb_DSASignOperations(const N8_DSAKeyObject_t *key_p)
{
   unsigned int ret = 0;
   const int NOT_USING_SKS_NUM_COMMANDS = 6;

   if (key_p->keyType == N8_PRIVATE_SKS)
   {
      ret = NOT_USING_SKS_NUM_COMMANDS - 1;
   }
   else
   {
      ret = NOT_USING_SKS_NUM_COMMANDS;
   }
   return ret;
} /* cb_RSADecryptOperations */

