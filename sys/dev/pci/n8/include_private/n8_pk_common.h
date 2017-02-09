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

/*****************************************************************************
 * @(#) n8_pk_common.h 1.17@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pk_common.h
 *  @brief #defs and typedefs used in accessing the Public Key Handler
 *
 * #defs and typedefs used in accessing the Public Key Handler.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/16/03 brr   Added PK_DH_CP_Byte_Length.
 * 05/15/03 brr   Added masks for command & bus errors.
 * 02/20/02 brr   Removed references to the queue structure.
 * 07/30/01 bac   Changed word size macros to take a queue control pointer.
 * 05/30/01 bac   Changed the macro BYTES_TO_PKDIGITS to correctly compute the
 *                number of digits by rounding up. 
 * 05/22/01 dws   Reversed the order of data_addr_ms and data_addr_ls in
 *                PK_LDST_CMD_BLOCK_t.
 * 04/16/01 jke   moved status and return types to n8_enqueue_common.h 
 *                and genericized for use in EA and PK
 * 04/05/01 bac   Added BYTES_TO_PKDIGITS and PKDIGITS_TO_BYTES
 * 03/30/01 dws   Removed the typdef for PK_BIGNUM_DIGIT. 
 * 03/29/01 dws   Removed simulator-specific declarations.
 *                Changed uint32 to uint32_t.
 * 03/28/01 jke   copied from n8_pk.h
 ****************************************************************************/

#ifndef N8_PK_COMMON_H
#define N8_PK_COMMON_H

#include "n8_common.h"

/* Public Key Processor Big Number Cache */
#define PK_Bytes_Per_BigNum_Digit      (SIMON_BITS_PER_DIGIT / 8)

#define BYTES_TO_PKDIGITS(__B)    (((__B) + PK_Bytes_Per_BigNum_Digit - 1) / PK_Bytes_Per_BigNum_Digit)
#define PKDIGITS_TO_BYTES(__D)    ((__D) * PK_Bytes_Per_BigNum_Digit)

/* Public Key Processor command block */
typedef struct {
    uint32_t opcode_si;                        /* word  0      */
    uint32_t r_offset;                         /* word  1      */
    uint32_t m_length_offset;                  /* word  2      */
    uint32_t a_length_offset;                  /* word  3      */
    uint32_t b_length_offset;                  /* word  4      */
    uint32_t c_offset;                         /* word  5      */
    uint32_t reserved[2];                      /* word  6 - 7  */
} PK_CMD_BLOCK_t; 

/* Public Key Processor Load/Store command block */
typedef struct {
    uint32_t opcode_si;                        /* word  0      */
    uint32_t r_offset;                         /* word  1      */
    uint32_t data_addr_ms;                     /* word  2      */
    uint32_t data_addr_ls;                     /* word  3      */
    uint32_t data_length;                      /* word  4      */
    uint32_t reserved[3];                      /* word  5 - 7  */
} PK_LDST_CMD_BLOCK_t; 

/* Public Key Processor RSA & DSA command block */
typedef struct {
    uint32_t opcode_si;                        /* word  0      */
    uint32_t sks;                              /* word  1      */
    uint32_t reserved[6];                      /* word  2 - 7  */
} PK_RSA_CMD_BLOCK_t; 

/* Field mask and shift contants */
#define PK_Cmd_SI_Mask                       0x08000000
#define PK_Cmd_Opcode_Mask                   0xf0000000
#define PK_Cmd_Offset_Mask                   0x000000ff
#define PK_Cmd_Length_Mask                   0x00ff0000
#define PK_Cmd_Length_Shift                  16
#define PK_Cmd_Data_Length_Mask              0x00001fff
#define PK_Cmd_N_Mask                        0x01000000
#define PK_Cmd_N_Shift                       24
#define PK_Cmd_Key_Length_Mask               0x00ff0000
#define PK_Cmd_Key_Length_Shift              16
#define PK_Cmd_SKS_Offset_Mask               0x00000fff

/* Opcode definitions - already shifted */
#define PK_Cmd_A_Mod_M                       0x00000000
#define PK_Cmd_R_Mod_M                       0x10000000
#define PK_Cmd_A_Plus_B_Mod_M                0x20000000
#define PK_Cmd_A_Minus_B_Mod_M               0x30000000
#define PK_Cmd_Minus_A_Mod_M                 0x40000000
#define PK_Cmd_AB_Mod_M                      0x50000000
#define PK_Cmd_Inverse_A_Mod_M               0x60000000
#define PK_Cmd_Exp_G_Mod_M                   0x70000000
#define PK_Cmd_Load_R                        0x80000000
#define PK_Cmd_Store_R                       0x90000000
#define PK_Cmd_RSA_Private_Key_Op            0xa0000000
#define PK_Cmd_DSA_Sign_Op                   0xb0000000
#define PK_Cmd_NOP                           0xf0000000


/* Status values for config/status register */
#define PK_Status_PKH_Enable                 0x80000000
#define PK_Status_PKH_Busy                   0x40000000
#define PK_Status_PKE_Go                     0x20000000
#define PK_Status_PKE_Busy                   0x10000000
#define PK_Status_Line_Number_Mask           0x0f000000

#define PK_Status_Cmd_Complete               0x00020000
#define PK_Status_SKS_Write_Done             0x00010000
#define PK_Status_SKS_Offset_Error           0x00008000
#define PK_Status_Length_Error               0x00004000
#define PK_Status_Opcode_Error               0x00002000
#define PK_Status_Q_Align_Error              0x00001000
#define PK_Status_Read_Data_Error            0x00000800
#define PK_Status_Write_Data_Error           0x00000400
#define PK_Status_Read_Opcode_Error          0x00000200
#define PK_Status_Access_Error               0x00000100
#define PK_Status_Reserved_Error             0x00000080
#define PK_Status_Timer_Error                0x00000040
#define PK_Status_Prime_Error                0x00000020
#define PK_Status_Invalid_B_Error            0x00000010
#define PK_Status_Invalid_A_Error            0x00000008
#define PK_Status_Invalid_M_Error            0x00000004
#define PK_Status_Invalid_R_Error            0x00000002
#define PK_Status_PKE_Opcode_Error           0x00000001

#define PK_Status_Any_Condition_Mask         0x0001ffff
#define PK_Status_Any_Error_Mask             0x00007fff
#define PK_Status_Cmd_Error_Mask             0x0000f1ff
#define PK_Status_Bus_Error_Mask             0x00000e00
#define PK_Status_Halting_Error_Mask         0x00007eff

/* Values for interrupt enables register */
#define PK_Enable_PKH_Enable                 0x80000000
#define PK_Enable_Cmd_Complete_Enable        0x00020000
#define PK_Enable_SKS_Write_Done_Enable      0x00010000
#define PK_Enable_SKS_Offset_Error_Enable    0x00008000
#define PK_Enable_Length_Error_Enable        0x00004000
#define PK_Enable_Opcode_Error_Enable        0x00002000
#define PK_Status_Q_Align_Error_Enable       0x00001000
#define PK_Enable_Read_Data_Error_Enable     0x00000800
#define PK_Enable_Write_Data_Error_Enable    0x00000400
#define PK_Enable_Read_Opcode_Error_Enable   0x00000200
#define PK_Enable_Access_Error_Enable        0x00000100
#define PK_Enable_Reserved_Error_Enable      0x00000080
#define PK_Enable_Timer_Error_Enable         0x00000040
#define PK_Enable_Prime_Error_Enable         0x00000020
#define PK_Enable_Invalid_B_Error_Enable     0x00000010
#define PK_Enable_Invalid_A_Error_Enable     0x00000008
#define PK_Enable_Invalid_M_Error_Enable     0x00000004
#define PK_Enable_Invalid_R_Error_Enable     0x00000002
#define PK_Enable_PKE_Opcode_Error_Enable    0x00000001

#define PK_Enable_All_Enable_Mask            0x0001ffff
#define PK_Enable_Error_Enable_Mask          0x00007fff

/* Values for Secure Key Storage control register */
#define PK_SKS_Go_Busy                       0x80000000
#define PK_SKS_PROM_Error                    0x40000000
#define PK_SKS_Access_Error                  0x20000000
#define PK_SKS_Operation_Mask                0x00003000
#define PK_SKS_From_PROM_Mask                0x00002000
#define PK_SKS_Cache_Only_Mask               0x00001000
#define PK_SKS_Address_Mask                  0x00000fff
#define PK_SKS_Op_Address_Mask               0x00003fff
#define PK_SKS_Any_Error_Mask                0x60000000
#define PK_SKS_Max_Length                    0x00001000

/* RSA & DSA constants */
#define PK_RSA_Min_Key_Length                2
#define PK_RSA_Max_Key_Length                32
#define PK_DSA_Min_Key_Length                2
#define PK_DSA_Max_Key_Length                32

/* Diffie-Hellman constants */
#define PK_DH_CP_Byte_Length                 PK_Bytes_Per_BigNum_Digit

#endif /* N8_PK_COMMON_H */

