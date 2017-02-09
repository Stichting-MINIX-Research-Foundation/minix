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

static char const n8_id[] = "$Id: n8_cb_ea.c,v 1.1 2008/10/30 12:02:15 darran Exp $";
/*****************************************************************************/
/** @file n8_cb_ea.c
 *  @brief Command blocks for EA.
 *
 * Generate command blocks for the Encryption Authentication functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:  
 * 08/18/03 brr   Combine Encrypt/Decrypt command block generators for SSL,
 *                TLS, & IPsec.
 * 07/01/03 brr   Support N8_HASH_NONE for IPsec.
 * 06/06/03 brr   Remove uneeded includes.
 * 09/10/02 brr   Set command complete bit on last command block.
 * 06/12/02 bac   Use symbol N8_ARC4_MAX_LENGTH instead of hard-coded
 *                constant. (Bug #768) 
 * 03/26/02 brr   Allocate the data buffer as part of the API request. 
 * 02/22/02 spm   Converted printf's to DBG's.
 * 02/19/02 brr   Removed byteSwapContext. (only required for FPGA support)
 * 01/22/02 bac   Changes to load contexts so they may be deferred until the
 *                first use rather than at initialization.
 * 01/12/02 bac   Simplified calls which used various ipad/opad/iv values.
 *                Introduced n8_IVSrc to specify where to get the IV for a
 *                command.  Reworked 'convertToBits' to update the correct
 *                counter. 
 * 12/06/01 bac   Fixed NSP2000 Bug #10 -- use EA_Ctx_Addr_Address_Mask
 *                when dealing with context indices.
 * 11/28/01 mel   Fixed bug #365 : ARC4 key type N8_RC4_t incorrectly declared 
 * 11/24/01 brr   Removed include of obsolete EA & PK specifice Queue files.
 * 11/09/01 mel   Fixed bug #253 : N8_HashCompleteMessage not compute HMAC 
 *                hashes correctly - added HMAC opcode to command block 
 * 11/08/01 mel   Fixed bug #289 : Some calls use N8_SINGLE_CHIP in the 
 *                commands directory
 * 10/30/01 bac   Added some debugging, updates to spell IPsec correctly, made
 *                nextCommandBlock_p optional.
 * 10/22/01 mel   Added cb_ea_hashHMACEnd command
 * 10/16/01 mel   Added 2 hashend commands to cb_ea_SSLHandshakeHash
 * 10/16/01 spm   IKE APIs: removed key physical addr parms
 * 10/15/01 spm   IKE APIs: changed if-else on alg to switch on alg.  Removed
 *                virtual pointer to message.  Had to keep virtual pointer
 *                to key, since the key needs to be copied into the command
 *                block itself.  Fixed bug, where 64 byte (exactly)key
 *                doesn't get copied.
 * 10/15/01 bac   Changed some interfaces to take unsigned ints, corrected a bug
 *                when DBG was used.
 * 10/12/01 mel   Added the command block cb_ea_SSLHandshakeHash.
 * 10/11/01 hml   Removed an errant 'i' to fix a compiler warning.
 * 10/11/01 hml   Added the command block generators for N8_HashCompleteMessage
 *                and the TLS modes of the N8_HandshakeHashEnd.
 * 10/08/01 bac   Fixed a bug in calling DBG_PRINT_EA_COMMAND_BLOCKS. A pointer
 *                which had been incremented was being passed.
 * 09/21/01 bac   Rearranged cb_ea_TLSKeyMaterialHash to not require an extra
 *                allocated command block.
 * 09/21/01 bac   Corrected signature on cb_ea_encrypt to take physical
 *                addresses. 
 * 09/20/01 bac   The interface to the command block generators changed and now
 *                accept the command block buffer.  
 * 09/18/01 bac   Massive changes to support model where the caller allocates
 *                the command buffer.  Lots of reorganization and renaming to be
 *                more standard.
 * 09/17/01 spm   Truncated lines >80 chars.
 * 09/14/01 bac   Use new DBG_PRINT_PK_CMD_BLOCKS macros.
 * 09/07/01 spm   Added CB support for IKE APIs.
 * 08/27/01 mel   Added Write/Read context command blocks.
 * 08/10/01 bac   In the FPGA, the context memory is read in the same manner as
 *                the command blocks.  Therefore, under our current FPGA
 *                implementation the contexts must always be endian byte-swapped
 *                before passing to the FPGA.  This fix was put in place.
 * 07/02/01 mel   Fixed comments.
 * 06/25/01 bac   More on conversion to use physical memory.
 * 06/21/01 mel   Corrected use of kernel memory.
 * 06/17/01 bac   Changes per code review.
 * 06/08/01 mel   Added Cryptographic command blocks.
 * 06/05/01 bac   Changes to not rely on N8_SSLTLSPacket_t being packed (Bug
 *                #31). 
 * 05/31/01 mel   Changed keys and IVs to be a char[] instead of uint32_t[].
 * 06/19/01 bac   Correct use of kernel space memory.
 * 05/23/01 bac   macSecret is a char[] now and is converted to big endian
 *                order before putting into command block.
 * 05/22/01 bac   Changed SSL Encrypt and Decrypt commands to pass
 *                packets instead of buffers.
 * 05/21/01 bac   Converted to use N8_ContextHandle_t and N8_Packet_t
 *                with integrated cipher and hash packet.
 * 05/21/01 bac   Fixed SSLAuthenticate to insert the context index when doing
 *                RC4.
 * 05/19/01 bac   Added debugging for printing contexts.
 * 05/18/01 bac   Corrected SSLDecrypt logic for setting non-context command
 *                block parameters.  Fixed printCommandBlock to work on lil'
 *                endian machines.
 * 05/16/01 dws   Modified cb_loadARC4key_to_contextMemory to use the ARC4
 *                i and j masks and shift counts in a more general way. This
 *                was done to accomodate the change in the i and j locations
 *                in the ARC4 context block.
 * 05/11/01 bac   Merge sanity changes.  Naming standardization.
 * 05/09/01 bac   Added support for SSL Encrypt/Authenticate. Converted to use
 *                new N8_MALLOC/N8_FREE macros.
 * 05/09/01 dws   Changed the way that the random bytes are loaded into the
 *                command block in cb_ea_SSLKeyMaterialHash. It now uses a
 *                series of BE_to_uint32 operations instead of memcpy. 
 * 05/04/01 bac   Fixed some compilation problems for prematurely checked in
 *                code. 
 * 05/03/01 bac   Replaced integer use of NULL with 0.
 * 05/01/01 bac   Support for SSLKeyMaterialHash.  Also fixed some merge errors.
 * 04/24/01 bac   Support for hash partial and hash end.
 * 04/12/01 bac   Original version.
 ****************************************************************************/
/** @defgroup cb_ea EA Command Block Generator
 */

#include "n8_cb_ea.h"
#include "n8_ea_common.h"                /* for typedef of EA_CMD_BLOCK_t */
#include "n8_common.h"
#include "n8_util.h"



#define BYTES_PER_ITERATION 16
#define NUM_RANDOM_BYTES    64

static void convertToBits(EA_CMD_BLOCK_t *cb_p, const N8_HashObject_t *obj_p,
                          const n8_IVSrc_t ivSrc); 
static void n8_RC4_set_key(N8_RC4_t *key, int len, const unsigned char *data);


/*****************************************************************************
 * cb_ea_hashPartial
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief  Cmd Blocks for hash partial
 *
 * Command Block generation for EA function "Hash N Blocks,
 * Pre-Load IV from Cmd Block" (opcodes 0x12 and 0x22)
 *
 *  @param req_p               RW:  Request pointer.
 *  @param obj_p               RW:  Pointer to hash object.  
 *  @param hashMsg_a           RO:  Physical address of message to be hashed.
 *  @param msgLength           RO:  Length of message
 *  @param result_a            RO:  Pointer to allocated buffer for
 *                                  the results to be placed by the
 *                                  EA.  This value is used but not
 *                                  written by this function.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_MALLOC_FAILED - memory allocation failed<BR>
 *    N8_INVALID_HASH  - hash specified was invalid<br>
 *
 * @par Assumptions
 *    Caller is responsible to free memory allocated to cmdBlock_pp.<br>
 *****************************************************************************/
N8_Status_t cb_ea_hashPartial(API_Request_t *req_p,
                              EA_CMD_BLOCK_t *cb_p,
                              const N8_HashObject_t *obj_p,
                              const n8_IVSrc_t ivSrc,
                              const uint32_t hashMsg_a,
                              const unsigned int msgLength,
                              const uint32_t result_a,
                              EA_CMD_BLOCK_t **next_cb_pp,
			      int            lastCmdBlock)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
         
      cb_p->read_data_addr_ls =  hashMsg_a;
      cb_p->write_data_addr_ls = result_a;

      /* If this is the last command block, set the command complete bit */
      if (lastCmdBlock == N8_TRUE)
      {
         cb_p->cp_si_context = EA_Cmd_SI_Mask;
      }
      switch (ivSrc)
      {
         case N8_IPAD:
            memcpy(cb_p->hash_IV, obj_p->ipadHMAC_iv, sizeof(obj_p->iv));
            break;
         case N8_OPAD:
            memcpy(cb_p->hash_IV, obj_p->opadHMAC_iv, sizeof(obj_p->iv));
            break;
         default:
            memcpy(cb_p->hash_IV, obj_p->iv, sizeof(obj_p->iv));
            break;
      }

      /* set opcode */
      switch (obj_p->type)
      {
        case N8_MD5:
        case N8_HMAC_MD5:
        case N8_HMAC_MD5_96:
            cb_p->opcode_iter_length = EA_Cmd_MD5_Mid_cmdIV;
            break;
         case N8_SHA1:
         case N8_HMAC_SHA1:
         case N8_HMAC_SHA1_96:
            cb_p->opcode_iter_length = EA_Cmd_SHA1_Mid_cmdIV;
            break;
         default:
            ret = N8_INVALID_HASH;
            break;
      }
      CHECK_RETURN(ret);
      cb_p->opcode_iter_length |= (EA_Cmd_Data_Length_Mask & msgLength);
      
      /* save next address for future use */
      if (next_cb_pp != NULL)
      {
         *next_cb_pp = (EA_CMD_BLOCK_t *) (cb_p + 1);
      }


   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("Hash Partial", cb_p,
                           N8_CB_EA_HASHPARTIAL_NUMCMDS);
   
   return ret;
} /* cb_ea_hashPartial */


/*****************************************************************************
 * cb_ea_hashCompleteMessage
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief  Cmd Blocks for hash complete message
 *
 * Command Block generation for EA function "Simple Hash"
 * (opcodes 0x10 and 0x20)
 *
 *  @param req_p               RW:  Request pointer.
 *  @param obj_p               RW:  Pointer to hash object.  
 *  @param hashMsg_a           RO:  Physical address of message to be hashed.
 *  @param msgLength           RO:  Length of message
 *  @param result_a            RO:  Pointer to allocated buffer for
 *                                  the results to be placed by the
 *                                  EA.  This value is used but not
 *                                  written by this function.
 *
 * @par Externals
 *    None
 *
 * @return
 *    N8_STATUS_OK on success or one of the error codes specified.
 *
 * @par Errors
 *    N8_MALLOC_FAILED - memory allocation failed<BR>
 *    N8_INVALID_HASH  - hash specified was invalid
 *
 * @par Assumptions
 *    Caller is responsible to free memory allocated to cmdBlock_pp.<br>
 *****************************************************************************/
N8_Status_t cb_ea_hashCompleteMessage(API_Request_t *req_p,
                                      EA_CMD_BLOCK_t *cb_p,
                                      const N8_HashObject_t *obj_p,
                                      const uint32_t hashMsg_a,
                                      const unsigned int msgLength,
                                      const uint32_t result_a)
{
   N8_Status_t ret = N8_STATUS_OK;
   EA_HMAC_CMD_BLOCK_t *cb_HMAC_p = NULL;
   do
   {
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
         
      cb_HMAC_p = (EA_HMAC_CMD_BLOCK_t *) cb_p;

      cb_p->read_data_addr_ls =  hashMsg_a;
      cb_p->write_data_addr_ls = result_a;
      cb_p->cp_si_context = EA_Cmd_SI_Mask;

      /* set opcode */
      switch (obj_p->type)
      {
        case N8_MD5:
            cb_p->opcode_iter_length =
               EA_Cmd_MD5 | (EA_Cmd_Data_Length_Mask & msgLength);
            break;
        case N8_HMAC_MD5:
        case N8_HMAC_MD5_96:
            memcpy(cb_HMAC_p->hmac_key, obj_p->hashedHMACKey, sizeof(obj_p->hashedHMACKey));
            cb_p->opcode_iter_length =
               (EA_Cmd_MD5_HMAC | 
                ((1 << EA_Cmd_Iterations_Shift)  & EA_Cmd_Iterations_Mask) |
                (EA_Cmd_Data_Length_Mask & msgLength));
            break;
         case N8_SHA1:
            cb_p->opcode_iter_length =
               EA_Cmd_SHA1 | (EA_Cmd_Data_Length_Mask & msgLength);
            break;
      case N8_HMAC_SHA1:
      case N8_HMAC_SHA1_96:
          memcpy(cb_HMAC_p->hmac_key, obj_p->hashedHMACKey, sizeof(obj_p->hashedHMACKey));
          cb_p->opcode_iter_length =
             (EA_Cmd_SHA1_HMAC | 
              ((1 << EA_Cmd_Iterations_Shift)  & EA_Cmd_Iterations_Mask) |
              (EA_Cmd_Data_Length_Mask & msgLength));
          break;
         default:
            ret = N8_INVALID_HASH;
            break;
      }
      CHECK_RETURN(ret);

   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("Hash Complete Message", cb_p,
                           N8_CB_EA_HASHCOMPLETEMESSAGE_NUMCMDS);
   
   return ret;
} /* cb_ea_hashCompleteMessage */

/*****************************************************************************
 * cb_ea_hashEnd
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief  Cmd Blocks for hash end
 *
 * Command Block generation for EA function "Hash Last Segment,
 * Pre-Load IV from Cmd Block" (opcodes 0x14 and 0x24).  This hardware
 * command does the final hash for a complete message.  Note the
 * message passed is the residual from the previous set of partial
 * hashes.  The length may be from 0 to the maximum of 16K.
 *
 *  @param req_p               RW:  Request pointer.
 *  @param obj_p               RW:  Pointer to hash object.  
 *  @param hashMsg_a           RO:  Physical address of message to be hashed.
 *  @param msgLength           RO:  Length of message
 *  @param result_a            RO:  Pointer to allocated buffer for
 *                                  the results to be placed by the
 *                                  EA.  This value is used but not
 *                                  written by this function.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_INVALID_VALUE - the ssl packet type is not recognized<br>
 *    N8_MALLOC_FAILED - memory allocation failed<BR>
 *    N8_INVALID_HASH  - hash specified was invalid<br>
 *    N8_INVALID_CIPHER - cipher specified was invalid<br>
 *    N8_INVALID_VERSION - SSL version is not supported<br>
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t cb_ea_hashEnd(API_Request_t *req_p,
                          EA_CMD_BLOCK_t *cb_p,
                          const N8_HashObject_t *obj_p,
                          const n8_IVSrc_t ivSrc,
                          const uint32_t hashMsg_a,
                          const unsigned int msgLength,
                          const uint32_t result_a,
                          EA_CMD_BLOCK_t **next_cb_pp,
			  int            lastCmdBlock)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
         
      cb_p->read_data_addr_ls = hashMsg_a;
      cb_p->write_data_addr_ls = result_a;
      /* If this is the last command block, set the command complete bit */
      if (lastCmdBlock == N8_TRUE)
      {
         cb_p->cp_si_context = EA_Cmd_SI_Mask;
      }

      /* copy the iv */
      switch (ivSrc)
      {
         case N8_IPAD:
            memcpy(cb_p->hash_IV, obj_p->ipadHMAC_iv, sizeof(obj_p->iv));
            break;
         case N8_OPAD:
            memcpy(cb_p->hash_IV, obj_p->opadHMAC_iv, sizeof(obj_p->iv));
            break;
         default:
            memcpy(cb_p->hash_IV, obj_p->iv, sizeof(obj_p->iv));
            break;
      }
      /* set opcode */
      switch (obj_p->type)
      {
         case N8_MD5:
         case N8_HMAC_MD5:
         case N8_HMAC_MD5_96:
            cb_p->opcode_iter_length = EA_Cmd_MD5_End_cmdIV;
            break;
         case N8_SHA1:
         case N8_HMAC_SHA1:
         case N8_HMAC_SHA1_96: 
            cb_p->opcode_iter_length = EA_Cmd_SHA1_End_cmdIV;
            break;
         default:
            ret = N8_INVALID_HASH;
            break;
      } 
      CHECK_RETURN(ret);

      cb_p->opcode_iter_length |= (EA_Cmd_Data_Length_Mask & msgLength);

      /* set the previous length in BITS. */
      convertToBits(cb_p, obj_p, ivSrc);

      /* save next address for future use */
      if (next_cb_pp != NULL)
      {
         *next_cb_pp = (EA_CMD_BLOCK_t *) (cb_p + 1);
      }

   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("Hash End", cb_p,
                           N8_CB_EA_HASHEND_NUMCMDS);
   return ret;
} /* cb_ea_hashEnd */


/*****************************************************************************
 * cb_ea_hashHMACEnd
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief  Cmd Blocks for HMAC hash end
 *
 *
 *  @param req_p               RW:  Request pointer.
 *  @param obj_p               RW:  Pointer to hash object.  
 *  @param hashMsg_a           RO:  Physical address of message to be hashed.
 *  @param msgLength           RO:  Length of message
 *  @param result_a            RO:  Pointer to allocated buffer for
 *                                  the results to be placed by the
 *                                  EA.  This value is used but not
 *                                  written by this function.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_INVALID_VALUE - the ssl packet type is not recognized<br>
 *    N8_MALLOC_FAILED - memory allocation failed<BR>
 *    N8_INVALID_HASH  - hash specified was invalid<br>
 *    N8_INVALID_CIPHER - cipher specified was invalid<br>
 *    N8_INVALID_VERSION - SSL version is not supported<br>
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t cb_ea_hashHMACEnd(API_Request_t *req_p,
                          EA_CMD_BLOCK_t *cb_p,
                          const N8_HashObject_t *obj_p,
                          const uint32_t hashMsg_a,
                          const unsigned int msgLength,
                          const uint32_t result_a,
                          EA_CMD_BLOCK_t **next_cb_pp)
{
   N8_Status_t ret = N8_STATUS_OK;
   EA_HMAC_CMD_BLOCK_t *cb_HMAC_p = NULL;
   do
   {
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
      
      cb_HMAC_p = (EA_HMAC_CMD_BLOCK_t *) cb_p;

      cb_HMAC_p->read_data_addr_ls = (unsigned int) hashMsg_a;
      cb_HMAC_p->write_data_addr_ls = (unsigned int) result_a;
      memcpy(cb_HMAC_p->hmac_key, obj_p->hashedHMACKey, sizeof(obj_p->hashedHMACKey));
      /* set opcode */
      switch (obj_p->type)
      {
         case N8_HMAC_MD5:
         case N8_HMAC_MD5_96:
             cb_HMAC_p->opcode_iter_length = EA_Cmd_MD5_HMAC | msgLength |
                  ((1 << EA_Cmd_Iterations_Shift)  & EA_Cmd_Iterations_Mask);
            break;
         case N8_HMAC_SHA1:
         case N8_HMAC_SHA1_96: 
             cb_HMAC_p->opcode_iter_length = EA_Cmd_SHA1_HMAC | msgLength |
                  ((1 << EA_Cmd_Iterations_Shift)  & EA_Cmd_Iterations_Mask);
            break;
         default:
            ret = N8_INVALID_HASH;
            break;
      } 
      CHECK_RETURN(ret);

      /* save next address for future use */
      if (next_cb_pp != NULL)
      {
         *next_cb_pp = (EA_CMD_BLOCK_t *) (cb_p + 1);
      }

   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("Hash End", cb_p,
                           N8_CB_EA_HASHEND_NUMCMDS);
   return ret;
} /* cb_ea_hashEnd */


/*****************************************************************************
 * cb_ea_SSLKeyMaterialHash
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Compute SSL key material.
 *
 * The key material for an SSL hash is computed.
 *
 *  @param req_p               RW:  Pointer to api request buffer.
 *  @param key_a               RO:  Physical address of key to use.
 *  @param keyLength           RO:  Length of the key.
 *  @param random_p            RO:  Pointer to random data to use.
 *  @param outputLength        RO:  Length of output.
 *  @param result_a            RW:  Physical address of pre-allocated buffer
 *                                  for the results.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error condition if raised.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    Results buffer is pre-allocated and of a sufficient size.
 *****************************************************************************/
N8_Status_t cb_ea_SSLKeyMaterialHash(API_Request_t *req_p,
                                     EA_CMD_BLOCK_t *cmdBlock_p,
                                     const uint32_t key_a,
                                     const int keyLength,
                                     const N8_Buffer_t *random_p,
                                     const int outputLength,
                                     uint32_t result_a)
{
   N8_Status_t ret = N8_STATUS_OK;
   EA_MSH_CMD_BLOCK_t *cb_p = NULL;
      int iterationCount;

   do
   {
      CHECK_OBJECT(req_p, ret);
      cb_p = (EA_MSH_CMD_BLOCK_t *) cmdBlock_p;
      CHECK_OBJECT(cb_p, ret);

      /* create a command block for CCH opcode 0x30:
       * "Master Secret Hash Command"
       */

      iterationCount = (int) ((outputLength / (float) BYTES_PER_ITERATION) +
                               0.5);
      
      /* set the opcode and length */
      cb_p->opcode_iter_length =
         EA_Cmd_SSL30_Master_Secret_Hash |
         (iterationCount << EA_Cmd_Iterations_Shift) |
         keyLength;
      /* set the address of the key, or "master secret" in the read
       * pointer */
      cb_p->read_data_addr_ls = key_a;
      /* set the output address */
      cb_p->write_data_addr_ls = result_a;
      cb_p->cp_si_context = EA_Cmd_SI_Mask;

      /* set the random data */
      cb_p->random1[0] = BE_to_uint32(random_p);
      cb_p->random1[1] = BE_to_uint32(random_p+4);
      cb_p->random1[2] = BE_to_uint32(random_p+8);
      cb_p->random1[3] = BE_to_uint32(random_p+12);
      cb_p->random1[4] = BE_to_uint32(random_p+16);
      cb_p->random1[5] = BE_to_uint32(random_p+20);
      cb_p->random1[6] = BE_to_uint32(random_p+24);
      cb_p->random1[7] = BE_to_uint32(random_p+28);
      cb_p->random2[0] = BE_to_uint32(random_p+32);
      cb_p->random2[1] = BE_to_uint32(random_p+36);
      cb_p->random2[2] = BE_to_uint32(random_p+40);
      cb_p->random2[3] = BE_to_uint32(random_p+44);
      cb_p->random2[4] = BE_to_uint32(random_p+48);
      cb_p->random2[5] = BE_to_uint32(random_p+52);
      cb_p->random2[6] = BE_to_uint32(random_p+56);
      cb_p->random2[7] = BE_to_uint32(random_p+60);
   }
   while (FALSE);
   DBG_PRINT_EA_CMD_BLOCKS("SSL Key Material Hash", (EA_CMD_BLOCK_t *) cb_p,
                           N8_CB_EA_SSLKEYMATERIALHASH_NUMCMDS);

   /* clean up */
   /* nothing to clean up */

   return ret;
} /* cb_ea_SSLKeyMaterialHash */

/*****************************************************************************
 * cb_ea_SSL
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Create the command blocks to perform E/A or Decrypt for an SSL packet.
 *
 *  @param cmdBlock_p          RO:  Pointer to beginning of the command block.
 *  @param packetObj_p         RW:  Pointer to the packet object.
 *  @param packet_p            RO:  Pointer to the header/data packet.
 *  @param input_a             RO:  Physical address of incoming SSL packet
 *  @param result_a            RO:  Physical address of decrypted SSL packet
 *  @param opCode              RO:  Op Code to build into the command block.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error if raised.
 *
 * @par Errors
 *    N8_INVALID_VALUE - the ssl packet type is not recognized<br>
 *    N8_INVALID_VERSION - SSL version is not supported<br>
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t cb_ea_SSL(EA_CMD_BLOCK_t *cmdBlock_p,
                      N8_Packet_t *packetObj_p,
                      const N8_SSLTLSPacket_t *packet_p,
                      const uint32_t input_a,
                      const uint32_t result_a,
                      const unsigned int opCode)
{
   EA_SSL30_CMD_BLOCK_t *cb_p = (EA_SSL30_CMD_BLOCK_t *) cmdBlock_p;
   uint16_t length;
   uint16_t sslver;
   uint8_t type;

   /* convert network order packet header data to host order */
   type    = SSLTLS_EXTRACT_TYPE(packet_p);
   sslver = SSLTLS_EXTRACT_VERSION(packet_p);
   length  = SSLTLS_EXTRACT_LENGTH(packet_p);

   /* ensure the sslProtocol passed is one of the valid values */
   switch (type)
   {
      case N8_CHANGE_CIPHER_SPEC:
      case N8_ALERT:
      case N8_HANDSHAKE:
      case N8_APPLICATION_DATA:
         break;
      default:
         return(N8_INVALID_VALUE);
   }

   /* check the version to ensure it is correct */
   if (sslver != N8_SSL_VERSION)
   {
      return(N8_INVALID_VERSION);
   }

   /* set the common elements */
   cb_p->read_data_addr_ls = input_a;
   cb_p->write_data_addr_ls = result_a;
   cb_p->result_type = type;
   cb_p->SSL_version_length = (sslver << 16) + length;

   /* set the opcode */
   cb_p->opcode_iter_length = opCode | length;

   /* Assume context use */
   cb_p->cp_si_context = EA_Cmd_CP_Mask |
      (EA_Ctx_Addr_Address_Mask & packetObj_p->contextHandle.index);

   /* based on cipher/hash, set the opcode and other specifics */
   if (packetObj_p->packetCipher == N8_CIPHER_DES)
   {
      /* DES may use context index or provide data in the command block */
      if (packetObj_p->contextHandle.inUse == N8_FALSE)
      {
         EA_SSL30_DES_MD5_CMD_BLOCK_t *desMd5_p;
         int i;

         /* Do not use context */
         cb_p->cp_si_context = 0;
         if (packetObj_p->packetHashAlgorithm == N8_MD5)
         {
            desMd5_p = (EA_SSL30_DES_MD5_CMD_BLOCK_t *) cb_p;
            /* set the precompute values */
            for (i = 0; i < 4; i++)
            {
               desMd5_p->precompute1[i] =
                    packetObj_p->cipherInfo.precompute1[i];
               desMd5_p->precompute2[i] =
                    packetObj_p->cipherInfo.precompute2[i];
            }
         }
         else /* N8_SHA1 */
         {
            for (i = 0; i < 5; i++)
            {
               cb_p->write_secret[i] = BE_to_uint32(
                    &packetObj_p->cipherInfo.macSecret[i*sizeof(uint32_t)]);
            }
         }

         /* set the params common for 3DES */
         cb_p->des_IV_ms = 
               BE_to_uint32(&packetObj_p->cipherInfo.IV[N8_MS_BYTE]);
         cb_p->des_IV_ls = 
               BE_to_uint32(&packetObj_p->cipherInfo.IV[N8_LS_BYTE]);
         cb_p->des_key1_ms = 
               BE_to_uint32(&packetObj_p->cipherInfo.key1[N8_MS_BYTE]);
         cb_p->des_key1_ls = 
               BE_to_uint32(&packetObj_p->cipherInfo.key1[N8_LS_BYTE]);
         cb_p->des_key2_ms = 
               BE_to_uint32(&packetObj_p->cipherInfo.key2[N8_MS_BYTE]);
         cb_p->des_key2_ls = 
               BE_to_uint32(&packetObj_p->cipherInfo.key2[N8_LS_BYTE]);
         cb_p->des_key3_ms = 
               BE_to_uint32(&packetObj_p->cipherInfo.key3[N8_MS_BYTE]);
         cb_p->des_key3_ls = 
               BE_to_uint32(&packetObj_p->cipherInfo.key3[N8_LS_BYTE]);

         cb_p->sequence_number[0] = packetObj_p->cipherInfo.sequence_number[0];
         cb_p->sequence_number[1] = packetObj_p->cipherInfo.sequence_number[1];
         /* increment the sequence number.  should we later discover the
          * operation failed, the sequence number will need to be
          * returned to its previous value.
          */
         packetObj_p->cipherInfo.sequence_number[1]++;
         /* check for wrapping */
         if (packetObj_p->cipherInfo.sequence_number[1] == 0)
         {
            /* increment the sequence number ms */
            packetObj_p->cipherInfo.sequence_number[0]++;
         }
      }
   }

   DBG_PRINT_EA_CMD_BLOCKS("SSL", (EA_CMD_BLOCK_t *) cb_p,
                           N8_CB_EA_SSLENCRYPTAUTHENTICATE_NUMCMDS);
   return N8_STATUS_OK;
} /* cb_ea_SSL */

/*****************************************************************************
 * cb_ea_TLS
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Create the command block to perform E/A or Decrypt for a TLS packet.
 *
 *  @param cmdBlock_p          RO:  Pointer to beginning of the command block.
 *  @param packetObj_p         RW:  Pointer to the packet object.
 *  @param packet_p            RO:  Pointer to the header/data packet.
 *  @param input_a             RO:  Physical adress of input data.
 *  @param result_a            RO:  Physical adress of results area.
 *  @param opCode              RO:  Op Code to build into the command block.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error if raised.
 *
 * @par Errors
 *    N8_INVALID_VALUE - the TLS packet type is not recognized<br>
 *    N8_INVALID_VERSION - TLS version is not supported<br>
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t cb_ea_TLS(EA_CMD_BLOCK_t *cmdBlock_p,
                      N8_Packet_t *packetObj_p,
                      const N8_SSLTLSPacket_t *packet_p,
                      const uint32_t input_a,
                      const uint32_t result_a,
                      const unsigned int opCode)
{
   EA_SSL30_CMD_BLOCK_t *cb_p = (EA_SSL30_CMD_BLOCK_t *) cmdBlock_p;
   uint16_t length;
   uint16_t tlsver;
   uint8_t type;

   /* convert network order packet header data to host order */
   type    = SSLTLS_EXTRACT_TYPE(packet_p);
   tlsver = SSLTLS_EXTRACT_VERSION(packet_p);
   length  = SSLTLS_EXTRACT_LENGTH(packet_p);
      
   /* ensure the sslProtocol passed is one of the valid values */
   switch (type)
   {
      case N8_CHANGE_CIPHER_SPEC:
      case N8_ALERT:
      case N8_HANDSHAKE:
      case N8_APPLICATION_DATA:
         break;
      default:
         return (N8_INVALID_VALUE);
   }

   /* check the version to ensure it is correct */
   if (tlsver != N8_TLS_VERSION)
   {
      return (N8_INVALID_VERSION);
   }

   /* set the common elements */
   cb_p->read_data_addr_ls = input_a;
   cb_p->write_data_addr_ls = result_a;
   cb_p->result_type = type;
   cb_p->SSL_version_length = (tlsver << 16) + length;

   /* set the opcode */
   cb_p->opcode_iter_length = opCode | length;

   /* Assume context use */
   cb_p->cp_si_context |= EA_Cmd_CP_Mask |
         (EA_Ctx_Addr_Address_Mask & packetObj_p->contextHandle.index);

   /* based on cipher/hash, set the opcode and other specifics */
   if (packetObj_p->packetCipher == N8_CIPHER_DES)
   {
      /* DES may use context index or provide data in the command block */
      if (packetObj_p->contextHandle.inUse == N8_FALSE)
      {
         EA_TLS10_CMD_BLOCK_t *tls_p = (EA_TLS10_CMD_BLOCK_t *) cb_p;
         int i;

         /* Clear context use */
         cb_p->cp_si_context = 0;

         if (packetObj_p->packetHashAlgorithm == N8_HMAC_MD5)
         {
            for (i = 0; i < 4; i++)
            {
               tls_p->ipad[i] = packetObj_p->hashPacket.ipadHMAC_iv[i];
               tls_p->opad[i] = packetObj_p->hashPacket.opadHMAC_iv[i];
            }
         }
         else /* N8_HMAC_SHA1 */
         {
            for (i = 0; i<5; i++)
            {
               tls_p->ipad[i] = packetObj_p->hashPacket.ipadHMAC_iv[i];
               tls_p->opad[i] = packetObj_p->hashPacket.opadHMAC_iv[i];
            }
         }

         /* set the params common for 3DES */
         cb_p->des_IV_ms = 
            BE_to_uint32(&packetObj_p->cipherInfo.IV[N8_MS_BYTE]);
         cb_p->des_IV_ls = 
            BE_to_uint32(&packetObj_p->cipherInfo.IV[N8_LS_BYTE]);
         cb_p->des_key1_ms = 
            BE_to_uint32(&packetObj_p->cipherInfo.key1[N8_MS_BYTE]);
         cb_p->des_key1_ls = 
            BE_to_uint32(&packetObj_p->cipherInfo.key1[N8_LS_BYTE]);
         cb_p->des_key2_ms = 
            BE_to_uint32(&packetObj_p->cipherInfo.key2[N8_MS_BYTE]);
         cb_p->des_key2_ls = 
            BE_to_uint32(&packetObj_p->cipherInfo.key2[N8_LS_BYTE]);
         cb_p->des_key3_ms = 
            BE_to_uint32(&packetObj_p->cipherInfo.key3[N8_MS_BYTE]);
         cb_p->des_key3_ls = 
            BE_to_uint32(&packetObj_p->cipherInfo.key3[N8_LS_BYTE]);

         cb_p->sequence_number[0] = packetObj_p->cipherInfo.sequence_number[0];
         cb_p->sequence_number[1] = packetObj_p->cipherInfo.sequence_number[1];
         /* increment the sequence number.  should we later discover the
          * operation failed, the sequence number will need to be
          * returned to its previous value.
          */
         packetObj_p->cipherInfo.sequence_number[1]++;
         /* check for wrapping */
         if (packetObj_p->cipherInfo.sequence_number[1] == 0)
         {
            /* increment the sequence number ms */
            packetObj_p->cipherInfo.sequence_number[0]++;
         }
      }
   }

   DBG_PRINT_EA_CMD_BLOCKS("TLS", (EA_CMD_BLOCK_t *) cb_p,
                           N8_CB_EA_TLSENCRYPTAUTHENTICATE_NUMCMDS);
   return (N8_STATUS_OK);
} /* cb_ea_TLS */

/*****************************************************************************
 * cb_ea_TLSKeyMaterialHash
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Compute TLS key material.
 *
 * The key material for an TLS hash is computed.
 *
 *  @param req_p                   RW:  Pointer to api request buffer.
 *  @param msg_p                   RO:  Virtual address of message to hash.
 *  @param msg_a                   RO:  Physical address of message to hash.
 *  @param dataLength              RO:  Length of the message.
 *  @param hmacKey_p               RO:  Virtual address of HMAC key.
 *  @param hmacKey_a               RO:  Physical address of HMAC key.
 *  @param keyLength               RO:  Length of HMAC key.
 *  @param outputLength            RO:  Length of output buffer.
 *  @param pseudorandomStream1_a   RO:  First part of pseudo-random stream.
 *  @param pseudorandomStream2_a   RO:  Second part of pseudo-random stream.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status.  Error condition if raised.
 *
 * @par Errors
 *
 * @par Assumptions
 *    All buffers are pre-allocated and of a sufficient size.
 *****************************************************************************/
N8_Status_t cb_ea_TLSKeyMaterialHash(API_Request_t *req_p,
                                     EA_CMD_BLOCK_t *cmdBlock_p,
                                     const N8_Buffer_t *msg_p,
                                     const uint32_t msg_a,
                                     const int dataLength,
                                     N8_Buffer_t *hmacKey_p,
                                     const uint32_t hmacKey_a,
                                     const int keyLength,
                                     const int outputLength,
                                     const uint32_t pseudorandomStream1_a,
                                     const uint32_t pseudorandomStream2_a,
                                     const int keyLen)
{
   N8_Status_t ret = N8_STATUS_OK;
   EA_HMAC_CMD_BLOCK_t *cb_p = NULL;
   int iterationCount_MD5;
   int iterationCount_SHA1;
   int bufferA_MD5_length = 0;
   int bufferA_SHA1_length = 0;
   int i, n, k, l;
   N8_Buffer_t *secret1_p = NULL;
   N8_Buffer_t *secret2_p = NULL;
   N8_Buffer_t *bufferA_MD5_p = NULL;
   N8_Buffer_t *bufferA_SHA1_p = NULL;
   uint32_t bufferA_MD5_a;
   uint32_t bufferA_SHA1_a;
   unsigned int numCommands=0;
   
   do
   {
      CHECK_OBJECT(req_p, ret);
      cb_p = (EA_HMAC_CMD_BLOCK_t *) cmdBlock_p;
      CHECK_OBJECT(cb_p, ret);
         
      secret1_p = hmacKey_p;
      secret2_p = &(hmacKey_p[keyLength]);

      /* 1) compute the first pseudorandom stream */
      iterationCount_MD5  = N8_MD5_HASHES_REQUIRED_TLS(outputLength);
      iterationCount_SHA1 = N8_SHA1_HASHES_REQUIRED_TLS(outputLength);

      /* allocate space for the command block */
      numCommands = N8_CB_EA_TLSKEYMATERIALHASH_NUMCMDS(outputLength);

      /* the code needs to be restructured to not depend on the numCommands +1
       * for the malloc.  it works here where it is local, but if an external
       * caller it so preallocate the memory, it cannot be expected to allocate
       * n+1.  the code herein must be fixed to not have that dependency.
       */
      /* allocate a kernel buffer for the hashes */
      bufferA_MD5_length  = dataLength + EA_MD5_Hash_Length;
      bufferA_SHA1_length = dataLength + EA_SHA1_Hash_Length;

      bufferA_MD5_p  = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset + keyLen);
      bufferA_MD5_a  =   req_p->qr.physicalAddress + req_p->dataoffset + keyLen;
      bufferA_SHA1_p = (N8_Buffer_t *) ((int)req_p + req_p->dataoffset + keyLen) +
           NEXT_WORD_SIZE(bufferA_MD5_length);
      bufferA_SHA1_a =   req_p->qr.physicalAddress + req_p->dataoffset + keyLen  +
           NEXT_WORD_SIZE(bufferA_MD5_length);

      /*
       * Unfortunately HMAC command doesn't do what it supposed to do. 
       * We have to calculate HMAC values "by hand".
       * Create as many commands to do this as we need. 
       */
      
      /* Note that the first calculation is an oddball and must be done before
       * entering the loop.  Due to this, we generate n-1 pairs and have to do
       * the final calculation outside of the loop.  This is true for both MD5
       * and SHA1. */

      /* Calculate buffer A first time */
      cb_p->opcode_iter_length =
         EA_Cmd_MD5_HMAC | (1 << EA_Cmd_Iterations_Shift) | dataLength;
      cb_p->read_data_addr_ls = msg_a;
      cb_p->write_data_addr_ls = bufferA_MD5_a;

      /* set the HMAC key data */
      for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
      {
         cb_p->hmac_key[i] = BE_to_uint32(secret1_p+n);
      }

      memcpy(bufferA_MD5_p+EA_MD5_Hash_Length, msg_p, dataLength);
          
      cb_p++;

      for (l = 0, k = 0; l < iterationCount_MD5 - 1; l++, k+=EA_MD5_Hash_Length)
      {
         /* A) calculate pseudo random stream 1 */
         /* set the opcode and length */
         cb_p->opcode_iter_length =
            EA_Cmd_MD5_HMAC | (1 << EA_Cmd_Iterations_Shift) |
              bufferA_MD5_length;
         cb_p->read_data_addr_ls = bufferA_MD5_a;
         cb_p->write_data_addr_ls = (uint32_t) pseudorandomStream1_a + k;

         /* set the HMAC key data */
         /* TODO:  are both termination tests necessary?  will one ever fire
          * without the other? if not, just use the most restrictive. */
         for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
         {
            cb_p->hmac_key[i] = BE_to_uint32(secret1_p+n);
         }
              
         cb_p++;
         /* B) calculate buffer A */
         /* set the opcode and length */
         cb_p->opcode_iter_length =
            EA_Cmd_MD5_HMAC | (1 << EA_Cmd_Iterations_Shift) |
              EA_MD5_Hash_Length;
         cb_p->read_data_addr_ls = bufferA_MD5_a;
         cb_p->write_data_addr_ls =  bufferA_MD5_a;

         /* set the HMAC key data */
         for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
         {
            cb_p->hmac_key[i] = BE_to_uint32(secret1_p+n);
         }
              
         cb_p++;
      }
      /* compute the last psuedo random stream 1 */
      /* A) calculate pseudo random stream 1 */
      /* set the opcode and length */
      cb_p->opcode_iter_length =
         EA_Cmd_MD5_HMAC | (1 << EA_Cmd_Iterations_Shift) |
         bufferA_MD5_length;
      cb_p->read_data_addr_ls = bufferA_MD5_a;
      cb_p->write_data_addr_ls = (uint32_t) pseudorandomStream1_a + k;

         /* set the HMAC key data */
         /* TODO:  are both termination tests necessary?  will one ever fire
          * without the other? if not, just use the most restrictive. */
      for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
      {
         cb_p->hmac_key[i] = BE_to_uint32(secret1_p+n);
      }
      cb_p++;
              
      /* 2) compute the second pseudorandom stream */
      /* Calculate buffer A first time */
      cb_p->opcode_iter_length =
         EA_Cmd_SHA1_HMAC |
         (1 << EA_Cmd_Iterations_Shift) |
         dataLength;
      cb_p->read_data_addr_ls = msg_a;
      cb_p->write_data_addr_ls = bufferA_SHA1_a;

      /* set the HMAC key data */
      for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
      {
         cb_p->hmac_key[i] = BE_to_uint32(secret2_p+n);
      }

      memcpy(bufferA_SHA1_p+EA_SHA1_Hash_Length, msg_p, dataLength);

      cb_p++;

      for (l = 0, k = 0; l < iterationCount_SHA1 - 1; l++, k+=EA_SHA1_Hash_Length)
      {
         /* A) calculate pseudo random stream 2 */
         /* set the opcode and length */
         cb_p->opcode_iter_length =
            EA_Cmd_SHA1_HMAC |
            (1 << EA_Cmd_Iterations_Shift) |
            bufferA_SHA1_length;
          
         cb_p->read_data_addr_ls = bufferA_SHA1_a;
         cb_p->write_data_addr_ls = (uint32_t) pseudorandomStream2_a + k;
    
         /* set the HMAC key data */
         for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
         {
            cb_p->hmac_key[i] = BE_to_uint32(secret2_p+n);
         }
         cb_p++;
         
         /* B) calculate buffer A */
         /* set the opcode and length */
         cb_p->opcode_iter_length =
            EA_Cmd_SHA1_HMAC |
            (1 << EA_Cmd_Iterations_Shift) |
            EA_SHA1_Hash_Length;
         cb_p->read_data_addr_ls = bufferA_SHA1_a;
         cb_p->write_data_addr_ls = bufferA_SHA1_a;
    
         /* set the HMAC key data */
         for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
         {
            cb_p->hmac_key[i] = BE_to_uint32(secret2_p+n);
         }
         cb_p++;
      }

      /* do the final calculation for psuedo random stream 2 */
      /* A) calculate pseudo random stream 2 */
      /* set the opcode and length */
      cb_p->opcode_iter_length =
         EA_Cmd_SHA1_HMAC |
         (1 << EA_Cmd_Iterations_Shift) |
         bufferA_SHA1_length;
          
      cb_p->read_data_addr_ls = bufferA_SHA1_a;
      cb_p->write_data_addr_ls = (uint32_t) pseudorandomStream2_a + k;
      cb_p->cp_si_context = EA_Cmd_SI_Mask;
    
      /* set the HMAC key data */
      for (i = 0, n = 0; (i < 16) && (n < keyLength); i++, n +=4)
      {
         cb_p->hmac_key[i] = BE_to_uint32(secret2_p+n);
      }

   }
   while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("TLS Key Material Hash", 
                           (EA_CMD_BLOCK_t *) cmdBlock_p,
                           numCommands);

   /* clean up */
   return ret;
} /* cb_ea_TLSKeyMaterialHash */


/*****************************************************************************
 * cb_ea_IKEPrf
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Prepares the command blocks for the N8_IKEPrf API
 *
 *  @param req_p               RW: pointer to API request structure
 *  @param alg                 RO: hash algorithm (md5 or sha1)
 *  @param kMsg_a              RO: physical address of message to be hashed
 *  @param msgLength           RO: length of message in bytes
 *  @param kKey_p              RO: virtual address of key
 *  @param keyLength           RO: length of key in bytes
 *  @param kRes_a              RO: physical address of result
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - hash is neither md5 nor sha1
 *    N8_MALLOC_FAILED - problem allocating memory
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
N8_Status_t cb_ea_IKEPrf(API_Request_t *req_p,
                         EA_CMD_BLOCK_t *cmdBlock_p,
                         const N8_HashAlgorithm_t alg,
                         const uint32_t kMsg_a,
                         const uint32_t msgLength,
                         const N8_Buffer_t *kKey_p,
                         const uint32_t keyLength,
                         const uint32_t kRes_a)
{
    N8_Status_t ret = N8_STATUS_OK;
    EA_HMAC_CMD_BLOCK_t *cb_p = NULL;
    N8_Buffer_t hmacKey[EA_HMAC_Key_Length];
    int i, n;


    do
    {
        CHECK_OBJECT(req_p, ret);
        cb_p = (EA_HMAC_CMD_BLOCK_t *) cmdBlock_p;
        CHECK_OBJECT(cb_p, ret);

        /* pad key with zeroes up to B=64 bytes as per RFC 2104 */
        if (keyLength <= EA_HMAC_Key_Length)
        {
            memcpy(hmacKey, kKey_p, keyLength);
            memset(hmacKey+keyLength, 0x0, EA_HMAC_Key_Length - keyLength);
        }
        else
        {
            ret = N8_INVALID_KEY_SIZE;
            break;
        }


        /* choose opcode/iteration count (1) and msg length
         * opcode is based on algorithm
         */
        switch (alg)
        {
        case N8_HMAC_SHA1:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_SHA1_IPSEC_KEYMAT |
                (N8_IKE_PRF_ITERATIONS << EA_Cmd_Iterations_Shift) |
                    msgLength;
                break;
            }
        case N8_HMAC_MD5:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_MD5_IPSEC_KEYMAT |
                (N8_IKE_PRF_ITERATIONS << EA_Cmd_Iterations_Shift) |
                    msgLength;
                break;
            }
        default:
            {
                ret = N8_INVALID_HASH;
                break;
            }
        }

        CHECK_RETURN(ret);

        cb_p->read_data_addr_ls = kMsg_a;
        cb_p->write_data_addr_ls = kRes_a;
        cb_p->cp_si_context = EA_Cmd_SI_Mask;

        /* set the HMAC key data */
        for (i = 0, n = 0;i < EA_HMAC_Key_Length/sizeof(uint32_t);
             i++, n +=sizeof(uint32_t))
        {
            cb_p->hmac_key[i] = BE_to_uint32(hmacKey+n);
        }
    } while (FALSE);

    DBG_PRINT_EA_CMD_BLOCKS("IKEPrf", cmdBlock_p,
                            N8_CB_EA_IKEPRF_NUMCMDS);

    /* clean up */
    return ret;

} /* cb_ea_IKEPrf */

/*****************************************************************************
 * cb_ea_IKESKEYIDExpand
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Prepares command blocks for the N8_IKESKEYIDExpand API
 *
 *  @param req_p               RW: pointer to API request structure
 *  @param alg                 RO: hash algorithm (md5 or sha1)
 *  @param kMsg_a              RO: physical address of message to be hashed
 *  @param msgLength           RO: length of message to be hashed in bytes
 *  @param kKey_p              RO: virtual address of key
 *  @param keyLength           RO: length of key in bytes
 *  @param kSKEYIDd_a          RO: physical (base) address of result buffer
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - hash is neither md5 nor sha1
 *    N8_MALLOC_FAILED - problem allocating memory
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/

N8_Status_t cb_ea_IKESKEYIDExpand(API_Request_t *req_p,
                                  EA_CMD_BLOCK_t *cmdBlock_p,
                                  const N8_HashAlgorithm_t alg,
                                  const uint32_t kMsg_a,
                                  const uint32_t msgLength,
                                  const N8_Buffer_t *kKey_p,
                                  const uint32_t keyLength,
                                  const uint32_t kSKEYIDd_a)
{
    N8_Status_t ret = N8_STATUS_OK;
    EA_HMAC_CMD_BLOCK_t *cb_p = NULL;
    N8_Buffer_t hmacKey[EA_HMAC_Key_Length];
    int i, n;

    do
    {
        CHECK_OBJECT(req_p, ret);
        cb_p = (EA_HMAC_CMD_BLOCK_t *) cmdBlock_p;
        CHECK_OBJECT(cb_p, ret);

        /* pad key with zeroes up to B=64 bytes as per RFC 2104 */
        if (keyLength <= EA_HMAC_Key_Length)
        {
            memcpy(hmacKey, kKey_p, keyLength);
            memset(hmacKey+keyLength, 0x0, EA_HMAC_Key_Length - keyLength);

        }
        else
        {
            ret = N8_INVALID_KEY_SIZE;
            break;
        }
        
        /* choose opcode/iteration count (3) and msg length
         * opcode is based on algorithm
         */
        switch (alg)
        {
        case N8_HMAC_SHA1:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_SHA1_IPSEC_SKEYID |
                    (N8_IKE_SKEYID_ITERATIONS << EA_Cmd_Iterations_Shift) |
                    msgLength;
                break;
            }
        case N8_HMAC_MD5:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_MD5_IPSEC_SKEYID |
                    (N8_IKE_SKEYID_ITERATIONS << EA_Cmd_Iterations_Shift) |
                    msgLength;
                break;
            }
        default:
            {
                ret = N8_INVALID_HASH;
                break;
            }
        }

        CHECK_RETURN(ret);

        cb_p->read_data_addr_ls = kMsg_a;
        cb_p->write_data_addr_ls = kSKEYIDd_a;
        cb_p->cp_si_context = EA_Cmd_SI_Mask;

        /* set the HMAC key data */
        for (i = 0, n = 0;i < EA_HMAC_Key_Length/sizeof(uint32_t);
             i++, n +=sizeof(uint32_t))
        {
            cb_p->hmac_key[i] = BE_to_uint32(hmacKey+n);
        }


    } while (FALSE);

    DBG_PRINT_EA_CMD_BLOCKS("IKESKEYIDExpand", cmdBlock_p,
                            N8_CB_EA_IKESKEYIDEXPAND_NUMCMDS);
    /* clean up */
    return ret;
} /* cb_ea_IKESKEYIDExpand */

/*****************************************************************************
 * cb_ea_IKEKeyMaterialExpand
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Prepares command blocks for the N8_IKEKeyMaterialExpand API
 *
 * @param req_p RW: pointer to API request structure
 * @param alg RO: hash algorithm (md5 or sha1)
 * @param kMsg_a RO: physical address of message to be hashed
 * @param msgLength RO: length of message to be hashed in bytes
 * @param kKey_p RO: virtual address of key
 * @param keyLength RO: length of key in bytes
 * @param kRes_a RO: physical address of result buffer
 * @param i_count RO: hash iteration count
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - hash is neither md5 nor sha1
 *    N8_MALLOC_FAILED - problem allocating memory
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
N8_Status_t cb_ea_IKEKeyMaterialExpand(API_Request_t *req_p,
                                       EA_CMD_BLOCK_t *cmdBlock_p,
                                       const N8_HashAlgorithm_t alg,
                                       const uint32_t kMsg_a,
                                       const uint32_t msgLength,
                                       const N8_Buffer_t *kKey_p,
                                       const uint32_t keyLength,
                                       const uint32_t kRes_a,
                                       const uint32_t i_count)
{
    N8_Status_t ret = N8_STATUS_OK;
    EA_HMAC_CMD_BLOCK_t *cb_p = NULL;
    N8_Buffer_t hmacKey[EA_HMAC_Key_Length];
    int i, n;

    do
    {
        CHECK_OBJECT(req_p, ret);
        cb_p = (EA_HMAC_CMD_BLOCK_t *) cmdBlock_p;
        CHECK_OBJECT(cb_p, ret);

        /* pad key with zeroes up to B=64 bytes as per RFC 2104 */
        if (keyLength <= EA_HMAC_Key_Length)
        {
            memcpy(hmacKey, kKey_p, keyLength);
            memset(hmacKey+keyLength, 0x0, EA_HMAC_Key_Length - keyLength);

        }
        else
        {
            ret = N8_INVALID_KEY_SIZE;
            break;
        }

        /* choose opcode/iteration count (i_count) and msg length
         * opcode is based on algorithm
         */
        switch (alg)
        {
        case N8_HMAC_SHA1:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_SHA1_HMAC |
                (i_count << EA_Cmd_Iterations_Shift) | msgLength;
                break;
            }
        case N8_HMAC_MD5:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_MD5_HMAC |
                (i_count << EA_Cmd_Iterations_Shift) | msgLength;
                break;
            }
        default:
            {
                ret = N8_INVALID_HASH;
                break;
            }
        }

        CHECK_RETURN(ret);

        cb_p->read_data_addr_ls = kMsg_a;
        cb_p->write_data_addr_ls = kRes_a;
        cb_p->cp_si_context = EA_Cmd_SI_Mask;

        /* set the HMAC key data */
        for (i = 0, n = 0;i < EA_HMAC_Key_Length/sizeof(uint32_t);
             i++, n +=sizeof(uint32_t))
        {
            cb_p->hmac_key[i] = BE_to_uint32(hmacKey+n);
        }

    } while ( FALSE );

    DBG_PRINT_EA_CMD_BLOCKS("IKEKeyMaterialExpand",
                            cmdBlock_p,
                            N8_CB_EA_IKEKEYMATERIALEXPAND_NUMCMDS);

    /* clean up */
    return ret;

} /* cb_ea_IKEKeyMaterialExpand */

/*****************************************************************************
 * cb_ea_IKEEncryptKeyExpand
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Prepares command blocks for the N8_IKEEncryptKeyExpand API
 *
 * @param req_p RW: pointer to API request structure
 * @param alg RO: hash algorithm (md5 or sha1)
 * @param kMsg_a RO: physical address of message to be hashed
 * @param msgLength RO: length of message to be hashed in bytes
 * @param kKey_p RO: virtual address of key
 * @param keyLength RO: length of key in bytes
 * @param kRes_a RO: physical address of result buffer
 * @param i_count RO: hash iteration count
 *
 * @par Externals:
 *
 * @return 
 *    N8_STATUS_OK
 *
 * @par Errors:
 *    N8_INVALID_HASH - hash is neither md5 nor sha1
 *    N8_MALLOC_FAILED - problem allocating memory
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
N8_Status_t cb_ea_IKEEncryptKeyExpand(API_Request_t *req_p,
                                      EA_CMD_BLOCK_t *cmdBlock_p,
                                      const N8_HashAlgorithm_t alg,
                                      const uint32_t kMsg_a,
                                      const uint32_t msgLength,
                                      const N8_Buffer_t *kKey_p,
                                      const uint32_t keyLength,
                                      const uint32_t kRes_a,
                                      const uint32_t i_count)
{
    N8_Status_t ret = N8_STATUS_OK;
    EA_HMAC_CMD_BLOCK_t *cb_p = NULL;
    N8_Buffer_t hmacKey[EA_HMAC_Key_Length];
    int i, n;

    do
    {
        CHECK_OBJECT(req_p, ret);
        cb_p = (EA_HMAC_CMD_BLOCK_t *) cmdBlock_p;
        CHECK_OBJECT(cb_p, ret);

        /* pad key with zeroes up to B=64 bytes as per RFC 2104 */
        if (keyLength <= EA_HMAC_Key_Length)
        {
            memcpy(hmacKey, kKey_p, keyLength);
            memset(hmacKey+keyLength, 0x0, EA_HMAC_Key_Length - keyLength);

        }
        else
        {
            ret = N8_INVALID_KEY_SIZE;
            break;
        }
        
        /* choose opcode/iteration count (i_count) and msg length
         * opcode is based on algorithm
         */
        switch (alg)
        {
        case N8_HMAC_SHA1:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_SHA1_IPSEC_KEYMAT |
                (i_count << EA_Cmd_Iterations_Shift) | msgLength;
                break;
            }
        case N8_HMAC_MD5:
            {
                cb_p->opcode_iter_length =
                EA_Cmd_MD5_IPSEC_KEYMAT |
                (i_count << EA_Cmd_Iterations_Shift) | msgLength;
                break;
            }
        default:
            {
                ret = N8_INVALID_HASH;
                break;
            }
        }

        CHECK_RETURN(ret);

        cb_p->read_data_addr_ls = kMsg_a;
        cb_p->write_data_addr_ls = kRes_a;
        cb_p->cp_si_context = EA_Cmd_SI_Mask;

        /* set the HMAC key data */
        for (i = 0, n = 0;i < EA_HMAC_Key_Length/sizeof(uint32_t);
             i++, n +=sizeof(uint32_t))
        {
            cb_p->hmac_key[i] = BE_to_uint32(hmacKey+n);
        }
    } while ( FALSE );

    DBG_PRINT_EA_CMD_BLOCKS("IKEEncryptKeyExpand",
                            cmdBlock_p,
                            N8_CB_EA_IKEENCRYPTKEYEXPAND_NUMCMDS);

    /* clean up */
    return ret;


} /* cb_ea_IKEEncryptKeyExpand */

/*****************************************************************************
 * cb_ea_writeContext
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Creates command to Write Buffer to context memory.
 *
 * Creates write buffer to context memory specified by contextIndex.
 *
 * @param contextIndex RO: context memory to write to
 * @param req_p WO: command buffer
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *    req_p - pointer to command buffer
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   request buffer was not allocated.<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed.<BR>
 *   
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *****************************************************************************/

N8_Status_t  cb_ea_writeContext (API_Request_t *req_p,
                                 EA_CMD_BLOCK_t *cb_p,
                                 const unsigned int   contextIndex,
                                 const N8_Buffer_t   *bufferToWrite_p,
                                 const unsigned int   length)
{
   N8_Status_t ret = N8_STATUS_OK;     /* the return status: OK or ERROR */

   DBG(("cb_ea_writeContext\n"));

   do
   {
      /* verify passed parameter */
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);

      /* set the context memory index */
      cb_p->cp_si_context = contextIndex | EA_Cmd_CP_Mask | EA_Cmd_SI_Mask;
      /* set write command plus the number of bytes to write */
      cb_p->opcode_iter_length = 
         EA_Cmd_Write_Context_Memory | EA_CTX_Record_Byte_Length;

      /* set the address of Zero buffer (kmalloc sets buffer to zero) */
      /* memset(kmem_p->VirtualAddress, 0x0, CONTEXT_ENTRY_SIZE); */

      memcpy((void *)((int)req_p + req_p->dataoffset), bufferToWrite_p, length);
      cb_p->read_data_addr_ls = (uint32_t) req_p->qr.physicalAddress + req_p->dataoffset;
      /* free context memory */
  
   } while(FALSE);

   DBG(("cb_ea_writeContext - FINISHED\n"));
   return ret;
} /* cb_ea_writeContext */

/*****************************************************************************
 * cb_ea_readContext
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Creates command to Write Buffer to context memory.
 *
 * Creates write buffer to context memory specified by contextIndex.
 *
 * @param req_p                RW: command buffer
 * @param contextIndex         RO: context memory to write to
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *    req_p - pointer to command buffer
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   request buffer was not allocated.<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed.<BR>
 *   
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *****************************************************************************/

N8_Status_t  cb_ea_readContext (API_Request_t     *req_p,
                                EA_CMD_BLOCK_t *cb_p,
                                const unsigned int contextIndex,
                                const uint32_t     bufferToRead_a,
                                const unsigned int length)
{
   N8_Status_t ret = N8_STATUS_OK;     /* the return status: OK or ERROR */

   DBG(("cb_writeContext\n"));

   do
   {
      /* verify passed parameter */
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
      /* set the context memory index */
      cb_p->cp_si_context = contextIndex | EA_Cmd_CP_Mask | EA_Cmd_SI_Mask;
      /* set write command plus the number of bytes to write */
      cb_p->opcode_iter_length = 
         EA_Cmd_Read_Context_Memory | EA_CTX_Record_Byte_Length;
      /* set the address of Zero buffer */
      cb_p->write_data_addr_ls = (uint32_t) bufferToRead_a;
      /* free context memory */
  
   } while(FALSE);

   DBG(("cb_writeContext - FINISHED\n"));
   return ret;
} /* cb_ea_readContext */

/* RC4 as implemented from a posting from
 * Newsgroups: sci.crypt
 * From: sterndark@netcom.com (David Sterndark)
 * Subject: RC4 Algorithm revealed.
 * Message-ID: <sternCvKL4B.Hyy@netcom.com>
 * Date: Wed, 14 Sep 1994 06:35:31 GMT
 */
/* n8_RC4_set_key - Similar to RC4_set_key in openssl. Used to avoid
   dependency on OpenSSL in tests. */
void n8_RC4_set_key(N8_RC4_t *key, int len, const unsigned char *data)
   {
   unsigned int tmp;
   unsigned int id1,id2;
   unsigned int *d;
   unsigned int i;
   
   d= &(key->data[0]);
   for (i = 0; i < N8_ARC4_MAX_LENGTH; i++)
   {
      d[i]=i;
   }
   key->x = 0;     
   key->y = 0;     
   id1=id2=0;     

#define SK_LOOP(n) { \
   tmp=d[(n)]; \
   id2 = (data[id1] + tmp + id2) & 0xff; \
   if (++id1 == len) id1=0; \
   d[(n)]=d[id2]; \
   d[id2]=tmp; }

   for (i = 0; i < N8_ARC4_MAX_LENGTH; i += 4)
   {
      SK_LOOP(i+0);
      SK_LOOP(i+1);
      SK_LOOP(i+2);
      SK_LOOP(i+3);
   }
}


/***************************************************************************       
 * cb_ea_loadARC4KeyToContext
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Loads ARC4 key to context memory.
 *
 *  @param req_p               RW:  pointer to request block
 *  @param cb_p                RW:  pointer to command block space
 *  @param packetObj_p         RO:  pointer to packet object
 *  @param cipher_p            RO:  ARC4 key info
 *  @param hashAlgorithm       RO:  hash algorithm in use
 *  @param ctx_p               RW:  pointer to context - virtaul
 *  @param ctx_a               RW:  physical address of context
 *  @param next_cb_pp          RW:  pointer to next command block pointer
 *
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *          N8_INVALID_HASH     -   unsupported hash algorithm
 *   
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *    ARC4 key is valid.
 *****************************************************************************/
N8_Status_t cb_ea_loadARC4KeyToContext(API_Request_t           *req_p,
                                       EA_CMD_BLOCK_t          *cb_p,
                                       const N8_Packet_t       *packetObj_p, 
                                       const N8_CipherInfo_t   *cipher_p,
                                       const N8_HashAlgorithm_t hashAlgorithm,
                                       EA_ARC4_CTX             *ctx_p,
                                       const uint32_t           ctx_a,
                                       EA_CMD_BLOCK_t          **next_cb_pp)
{
   int i,j;
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */

   N8_RC4_t keyARC4;
   DBG(("Command Block: loadARC4KeyToContext\n"));

   do
   {
      /* verify passed parameters */
      CHECK_OBJECT(packetObj_p, ret);
      CHECK_OBJECT(cb_p, ret);
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cipher_p, ret);

      switch (hashAlgorithm)
      {
         case N8_SHA1:
            for (i = 0; i < N8_PRECOMPUTE_SIZE; i++)
            {
               ctx_p->secret1[i] = BE_to_uint32(&cipher_p->macSecret[i*sizeof(uint32_t)]);
            }
            break;
         case N8_MD5:
            for (i = 0; i < N8_PRECOMPUTE_SIZE; i++)
            {
               ctx_p->secret1[i] = packetObj_p->cipherInfo.precompute1[i];
               ctx_p->secret2[i] = packetObj_p->cipherInfo.precompute2[i];
            }
            break;
         case N8_HMAC_MD5:
         case N8_HMAC_SHA1:
            for (i = 0; i < N8_PRECOMPUTE_SIZE; i++)
            {
               ctx_p->secret1[i] = packetObj_p->hashPacket.ipadHMAC_iv[i];
               ctx_p->secret2[i] = packetObj_p->hashPacket.opadHMAC_iv[i];
            }
            break;
         default:
            ret = N8_INVALID_HASH;
            break;
      }
      CHECK_RETURN(ret);           

      ctx_p->sequence_number[0] = cipher_p->sequence_number[0];
      ctx_p->sequence_number[1] = cipher_p->sequence_number[1];

      memset(&keyARC4, 0x0, sizeof(N8_RC4_t));
      n8_RC4_set_key(&keyARC4, cipher_p->keySize, cipher_p->key.keyARC4);
      /* put the i & j counters in the context memory image */
      ctx_p->i_j = 
         ((keyARC4.y << EA_CTX_J_Shift) & EA_CTX_J_Mask) | 
         ((keyARC4.x << EA_CTX_I_Shift) & EA_CTX_I_Mask);     

      /* put the S-box data in the context memory image */
      for (i = 0,j = 0; i < 64; i++,j+=4)
      {
         ctx_p->s_box[i] =
            ((keyARC4.data[j]   & 0xff) << 24) |
            ((keyARC4.data[j+1] & 0xff) << 16) |
            ((keyARC4.data[j+2] & 0xff) <<  8) |
            ((keyARC4.data[j+3] & 0xff)      );

      }

#if 0
      {
         unsigned char *ptr; 
         ptr = (unsigned char *) ctx_p;
         DBG(("Context window image\n"));
         for (i=0; i<EA_CTX_Record_Byte_Length; i+=16) {
            DBG(("%02x%02x%02x%02x ", 
                   *ptr++, *ptr++, *ptr++, *ptr++));
            DBG(("%02x%02x%02x%02x ", 
                   *ptr++, *ptr++, *ptr++, *ptr++));
            DBG(("%02x%02x%02x%02x ", 
                   *ptr++, *ptr++, *ptr++, *ptr++));
            DBG(("%02x%02x%02x%02x ", 
                   *ptr++, *ptr++, *ptr++, *ptr++));
            DBG(("\n"));
         }
      }
#endif
       
      cb_p->cp_si_context = EA_Cmd_CP_Mask |
         (EA_Ctx_Addr_Address_Mask & packetObj_p->contextHandle.index);
      cb_p->read_data_addr_ls = (uint32_t) ctx_a;
      cb_p->opcode_iter_length = EA_Cmd_Write_Context_Memory | sizeof(EA_ARC4_CTX);
      
      /* save next address for future use */
      if (next_cb_pp != NULL)
      {
         *next_cb_pp = (EA_CMD_BLOCK_t *) (cb_p + 1);
      }

   } while(FALSE);
   DBG_PRINT_EA_CMD_BLOCKS("Load ARC4 Key to Context Memory", cb_p,
                           N8_CB_EA_LOADARC4KEYTOCONTEXT_NUMCMDS);
   return ret;
} /* cb_ea_loadARC4KeyToContext */

/***************************************************************************       
 * cb_ea_loadARC4key_Only
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Loads ARC4 key to context memory.
 *
 *
 * @param req_p             RW: pointer to request block
 * @param encryptObject_p   RO: pointer to encrypted object
 * @param cipher_p          RO: ARC4 key info
 * @param ctx_p        RW: pointer to ARC4 context
 *
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *          N8_INVALID_HASH     -   unsupported hash algorithm
 *   
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *    ARC4 key is valid.
 *****************************************************************************/
N8_Status_t  cb_ea_loadARC4keyOnly(API_Request_t *req_p,
                                   EA_CMD_BLOCK_t *cb_p,
                                   const N8_ContextHandle_t *contextHandle_p,
                                   const N8_EncryptCipher_t *cipher_p)
{
   int                i,j;                /* loop iterators */
   N8_Status_t        ret = N8_STATUS_OK; /* return code */
   EA_ARC4_CTX       *ctx_p = NULL;       /* context virtual pointer */
    
   N8_RC4_t keyARC4;

   DBG(("Command Block: cb_ea_loadARC4key_for_CryptoInterface\n"));

   do
   {
      /* verify passed parameters */
      CHECK_OBJECT(contextHandle_p, ret);
      CHECK_OBJECT(cipher_p, ret);
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);

      ctx_p = (EA_ARC4_CTX *) ((int)req_p + req_p->dataoffset);
      ctx_p->sequence_number[0] = cipher_p->sequence_number[0];
      ctx_p->sequence_number[1] = cipher_p->sequence_number[1];
       
      memset(&keyARC4, 0x0, sizeof(N8_RC4_t));

      n8_RC4_set_key(&keyARC4, cipher_p->keySize, cipher_p->key.keyARC4);
      /* put the i & j counters in the context memory image */
      ctx_p->i_j = 
         ((keyARC4.y << EA_CTX_J_Shift) & EA_CTX_J_Mask) | 
         ((keyARC4.x << EA_CTX_I_Shift) & EA_CTX_I_Mask);     

      /* put the S-box data in the context memory image */
      for (i=0,j=0; i<64; i++,j+=4) {
         ctx_p->s_box[i] =
            ((keyARC4.data[j]   & 0xff) << 24) |
            ((keyARC4.data[j+1] & 0xff) << 16) |
            ((keyARC4.data[j+2] & 0xff) <<  8) |
            ((keyARC4.data[j+3] & 0xff));
      }

      cb_p->cp_si_context = contextHandle_p->index | EA_Cmd_CP_Mask |
	                    EA_Cmd_SI_Mask;
      cb_p->opcode_iter_length = 
      cb_p->read_data_addr_ls = req_p->qr.physicalAddress + req_p->dataoffset;
      cb_p->opcode_iter_length = EA_Cmd_Write_Context_Memory | sizeof(EA_ARC4_CTX);


   } while(FALSE);
   DBG_PRINT_EA_CMD_BLOCKS("Load ARC4 Key Only", cb_p,
                           N8_CB_EA_LOADARC4KEYONLY_NUMCMDS);
   return ret;
} /* cb_ea_loadARC4keyOnly */

/*****************************************************************************
 * cb_ea_encrypt
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Create the command blocks to perform raw encryption.
 *
 *  @param req_p               RW:  Request pointer.
 *  @param encryptObject_p     RO:  Pointer to the encrypted object.
 *  @param message_p           RO:  Address of input message.
 *  @param encryptedMessage_p  RW:  Pointer to the encrypted message (result).
 *  @param messageLength       RO:  Message length.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error if raised.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t cb_ea_encrypt(const API_Request_t     *req_p,
                          EA_CMD_BLOCK_t          *cb_p,
                          N8_EncryptObject_t      *encryptObject_p,
                          const uint32_t           message_a,
                          const uint32_t           encryptedMessage_a,
                          const int                messageLength)
{
   N8_Status_t ret = N8_STATUS_OK;
   do
   {
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);

      /* set the common elements */
      cb_p->read_data_addr_ls = message_a;
      cb_p->write_data_addr_ls = encryptedMessage_a;
      cb_p->cp_si_context = EA_Cmd_SI_Mask;

      /* based on cipher/hash, set the opcode and other specifics */
      if (encryptObject_p->cipher == N8_CIPHER_ARC4)
      {
         /* ARC4 must use context index */
         cb_p->cp_si_context |= EA_Cmd_CP_Mask |
            (EA_Ctx_Addr_Address_Mask & encryptObject_p->contextHandle.index);
         /* set the opcode */
         cb_p->opcode_iter_length = EA_Cmd_ARC4 | messageLength;
      }
      else if (encryptObject_p->cipher == N8_CIPHER_DES)
      {
         /* set the opcode */
         cb_p->opcode_iter_length = EA_Cmd_3DES_CBC_Encrypt | messageLength;
         /* DES may use context index or provide data in the command block */
         if (encryptObject_p->contextHandle.inUse == N8_FALSE)
         {
            /* set the params common for 3DES */
            cb_p->des_IV_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.IV[N8_MS_BYTE]);
            cb_p->des_IV_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.IV[N8_LS_BYTE]);
            cb_p->des_key1_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key1[N8_MS_BYTE]);
            cb_p->des_key1_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key1[N8_LS_BYTE]);
            cb_p->des_key2_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key2[N8_MS_BYTE]);
            cb_p->des_key2_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key2[N8_LS_BYTE]);
            cb_p->des_key3_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key3[N8_MS_BYTE]);
            cb_p->des_key3_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key3[N8_LS_BYTE]);

            /* increment the sequence number.  should we later discover the
             * operation failed, the sequence number will need to be returned to
             * its previous value.
             */
            encryptObject_p->cipherInfo.sequence_number[1]++;
            /* check for wrapping */
            if (encryptObject_p->cipherInfo.sequence_number[1] == 0)
            {
               /* increment the sequence number ms */
               encryptObject_p->cipherInfo.sequence_number[0]++;
            }
         }
         else
         {
            /* set the CP bit and the context index */
            cb_p->cp_si_context |= EA_Cmd_CP_Mask |
               (EA_Ctx_Addr_Address_Mask & encryptObject_p->contextHandle.index);
         }
      }
      else
      {
         ret = N8_INVALID_ENUM;
         break;
      }
   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("cb_ea_encrypt", (EA_CMD_BLOCK_t *) cb_p,
                           N8_CB_EA_ENCRYPT_NUMCMDS);
   return ret;
} /* cb_ea_Encrypt */


/*****************************************************************************
 * cb_ea_decrypt
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Create the command blocks to perform decrypt operation.
 *
 *  @param req_p                  RW:  Request pointer.
 *  @param encryptObject_p        RO:  Pointer to the encrypted object.
 *  @param message_p              RO:  Address of results area.
 *  @param encryptedMessage_p     RW:  Pointer to the encrypted message.
 *  @param encryptedMessageLength RO:  Encrypted message length.
 *
 * @par Externals
 *    None
 *
 * @return
 *    Error if raised.
 *
 * @par Errors
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
N8_Status_t cb_ea_decrypt(API_Request_t            *req_p,
                          EA_CMD_BLOCK_t           *cb_p,
                          N8_EncryptObject_t       *encryptObject_p,
                          const uint32_t            encryptedMessage_a,
                          const uint32_t            message_a,
                          const unsigned int        encryptedMessageLength)
{
   N8_Status_t ret = N8_STATUS_OK;
   
   do
   {
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
      
      /* set the common elements */
      cb_p->read_data_addr_ls  = encryptedMessage_a;
      cb_p->write_data_addr_ls = message_a;
      cb_p->cp_si_context = EA_Cmd_SI_Mask;

      /* based on cipher, set the opcode and other specifics */
      if (encryptObject_p->cipher == N8_CIPHER_ARC4)
      {
         /* ARC4 must use context index */
         cb_p->cp_si_context |= EA_Cmd_CP_Mask |
            (EA_Ctx_Addr_Address_Mask & encryptObject_p->contextHandle.index);
         /* set the opcode */
         cb_p->opcode_iter_length = EA_Cmd_ARC4 | encryptedMessageLength;
      }
      else                      /* must be 3DES */
      {
         /* set the opcode */
         cb_p->opcode_iter_length = EA_Cmd_3DES_CBC_Decrypt | encryptedMessageLength;
         /* DES may use context index or provide data in the command block */
         if (encryptObject_p->contextHandle.inUse == N8_FALSE)
         {
            /* set the params common for 3DES */
            cb_p->des_IV_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.IV[N8_MS_BYTE]);
            cb_p->des_IV_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.IV[N8_LS_BYTE]);
            cb_p->des_key1_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key1[N8_MS_BYTE]);
            cb_p->des_key1_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key1[N8_LS_BYTE]);
            cb_p->des_key2_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key2[N8_MS_BYTE]);
            cb_p->des_key2_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key2[N8_LS_BYTE]);
            cb_p->des_key3_ms = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key3[N8_MS_BYTE]);
            cb_p->des_key3_ls = 
               BE_to_uint32(&encryptObject_p->cipherInfo.key.keyDES.key3[N8_LS_BYTE]);

            /* increment the sequence number.  should we later discover the
             * operation failed, the sequence number will need to be returned to
             * its previous value.
             */
            encryptObject_p->cipherInfo.sequence_number[1]++;
            /* check for wrapping */
            if (encryptObject_p->cipherInfo.sequence_number[1] == 0)
            {
               /* increment the sequence number ms */
               encryptObject_p->cipherInfo.sequence_number[0]++;
            }
         }
         else
         {
            /* set the CP bit and the context index */
            cb_p->cp_si_context |= EA_Cmd_CP_Mask |
               (EA_Ctx_Addr_Address_Mask & encryptObject_p->contextHandle.index);
         }
      }
   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("cb_ea_decrypt",
                           (EA_CMD_BLOCK_t *) cb_p,
                           N8_CB_EA_DECRYPT_NUMCMDS); 
   return ret;
} /* cb_ea_decrypt */

/*****************************************************************************
 * cb_ea_loadDESKeyToContext
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Loads DES key to context memory.
 *
 *
 * @param packetObj_p       RO: pointer to packet object
 * @param cipherInfo_p      RO: pointer to cipher infor
 * @param req_p             RW: pointer to request block
 * @param hashAlgorithm     RW: hash algorithm in use
 *
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *   
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *    DES keys are valid.
 *****************************************************************************/

N8_Status_t cb_ea_loadDESKeyToContext(API_Request_t           *req_p,
                                      EA_CMD_BLOCK_t          *cb_p,
                                      const N8_Packet_t    *packetObj_p, 
                                      const N8_CipherInfo_t   *cipherInfo_p,
                                      const N8_HashAlgorithm_t hashAlgorithm,
                                      EA_SSL30_CTX            *ctx_p,
                                      const uint32_t          ctx_a,
                                      EA_CMD_BLOCK_t          **next_cb_pp)
{
   unsigned int i;
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */

   DBG(("cb_ea_loadDESKeyToContext\n"));
   do
   {
      /* verify passed parameters */
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
      CHECK_OBJECT(packetObj_p, ret);
      CHECK_OBJECT(cipherInfo_p, ret);

      /* build a context window image with the keys, IVs and other information*/
      ctx_p->des_IV_ms = 
         BE_to_uint32(&cipherInfo_p->IV[N8_MS_BYTE]);
      ctx_p->des_IV_ls = 
         BE_to_uint32(&cipherInfo_p->IV[N8_LS_BYTE]);
      ctx_p->des_key1_ms = 
         BE_to_uint32(&cipherInfo_p->key1[N8_MS_BYTE]);
      ctx_p->des_key1_ls = 
         BE_to_uint32(&cipherInfo_p->key1[N8_LS_BYTE]);
      ctx_p->des_key2_ms = 
         BE_to_uint32(&cipherInfo_p->key2[N8_MS_BYTE]);
      ctx_p->des_key2_ls = 
         BE_to_uint32(&cipherInfo_p->key2[N8_LS_BYTE]);
      ctx_p->des_key3_ms = 
         BE_to_uint32(&cipherInfo_p->key3[N8_MS_BYTE]);
      ctx_p->des_key3_ls = 
         BE_to_uint32(&cipherInfo_p->key3[N8_LS_BYTE]);

      switch (hashAlgorithm)
      {
         case N8_MD5:
            for (i = 0; i < N8_PRECOMPUTE_SIZE; i++)
            {
               ctx_p->secret1[i] = packetObj_p->cipherInfo.precompute1[i];
               ctx_p->secret2[i] = packetObj_p->cipherInfo.precompute2[i];
            }
            break;
         case N8_HMAC_MD5_96:
         case N8_HMAC_SHA1_96:
         case N8_HMAC_MD5:
         case N8_HMAC_SHA1:
            for (i = 0; i < N8_PRECOMPUTE_SIZE; i++)
            {
               ctx_p->secret1[i] = packetObj_p->hashPacket.ipadHMAC_iv[i];
               ctx_p->secret2[i] = packetObj_p->hashPacket.opadHMAC_iv[i];
            }
            break;
         case N8_SHA1:
             for (i = 0; i<5; i++)
             {
                ctx_p->secret1[i] =
                   BE_to_uint32(&cipherInfo_p->macSecret[i * sizeof(uint32_t)]);
             }
             break;
         case N8_HASH_NONE:
             break;
         default:
            DBG(("Invalid hash: %s\n", N8_HashAlgorithm_t_text(hashAlgorithm)));
            ret = N8_INVALID_HASH;
            break;
      }
      CHECK_RETURN(ret);           
      ctx_p->sequence_number[0] = cipherInfo_p->sequence_number[0];
      ctx_p->sequence_number[1] = cipherInfo_p->sequence_number[1];
       
      /* insert a command to put the keys in context memory window */
      cb_p->cp_si_context = EA_Cmd_CP_Mask |
         (EA_Ctx_Addr_Address_Mask & packetObj_p->contextHandle.index);
      cb_p->opcode_iter_length = 
         EA_Cmd_Write_Context_Memory | sizeof(EA_SSL30_CTX);
      cb_p->read_data_addr_ls = (uint32_t) ctx_a;

      /* save next address for future use */
      if (next_cb_pp != NULL)
      {
         *next_cb_pp = (EA_CMD_BLOCK_t *) (cb_p + 1);
      }

   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("Load DES Key to Context Memory", cb_p,
                           N8_CB_EA_LOADDESKEYTOCONTEXT_NUMCMDS);
   return ret;
} /* cb_ea_loadDESKeyToContext */



/*****************************************************************************
 * cb_ea_loadDESkeyOnly
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Loads DES key to context memory.
 *
 * @param req_p             RW: pointer to request block
 * @param encryptObject_p   RO: pointer to encrypted object
 * @param cipherInfo_p      RO: pointer to cipher info
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *   
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *    DES keys are valid.
 *****************************************************************************/
N8_Status_t cb_ea_loadDESkeyOnly(API_Request_t *req_p,
                                 EA_CMD_BLOCK_t *cb_p,
                                 const N8_ContextHandle_t *contextHandle_p,
                                 const N8_EncryptCipher_t *cipherInfo_p)
{
   N8_Status_t ret = N8_STATUS_OK;    /* the return status: OK or ERROR */
   EA_SSL30_CTX      *ctx_p = NULL;
   
   DBG(("cb_ea_loadDESkey_for_CryptoInterface\n"));

   do
   {
      /* verify passed parameters */
      CHECK_OBJECT(contextHandle_p, ret);
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cipherInfo_p, ret);
      CHECK_OBJECT(cb_p, ret);

      ctx_p = (EA_SSL30_CTX *) ((int)req_p + req_p->dataoffset);

      /* build a context window image with the keys, IVs and other information*/
      ctx_p->des_IV_ms = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.IV[N8_MS_BYTE]);
      ctx_p->des_IV_ls = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.IV[N8_LS_BYTE]);
      ctx_p->des_key1_ms = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.key1[N8_MS_BYTE]);
      ctx_p->des_key1_ls = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.key1[N8_LS_BYTE]);
      ctx_p->des_key2_ms = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.key2[N8_MS_BYTE]);
      ctx_p->des_key2_ls = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.key2[N8_LS_BYTE]);
      ctx_p->des_key3_ms = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.key3[N8_MS_BYTE]);
      ctx_p->des_key3_ls = 
         BE_to_uint32(&cipherInfo_p->key.keyDES.key3[N8_LS_BYTE]);

      ctx_p->sequence_number[0] = cipherInfo_p->sequence_number[0];
      ctx_p->sequence_number[1] = cipherInfo_p->sequence_number[1];
       
      /* insert a command to put the keys in context memory window */
      cb_p->cp_si_context = contextHandle_p->index | EA_Cmd_CP_Mask | 
	                    EA_Cmd_SI_Mask;
      cb_p->opcode_iter_length = 
         EA_Cmd_Write_Context_Memory | sizeof(EA_SSL30_CTX);
      cb_p->read_data_addr_ls = req_p->qr.physicalAddress + req_p->dataoffset;


   } while(FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("Load DES Key Only", cb_p,
                           N8_CB_EA_LOADDESKEYONLY_NUMCMDS);
   return ret;
} /* cb_ea_loadDESkeyOnly */

/*****************************************************************************
 * cb_ea_loadIPsecKeyToContext
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Loads IPsec DES key to context memory.
 *
 *
 * @param contextIndex      RO: context memory to write
 * @param cipherInfo_p      RO: pointer to cipher infor
 * @param req_p     RW: pointer to request block
 *
 *
 * @return 
 *    ret - returns N8_STATUS_OK if successful or Error value.
 *
 * @par Errors:
 *          N8_INVALID_OBJECT   -   context request object is NULL<BR>
 *          N8_MALLOC_FAILED    -   memory allocation failed<BR>
 *   
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *    DES keys are valid.
 *****************************************************************************/

N8_Status_t cb_ea_loadIPsecKeyToContext(API_Request_t          *req_p,
                                        EA_CMD_BLOCK_t          *cb_p,
                                        const unsigned int      contextIndex, 
                                        const N8_CipherInfo_t   *cipherInfo_p,
                                        EA_IPSEC_CTX            *IPsec_ctx_p,
                                        const uint32_t          IPsec_ctx_a,
                                        EA_CMD_BLOCK_t          **next_cb_pp)
{
   N8_Status_t     ret = N8_STATUS_OK;    /* the return status: OK or ERROR */

   DBG(("cb_ea_loadIPsecKeyToContext\n"));

   do
   {
      /* verify passed parameters */
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
      CHECK_OBJECT(cipherInfo_p, ret);


      /* build a context window image with the keys, IVs and other information*/
      IPsec_ctx_p->des_key1_ms = 
         BE_to_uint32(&cipherInfo_p->key1[N8_MS_BYTE]);
      IPsec_ctx_p->des_key1_ls = 
         BE_to_uint32(&cipherInfo_p->key1[N8_LS_BYTE]);
      IPsec_ctx_p->des_key2_ms = 
         BE_to_uint32(&cipherInfo_p->key2[N8_MS_BYTE]);
      IPsec_ctx_p->des_key2_ls = 
         BE_to_uint32(&cipherInfo_p->key2[N8_LS_BYTE]);
      IPsec_ctx_p->des_key3_ms = 
         BE_to_uint32(&cipherInfo_p->key3[N8_MS_BYTE]);
      IPsec_ctx_p->des_key3_ls = 
         BE_to_uint32(&cipherInfo_p->key3[N8_LS_BYTE]);


      /* insert a command to put the keys in context memory window */
      cb_p->cp_si_context = contextIndex | EA_Cmd_CP_Mask;
      cb_p->opcode_iter_length = 
         EA_Cmd_Write_Context_Memory | sizeof(EA_IPSEC_CTX);
      cb_p->read_data_addr_ls = (uint32_t) IPsec_ctx_a;

      /* save next address for future use */
      if (next_cb_pp != NULL)
      {
         *next_cb_pp = (EA_CMD_BLOCK_t *) (cb_p + 1);
      }

   } while(FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("Load IPsec Key to Context Memory", cb_p,
                           N8_CB_EA_LOADIPSECKEYTOCONTEXT_NUMCMDS);
   return ret;
} /* cb_ea_loadIPsecKeyToContext */

/*****************************************************************************
 * cb_ea_IPsec
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Create IPsec encrypt/authenticate or decrypt/verify command block.
 *
 *
 *
 * @param cmdBlock_p     RO:    Pointer to the beginning of the command block.
 * @param packetObj_p    RO:    The  object denoting the decryption and
 *                              verification computation to be done.
 *                              PacketObject must have been initialized
 *                              for use with IPsec. 
 *                              The state in PacketObject will be updated if 
 *                              necessary as part of the call. <BR> 
 * @param packet_a       RO:    Physical address of IPsec packet<BR>
 * @param result_a       WO:    Physical address of the encrypted and
 *                              authenticated result.<BR>
 * @param packetLength   RO:    Packet length.<BR>
 * @param SPI            RO:    The  IPsec Security Parameter Index for the 
 *                              packet. (4 bytes).<BR> 
 * @param opCode         RO:    Op Code to build into the command block.<BR>
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    N/A
 *
 * @par Assumptions:
 *    contextIndex is valid and was passed to us by his rightful owner.
 *    DES keys are valid.
 *****************************************************************************/
void  cb_ea_IPsec(EA_CMD_BLOCK_t         *cmdBlock_p,
                  const N8_Packet_t      *packetObj_p, 
                  const uint32_t          packet_a, 
                  const uint32_t          result_a, 
                  const unsigned int      packetLength,
                  const int               SPI,
                  const unsigned int      opCode)
{
   EA_IPSEC_CMD_BLOCK_t  *cb_p = (EA_IPSEC_CMD_BLOCK_t *)cmdBlock_p;
   int i;

   DBG(("cb_ea_IPsec\n"));

   if (packetObj_p->contextHandle.inUse)
   {
      /* read keys from context memory */
      cb_p->cp_si_context = EA_Cmd_CP_Mask |
         (EA_Ctx_Addr_Address_Mask & packetObj_p->contextHandle.index);
   }
   else
   {
      /* read keys from command block */
      for (i = 0; i<5; i++)
      {
         cb_p->ipad[i] = packetObj_p->cipherInfo.key.IPsecKeyDES.ipad[i];
         cb_p->opad[i] = packetObj_p->cipherInfo.key.IPsecKeyDES.opad[i];
      }

      cb_p->des_key1_ms = 
         BE_to_uint32(&packetObj_p->cipherInfo.key1[N8_MS_BYTE]);
      cb_p->des_key1_ls = 
         BE_to_uint32(&packetObj_p->cipherInfo.key1[N8_LS_BYTE]);
      cb_p->des_key2_ms = 
         BE_to_uint32(&packetObj_p->cipherInfo.key2[N8_MS_BYTE]);
      cb_p->des_key2_ls = 
         BE_to_uint32(&packetObj_p->cipherInfo.key2[N8_LS_BYTE]);
      cb_p->des_key3_ms = 
         BE_to_uint32(&packetObj_p->cipherInfo.key3[N8_MS_BYTE]);
      cb_p->des_key3_ls = 
         BE_to_uint32(&packetObj_p->cipherInfo.key3[N8_LS_BYTE]);

   }

   cb_p->opcode_iter_length = opCode | packetLength;
 
   cb_p->read_data_addr_ls = (uint32_t) packet_a;
   cb_p->write_data_addr_ls = (uint32_t) result_a;
   cb_p->SPI = SPI;
   cb_p->sequence_number = packetObj_p->cipherInfo.key.IPsecKeyDES.sequence_number;
   cb_p->des_IV_ms = BE_to_uint32(&packetObj_p->cipherInfo.IV[N8_MS_BYTE]);
   cb_p->des_IV_ls = BE_to_uint32(&packetObj_p->cipherInfo.IV[N8_LS_BYTE]);


   DBG_PRINT_EA_CMD_BLOCKS("IPsec ",
                           (EA_CMD_BLOCK_t *) cb_p,
                           N8_CB_EA_IPSECENCRYPTAUTHENTICATE_NUMCMDS);
} /* cb_ea_IPsec */


/**************************************************
 * Local Functions
 **************************************************/

/*****************************************************************************
 * convertToBits
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Convert a 64 bit byte length to bits. 
 *
 * The CCH requires the hash message length be specified in a 64 bit
 * quantity representing the number of bits.  The API up to this point
 * keeps the value as the number of bytes spread across two 32 bit
 * quantities.  This routine multiplies the two 32-bit quantities by
 * 8.  It is done by shifting left 3 places.  Of course, the upper
 * 3 bits of the least significant must be placed back into the most
 * significant.
 *
 *  @param cb_p                RW:  command block pointer
 *  @param obj_p               RO:  hash object
 *
 * @par Externals
 *    None
 *
 * @par Errors
 *    If the previous quantity is > 0x000FFFFF FFFFFFFF an overflow
 * will occur.<br>
 *
 * @par Assumptions
 *    None
 *****************************************************************************/
static void convertToBits(EA_CMD_BLOCK_t *cb_p,
                          const N8_HashObject_t *obj_p,
                          const n8_IVSrc_t ivSrc)

{
   uint32_t high, low;
   switch (ivSrc)
   {
      case N8_OPAD:
         high = obj_p->opad_Nh;
         low  = obj_p->opad_Nl;
         break;
      default:
         high = obj_p->Nh;
         low  = obj_p->Nl;
         break;
   }
   
   cb_p->prev_length_ms = (high << 3) | (low >> 29);
   cb_p->prev_length_ls = low << 3;
} /* convertToBits */

/*****************************************************************************
 * cb_ea_TLSHandshakeHash
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Generate the command blocks for the N8_HandshakeHashEnd 
 *  N8_TLS_CERT and N8_TLS_FINISH modes.
 *
 * This function generates the command blocks for the N8_TLS_CERT and
 * N8_TLS_FINISH modes. The algorithm is defined in RFC 2246 section 5.
 *
 *
 * @param req_p        RO: The API request.
 * @param protocol     RO: The protocol (N8_TLS_CERT or N8_TLS_FINISH).
 * @param resMD5_a     RO: Physical address for MD5 result.
 * @param hashMsgMD5_a RO: Physical address for MD5 input.
 * @param md5Length    RO: Length of MD5 input.
 * @param resSHA_a     RO: Physical address for SHA1 result.
 * @param hashMsgSHA_a RO: Physical address for SHA1 input.
 * @param sha1Length   RO: Length of SHA1 input.
 * @param resMD5PRF_a  RO: Physical address for MD5 TLS PRF result.
 * @param resSHA1PRF_a RO: Physical address for SHA TLS PRF result.
 * @param key_p        RO: The key for this transaction.
 * @param keyLength    RO: The length of the key.
 * @param roleStr_a    RO: Physical address for the role string.
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK on success.
 *    N8_INVALID_PARAMETER if the key is too long.
 *    N8_INVALID_OBJECT if one of the inputs is invalid.
 *
 * @par Errors:
 *    See return section.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    We are assuming that the command blocks are all memset to zero before
 *    they are passed in to this function.
 *****************************************************************************/

N8_Status_t 
cb_ea_TLSHandshakeHash(API_Request_t      *req_p,
                       N8_HashProtocol_t   protocol,
                       uint32_t            resMD5_a,
                       uint32_t            hashMsgMD5_a, 
                       N8_HashObject_t     *hashMsgMD5_p,
                       int                 md5Length,
                       uint32_t            resSHA1_a,
                       uint32_t            hashMsgSHA1_a,
                       N8_HashObject_t     *hashMsgSHA1_p,
                       int                 sha1Length,
                       uint32_t            resMD5PRF_a,
                       uint32_t            resSHA1PRF_a,
                       const N8_Buffer_t  *key_p,
                       int                 keyLength,
                       uint32_t            roleStr_a)
{
   EA_CMD_BLOCK_t        *cb_p = NULL;
   EA_HMAC_CMD_BLOCK_t   *cb_HMAC_p = NULL;
   N8_Status_t            ret = N8_STATUS_OK; /* return status: OK or ERROR */
   int                    halfKey;
   int                    dataLength;
   int                    nCmdBlocks = N8_CB_EA_CERTTLSHANDSHAKE_NUMCMDS;
   uint32_t               keyBuffer[N8_HASH_BLOCK_SIZE];
   int                    i;

   DBG(("cb_ea_TLSHandshakeHash\n"));

   do
   {
      /* verify passed parameters */
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(req_p->EA_CommandBlock_ptr, ret);

      if (keyLength > N8_HASH_BLOCK_SIZE * 2)
      {
         /* Note that eventually the upper level function
            will hash the key if it is too long.  At the moment
            we are just going to return an error */
         ret = N8_INVALID_PARAMETER;
         break; 
      }

      cb_p = req_p->EA_CommandBlock_ptr;

      /* Command block 1: Complete the MD5 hash using command 0x14 */
      cb_p->read_data_addr_ls =  hashMsgMD5_a;
      cb_p->write_data_addr_ls = resMD5_a;
      memcpy(cb_p->hash_IV, hashMsgMD5_p->iv, sizeof(hashMsgMD5_p->iv));
      cb_p->opcode_iter_length = EA_Cmd_MD5_End_cmdIV;
      cb_p->opcode_iter_length |= (EA_Cmd_Data_Length_Mask & md5Length); 
      convertToBits(cb_p, hashMsgMD5_p, N8_IV);
      cb_p ++;

      /* Command block 2: Complete the SHA1 hash using command 0x24 */
      cb_p->read_data_addr_ls =  hashMsgSHA1_a;
      cb_p->write_data_addr_ls = resSHA1_a;
      cb_p->opcode_iter_length = EA_Cmd_SHA1_End_cmdIV;
      cb_p->opcode_iter_length |= (EA_Cmd_Data_Length_Mask & sha1Length);
      memcpy(cb_p->hash_IV, hashMsgSHA1_p->iv, sizeof(hashMsgSHA1_p->iv));
      convertToBits(cb_p, hashMsgSHA1_p, N8_IV);
      cb_HMAC_p = (EA_HMAC_CMD_BLOCK_t*) (cb_p + 1);

      if (protocol == N8_TLS_FINISH)
      {
         /* At this point we have completed the MD5 and the SHA1 hash.  So
            we should have the label, the MD5 hash and the SHA1 right next to
            each other ready to be used as input for the TLS pseudo random 
            function (PRF). Please see RFC 2246 section 5 for a description of
            the PRF we are implementing. */

         /* According to the RFC, we use half of the key for the MD5 Hash and
            half for the SHA1 hash. If the key length is odd, we use the 
            middle byte in both. So first we find the ceiling of half the
            key length */
         halfKey = CEIL(keyLength, 2);         

         /* The length of the label, MD5 hash and SHA1 hash */
         dataLength = N8_TLS_ROLE_STRING_LENGTH + MD5_HASH_RESULT_LENGTH +
                      SHA1_HASH_RESULT_LENGTH;

         /* This is for the debug printing */
         nCmdBlocks = N8_CB_EA_FINISHTLSHANDSHAKE_NUMCMDS;

         /* Command block 3:  The MD5 portion of the PRF */
         /* set the HMAC key data */

         /* Copy the first halfKey bytes of the key to the command block */
         memset(keyBuffer, 0, sizeof(keyBuffer));
         memcpy(keyBuffer, key_p, halfKey);

         for (i = 0; i < halfKey; i+= 4)
         {
            cb_HMAC_p->hmac_key[i] = BE_to_uint32(keyBuffer + i);
         }

         /* This is the role string plus the MD5 and SHA1 hash results */
         cb_HMAC_p->read_data_addr_ls =  roleStr_a;

         cb_HMAC_p->write_data_addr_ls = resMD5PRF_a;
         cb_HMAC_p->opcode_iter_length = EA_Cmd_MD5_HMAC;
         cb_HMAC_p->opcode_iter_length |= (2 << EA_Cmd_Iterations_Shift);
         cb_HMAC_p->opcode_iter_length |= (EA_Cmd_Data_Length_Mask & dataLength);
         cb_HMAC_p ++;

         /* Command block 4: The SHA1 portion of the PRF.  Uses the same 
            input data as command block 3 */
         /* Copy the second halfKey bytes of the key to the command block */
         /* if the number of bytes was odd, we start the copy a byte earlier */
         /* Note that since the two keys are guaranteed to be the same size
            we don't need to memset the key buffer again */
         if (keyLength % 2)
         {
            /* The key length was odd */
            memcpy(keyBuffer, &(key_p[halfKey - 1]), halfKey);
         }
         else
         {
            memcpy(keyBuffer, &(key_p[halfKey]), halfKey);
         }

         for (i = 0; i < halfKey; i+= 4)
         {
            cb_HMAC_p->hmac_key[i] = BE_to_uint32(keyBuffer + i);
         }

         /* This is STILL the role string plus MD5 and SHA1 hash results */
         cb_HMAC_p->read_data_addr_ls =  roleStr_a;

         cb_HMAC_p->write_data_addr_ls = resSHA1PRF_a;
         cb_HMAC_p->cp_si_context = EA_Cmd_SI_Mask;
         cb_HMAC_p->opcode_iter_length = EA_Cmd_SHA1_HMAC;
         cb_HMAC_p->opcode_iter_length |= (2 << EA_Cmd_Iterations_Shift);
         cb_HMAC_p->opcode_iter_length |= (EA_Cmd_Data_Length_Mask & dataLength);
      }
      else 
      {
         cb_p->cp_si_context = EA_Cmd_SI_Mask;
      }
   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("TLS Handshake End/Cert",
                           (EA_CMD_BLOCK_t *) req_p->EA_CommandBlock_ptr,
                           nCmdBlocks);
   return ret;

}


/*****************************************************************************
 * cb_ea_SSLHandshakeHash
 *****************************************************************************/
/** @ingroup cb_ea
 * @brief Generate the command blocks for the N8_HandshakeHashEnd
 *  N8_SSL_CERT and N8_SSL_FINISH modes.
 *
 * This function generates the command blocks for the N8_SSL_CERT and
 * N8_SSL_FINISH modes. 
 *
 *
 * @param req_p        RO: The API request.
 * @param resMD5_a     RO: Physical address for MD5 result.
 * @param hashMsgMD5_a RO: Physical address for MD5 input.
 * @param md5Length    RO: Length of MD5 input.
 * @param resSHA_a     RO: Physical address for SHA1 result.
 * @param hashMsgSHA_a RO: Physical address for SHA1 input.
 * @param sha1Length   RO: Length of SHA1 input.
 * @param endresMD5_a  RO: Physical address for MD5 SSL result.
 * @param endresSHA1_a RO: Physical address for SHA SSL result.
 * @param outerMsgMD5_a RO: Physical address for the outerMsg.
 * @param outerMsgSHA_a RO: Physical address for the outerMsg.
 *
 * @par Externals:
 *    None.
 *
 * @return
 *    N8_STATUS_OK on success.
 *    N8_INVALID_PARAMETER if the key is too long.
 *    N8_INVALID_OBJECT if one of the inputs is invalid.
 *
 * @par Errors:
 *    See return section.
 *
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *    We are assuming that the command blocks are all memset to zero before
 *    they are passed in to this function.
 *****************************************************************************/

N8_Status_t cb_ea_SSLHandshakeHash(API_Request_t       *req_p,
                                   EA_CMD_BLOCK_t      *cb_p,
                                   N8_HashObject_t     *hObjMD5_p,
                                   uint32_t            innerResult_md5_a,
                                   uint32_t            hashMsgMD5_a,
                                   int                 hashingLength_md5,
                                   N8_HashObject_t     *hObjSHA_p,
                                   uint32_t            innerResult_sha_a,
                                   uint32_t            hashMsgSHA_a,
                                   int                 hashingLength_sha,
                                   uint32_t            endresMD5_a,
                                   uint32_t            endresSHA1_a,
                                   uint32_t            outerMsgMD5_a,
                                   unsigned int        outer_md5Length,
                                   uint32_t            outerMsgSHA1_a,
                                   unsigned int        outer_shaLength)
{
   N8_Status_t      ret = N8_STATUS_OK; /* return status: OK or ERROR */

   DBG(("cb_ea_SSLHandshakeHash\n"));

   do
   {
      /* verify passed parameters */
      CHECK_OBJECT(req_p, ret);
      CHECK_OBJECT(cb_p, ret);
      /* Command block 1: Complete the inner MD5 hash */
      cb_p->read_data_addr_ls = hashMsgMD5_a;
      cb_p->write_data_addr_ls = innerResult_md5_a;
      memcpy(cb_p->hash_IV, hObjMD5_p->iv, sizeof(hObjMD5_p->iv));
      cb_p->opcode_iter_length = EA_Cmd_MD5_End_cmdIV | hashingLength_md5; 
      convertToBits(cb_p, hObjMD5_p, N8_IV);
      cb_p ++;
             
      /* Command block 2: Complete the inner SHA1 hash */
      cb_p->read_data_addr_ls = hashMsgSHA_a;
      cb_p->write_data_addr_ls = innerResult_sha_a;
      memcpy(cb_p->hash_IV, hObjSHA_p->iv, sizeof(hObjSHA_p->iv));
      cb_p->opcode_iter_length = EA_Cmd_SHA1_End_cmdIV | hashingLength_sha;  
      convertToBits(cb_p, hObjSHA_p, N8_IV);
      cb_p ++;

      /* Command block 3: Complete the outer MD5 hash using command 0x10 */

      /* Copy the first halfKey bytes of the key to the command block */
      cb_p->read_data_addr_ls =  outerMsgMD5_a;
      cb_p->write_data_addr_ls = endresMD5_a;
      cb_p->opcode_iter_length = EA_Cmd_MD5 | outer_md5Length;
      cb_p ++;

      /* Command block 4: Complete the outer SHA1 hash using command 0x20 */

      cb_p->read_data_addr_ls =  outerMsgSHA1_a;
      cb_p->write_data_addr_ls = endresSHA1_a;
      cb_p->cp_si_context = EA_Cmd_SI_Mask;
      cb_p->opcode_iter_length = EA_Cmd_SHA1 | outer_shaLength;
   } while (FALSE);

   DBG_PRINT_EA_CMD_BLOCKS("SSL Handshake End",
                           (EA_CMD_BLOCK_t *) req_p->EA_CommandBlock_ptr,
                           N8_CB_EA_SSLSHANDSHAKEHASH_NUMCMDS);
   return ret;

} /* cb_ea_SSLHandshakeHash */





