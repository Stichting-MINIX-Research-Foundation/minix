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
 * @(#) n8_ea_common.h 1.16@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_ea_common.h
 *  @brief Crypto controller command queue interface.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/15/03 brr   Added masks for command & bus errors.
 * 12/07/01 bac   Fixed NSP2000 Bug #10 -- corrected EA_Ctx_Addr_Address_Mask
 * 07/05/01 dws   Added context index mask values.
 * 06/05/01 mlando Added EA_TLS10_CMD_BLOCK_t.
 * 06/05/01 dws   Changed word 19 of EA_IPSEC_CMD_BLOCK_t to reserved1.
 * 06/04/01 dws   Changed the value of EA_Result_MAC_Mismatch, added a 
 *                constant EA_Result_Pad_Error.
 * 05/15/01 dws   Changed the ARC4 i & j mask and shift count defines to move
 *                i and j to the most-significant 16 bits of word 20 of the 
 *                ARC4 context.
 * 05/14/01 dws   Increased the EA maximum data sizes to 18K for SSL decrypt
 *                and 17K for encrypt.
 * 05/11/01 bac   Added EA_SSL30_DES_MD5_CMD_BLOCK_t and
 *                EA_SSL30_DECRYPT_RESULT_CMD_BLOCK_t 
 * 05/10/01 dws   Byte-swapped MD5 default IVs.
 * 04/16/01 jke   moved enqueue retvals from retval genericized names
 * 04/09/01 dws   Added secret/precompute fields to the EA_ARC4_CTX
 *                structure. These are words 6-13, used for SSL3.0 
 *                record layer operations.
 * 03/30/01 dws   Original version.
 ****************************************************************************/

#ifndef N8_EA_COMMON_H
#define N8_EA_COMMON_H

#include "n8_common.h"

/* Crypto controller generic command block */
typedef struct {
    uint32_t cp_si_context;                    /* word  0      */
    uint32_t opcode_iter_length;               /* word  1      */
    uint32_t read_data_addr_ms;                /* word  2      */
    uint32_t read_data_addr_ls;                /* word  3      */
    uint32_t write_data_addr_ms;               /* word  4      */
    uint32_t write_data_addr_ls;               /* word  5      */
    uint32_t hash_IV[5];                       /* word  6 - 10 */
    uint32_t prev_length_ms;                   /* word 11      */
    uint32_t prev_length_ls;                   /* word 12      */
    uint32_t reserved1[7];                     /* word 13 - 19 */
    uint32_t des_IV_ms;                        /* word 20      */
    uint32_t des_IV_ls;                        /* word 21      */
    uint32_t reserved3[2];                     /* word 22 - 23 */
    uint32_t des_key1_ms;                      /* word 24      */
    uint32_t des_key1_ls;                      /* word 25      */
    uint32_t des_key2_ms;                      /* word 26      */
    uint32_t des_key2_ls;                      /* word 27      */
    uint32_t des_key3_ms;                      /* word 28      */
    uint32_t des_key3_ls;                      /* word 29      */
    uint32_t reserved2[2];                     /* word 30 - 31 */
} EA_CMD_BLOCK_t;

/* Crypto controller SSL3.0 Packet-level command block */
typedef struct {
    uint32_t cp_si_context;                    /* word  0      */
    uint32_t opcode_iter_length;               /* word  1      */
    uint32_t read_data_addr_ms;                /* word  2      */
    uint32_t read_data_addr_ls;                /* word  3      */
    uint32_t write_data_addr_ms;               /* word  4      */
    uint32_t write_data_addr_ls;               /* word  5      */
    uint32_t write_secret[5];                  /* word  6 - 10 */
    uint32_t mac[5];                           /* word 11 - 15 */
    uint32_t sequence_number[2];               /* word 16 - 17 */
    uint32_t result_type;                      /* word 18      */
    uint32_t SSL_version_length;               /* word 19      */
    uint32_t des_IV_ms;                        /* word 20      */
    uint32_t des_IV_ls;                        /* word 21      */
    uint32_t reserved2[2];                     /* word 22 - 23 */
    uint32_t des_key1_ms;                      /* word 24      */
    uint32_t des_key1_ls;                      /* word 25      */
    uint32_t des_key2_ms;                      /* word 26      */
    uint32_t des_key2_ls;                      /* word 27      */
    uint32_t des_key3_ms;                      /* word 28      */
    uint32_t des_key3_ls;                      /* word 29      */
    uint32_t reserved3[2];                     /* word 30 - 31 */
} EA_SSL30_CMD_BLOCK_t;

/* Crypto controller SSL3.0 Packet-level command block */
typedef struct {
   uint32_t cp_si_context;                    /* word  0      */
   uint32_t opcode_iter_length;               /* word  1      */
   uint32_t read_data_addr_ms;                /* word  2      */
   uint32_t read_data_addr_ls;                /* word  3      */
   uint32_t write_data_addr_ms;               /* word  4      */
   uint32_t write_data_addr_ls;               /* word  5      */
   uint32_t precompute1[4];                   /* word  6 - 9  */
   uint32_t reserved1;                        /* word 10      */
   uint32_t precompute2[4];                   /* word 11 - 14 */
   uint32_t reserved2;                        /* word 15      */
   uint32_t sequence_number[2];               /* word 16 - 17 */
   uint32_t result_type;                      /* word 18      */
   uint32_t SSL_version_length;               /* word 19      */
   uint32_t des_IV_ms;                        /* word 20      */
   uint32_t des_IV_ls;                        /* word 21      */
   uint32_t reserved3[2];                     /* word 22 - 23 */
   uint32_t des_key1_ms;                      /* word 24      */
   uint32_t des_key1_ls;                      /* word 25      */
   uint32_t des_key2_ms;                      /* word 26      */
   uint32_t des_key2_ls;                      /* word 27      */
   uint32_t des_key3_ms;                      /* word 28      */
   uint32_t des_key3_ls;                      /* word 29      */
   uint32_t reserved4[2];                     /* word 30 - 31 */
} EA_SSL30_DES_MD5_CMD_BLOCK_t;

/* Crypto controller TLS Packet-level command block */
typedef struct {
   uint32_t cp_si_context;                    /* word  0      */
   uint32_t opcode_iter_length;               /* word  1      */
   uint32_t read_data_addr_ms;                /* word  2      */
   uint32_t read_data_addr_ls;                /* word  3      */
   uint32_t write_data_addr_ms;               /* word  4      */
   uint32_t write_data_addr_ls;               /* word  5      */
   uint32_t ipad[5];                          /* word  6 - 10 */
   uint32_t opad[5];                          /* word 11 - 15 */
   uint32_t sequence_number[2];               /* word 16 - 17 */
   uint32_t result_type;                      /* word 18      */
   uint32_t SSL_version_length;               /* word 19      */
   uint32_t des_IV_ms;                        /* word 20      */
   uint32_t des_IV_ls;                        /* word 21      */
   uint32_t reserved3[2];                     /* word 22 - 23 */
   uint32_t des_key1_ms;                      /* word 24      */
   uint32_t des_key1_ls;                      /* word 25      */
   uint32_t des_key2_ms;                      /* word 26      */
   uint32_t des_key2_ls;                      /* word 27      */
   uint32_t des_key3_ms;                      /* word 28      */
   uint32_t des_key3_ls;                      /* word 29      */
   uint32_t reserved4[2];                     /* word 30 - 31 */
} EA_TLS10_CMD_BLOCK_t;

/* Crypto controller SSL Decrypt results write-back */
typedef struct {
   uint32_t reserved1[11];                    /* word   0 - 10 */
   uint32_t mac[4];                           /* word  11 - 14 */
   uint32_t reserved2;                        /* word  15      */
   uint32_t sequence_number[2];               /* word  16 - 17 */
   uint32_t mismatch;                         /* word  18      */
   uint32_t reserved3[13];                    /* word  19 - 31 */
} EA_SSL30_DECRYPT_RESULT_CMD_BLOCK_t;

/* Crypto controller IPSec ESP Packet-level command block */
typedef struct {
    uint32_t cp_si_context;                    /* word  0      */
    uint32_t opcode_iter_length;               /* word  1      */
    uint32_t read_data_addr_ms;                /* word  2      */
    uint32_t read_data_addr_ls;                /* word  3      */
    uint32_t write_data_addr_ms;               /* word  4      */
    uint32_t write_data_addr_ls;               /* word  5      */
    uint32_t ipad[5];                          /* word  6 - 10 */
    uint32_t opad[5];                          /* word 11 - 15 */
    uint32_t SPI;                              /* word 16      */
    uint32_t sequence_number;                  /* word 17      */
    uint32_t result;                           /* word 18      */
    uint32_t reserved1;                        /* word 19      */
    uint32_t des_IV_ms;                        /* word 20      */
    uint32_t des_IV_ls;                        /* word 21      */
    uint32_t reserved2[2];                     /* word 22 - 23 */
    uint32_t des_key1_ms;                      /* word 24      */
    uint32_t des_key1_ls;                      /* word 25      */
    uint32_t des_key2_ms;                      /* word 26      */
    uint32_t des_key2_ls;                      /* word 27      */
    uint32_t des_key3_ms;                      /* word 28      */
    uint32_t des_key3_ls;                      /* word 29      */
    uint32_t reserved3[2];                     /* word 30 - 31 */
} EA_IPSEC_CMD_BLOCK_t;

/* Crypto controller HMAC command block */
typedef struct {
    uint32_t cp_si_context;                    /* word  0      */
    uint32_t opcode_iter_length;               /* word  1      */
    uint32_t read_data_addr_ms;                /* word  2      */
    uint32_t read_data_addr_ls;                /* word  3      */
    uint32_t write_data_addr_ms;               /* word  4      */
    uint32_t write_data_addr_ls;               /* word  5      */
    uint32_t hmac_key[16];                     /* word  6 - 21 */
    uint32_t reserved[10];                     /* word 22 - 31 */
} EA_HMAC_CMD_BLOCK_t;


/* Crypto controller Master Secret Hash command block */
typedef struct {
    uint32_t cp_si_context;                    /* word  0      */
    uint32_t opcode_iter_length;               /* word  1      */
    uint32_t read_data_addr_ms;                /* word  2      */
    uint32_t read_data_addr_ls;                /* word  3      */
    uint32_t write_data_addr_ms;               /* word  4      */
    uint32_t write_data_addr_ls;               /* word  5      */
    uint32_t random1[8];                       /* word  6 - 13 */
    uint32_t random2[8];                       /* word 14 - 21 */
    uint32_t reserved[12];                     /* word 22 - 31 */
} EA_MSH_CMD_BLOCK_t;

/* Field mask and shift contants */
#define EA_Cmd_CP_Mask                       0x80000000
#define EA_Cmd_SI_Mask                       0x40000000
#define EA_Cmd_Context_Index_Mask            0x0003ffff
#define EA_Context_Index_Mask_256            0x0003ffff
#define EA_Context_Index_Mask_128            0x0001ffff
#define EA_Context_Index_Mask_64             0x0000ffff

#define EA_Cmd_Opcode_Mask                   0xff000000
#define EA_Cmd_Iterations_Shift              16
#define EA_Cmd_Iterations_Mask               0x000f0000
#define EA_Cmd_Data_Length_Mask              0x0000ffff

#define EA_Cmd_SSL_Type_Mask                 0x000000ff
#define EA_Cmd_SSL_Version_Mask              0xffff0000
#define EA_Cmd_SSL_Version_Shift             16
#define EA_Cmd_SSL_Length_Mask               0x0000ffff

#define EA_Cmd_Result_Mask                   0xf0000000

/* Result definitions */
#define EA_Result_MAC_Mismatch               0x80000000
#define EA_Result_Pad_Error                  0x40000000
#define EA_IPSec_MAC_Mismatch                0x80000000

/* Opcode definitions - already shifted */
#define EA_Cmd_NOP                           0x00000000
#define EA_Cmd_Write_Context_Memory          0x01000000
#define EA_Cmd_Read_Context_Memory           0x02000000

#define EA_Cmd_3DES_CBC_Encrypt              0x03000000
#define EA_Cmd_3DES_CBC_Decrypt              0x04000000
#define EA_Cmd_ARC4                          0x05000000

#define EA_Cmd_MD5                           0x10000000
#define EA_Cmd_MD5_SSL30_Finish              0x11000000
#define EA_Cmd_MD5_Mid_cmdIV                 0x12000000
#define EA_Cmd_MD5_Mid_lhrIV                 0x13000000
#define EA_Cmd_MD5_End_cmdIV                 0x14000000
#define EA_Cmd_MD5_End_lhrIV                 0x15000000
#define EA_Cmd_MD5_IPSEC_KEYMAT              0x16000000
#define EA_Cmd_MD5_IPSEC_SKEYID              0x17000000
#define EA_Cmd_MD5_HMAC                      0x18000000

#define EA_Cmd_SHA1                          0x20000000
#define EA_Cmd_SHA1_SSL30_Finish             0x21000000
#define EA_Cmd_SHA1_Mid_cmdIV                0x22000000
#define EA_Cmd_SHA1_Mid_lhrIV                0x23000000
#define EA_Cmd_SHA1_End_cmdIV                0x24000000
#define EA_Cmd_SHA1_End_lhrIV                0x25000000
#define EA_Cmd_SHA1_IPSEC_KEYMAT             0x26000000
#define EA_Cmd_SHA1_IPSEC_SKEYID             0x27000000
#define EA_Cmd_SHA1_HMAC                     0x28000000

#define EA_Cmd_SSL30_Master_Secret_Hash      0x30000000

/* Packet level commands */
#define EA_Cmd_Null_Null                     0x80000000
#define EA_Cmd_SSL30_ARC4_MD5_Encrypt        0x81000000
#define EA_Cmd_SSL30_3DES_MD5_Encrypt        0x82000000
#define EA_Cmd_SSL30_ARC4_SHA1_Encrypt       0x83000000
#define EA_Cmd_SSL30_3DES_SHA1_Encrypt       0x84000000
#define EA_Cmd_SSL30_ARC4_MD5_Decrypt        0x91000000
#define EA_Cmd_SSL30_3DES_MD5_Decrypt        0x92000000
#define EA_Cmd_SSL30_ARC4_SHA1_Decrypt       0x93000000
#define EA_Cmd_SSL30_3DES_SHA1_Decrypt       0x94000000

#define EA_Cmd_TLS10_ARC4_MD5_Encrypt        0xa1000000
#define EA_Cmd_TLS10_3DES_MD5_Encrypt        0xa2000000
#define EA_Cmd_TLS10_ARC4_SHA1_Encrypt       0xa3000000
#define EA_Cmd_TLS10_3DES_SHA1_Encrypt       0xa4000000
#define EA_Cmd_TLS10_ARC4_MD5_Decrypt        0xb1000000
#define EA_Cmd_TLS10_3DES_MD5_Decrypt        0xb2000000
#define EA_Cmd_TLS10_ARC4_SHA1_Decrypt       0xb3000000
#define EA_Cmd_TLS10_3DES_SHA1_Decrypt       0xb4000000

#define EA_Cmd_ESP_3DES_MD5_Encrypt          0xc1000000
#define EA_Cmd_ESP_3DES_SHA1_Encrypt         0xc2000000
#define EA_Cmd_ESP_3DES_MD5_Decrypt          0xd1000000
#define EA_Cmd_ESP_3DES_SHA1_Decrypt         0xd2000000

/* Assorted field length constants */
#define EA_Sequence_Number_Length            8
#define EA_Protocol_Length                   1
#define EA_Version_Length                    2
#define EA_Packet_Length_Length              2

#define EA_ESP_Key_Length                    16
#define EA_SPI_Length                        4
#define EA_ESP_Sequence_Number_Length        4
#define EA_DES_IV_Length                     8
#define EA_Random_Length                     32

#define EA_HMAC_Key_Length                   64
#define EA_MD5_Hash_Length                   16
#define EA_MD5_Block_Length                  64
#define EA_MD5_Block_Length_Mask             0x3f
#define EA_MD5_Block_Bit_Length_Mask         0x1ff
#define EA_SHA1_Hash_Length                  20
#define EA_SHA1_Block_Length                 64
#define EA_SHA1_Block_Length_Mask            0x3f
#define EA_SHA1_Block_Bit_Length_Mask        0x1ff

#define EA_DES_Block_Length                  8
#define EA_DES_Block_Length_Mask             0x07
#define EA_Max_Data_Length                   0x4800
#define EA_SSL_Encrypt_Max_Data_Length       0x4400
#define EA_SSL_Decrypt_Max_Data_Length       0x4800
#define EA_SSL_ARC4_MD5_Decrypt_Min_Length   16
#define EA_SSL_DES_MD5_Decrypt_Min_Length    24
#define EA_SSL_ARC4_SHA1_Decrypt_Min_Length  20
#define EA_SSL_DES_SHA1_Decrypt_Min_Length   24

#define EA_ESP_Encrypt_Min_Data_Length       8
#define EA_ESP_Decrypt_Min_Data_Length       20

#define EA_IPSEC_ESP_MAC_Length              12

/* default hash IVs */
#define EA_MD5_IV_0                          0x01234567
#define EA_MD5_IV_1                          0x89abcdef
#define EA_MD5_IV_2                          0xfedcba98
#define EA_MD5_IV_3                          0x76543210

#define EA_SHA1_IV_0                         0x67452301
#define EA_SHA1_IV_1                         0xefcdab89
#define EA_SHA1_IV_2                         0x98badcfe
#define EA_SHA1_IV_3                         0x10325476
#define EA_SHA1_IV_4                         0xc3d2e1f0


/* Context memory constants */
#define EA_CTX_Record_Word_Length            128
#define EA_CTX_Record_Byte_Length            512

/* ARC4 context format */
typedef struct {
   uint32_t reserved1[6];                      /* word  0 - 5   */
   uint32_t secret1[5];                        /* word  6 - 10  */
   uint32_t secret2[5];                        /* word 11 - 15  */
   uint32_t sequence_number[2];                /* word 16 - 17  */
   uint32_t reserved3[2];                      /* word 18 - 19  */
   uint32_t i_j;                               /* word 20       */
   uint32_t s_box[64];                         /* word 21 - 84  */
   uint32_t reserved4[43];                     /* word 85 - 127 */
} EA_ARC4_CTX;

#define EA_CTX_I_Mask                        0xff000000
#define EA_CTX_I_Shift                       24
#define EA_CTX_J_Mask                        0x00ff0000
#define EA_CTX_J_Shift                       16

/* SSL 3.0 & TLS 1.0 packet level context format */
typedef struct {
   uint32_t reserved1[6];                      /* word  0 - 5   */
   uint32_t secret1[5];                        /* word  6 - 10  */
   uint32_t secret2[5];                        /* word 11 - 15  */
   uint32_t sequence_number[2];                /* word 16 - 17  */
   uint32_t reserved3[2];                      /* word 18 - 19  */
   uint32_t des_IV_ms;                         /* word 20       */
   uint32_t des_IV_ls;                         /* word 21       */
   uint32_t reserved4[2];                      /* word 22 - 23  */
   uint32_t des_key1_ms;                       /* word 24       */
   uint32_t des_key1_ls;                       /* word 25       */
   uint32_t des_key2_ms;                       /* word 26       */
   uint32_t des_key2_ls;                       /* word 27       */
   uint32_t des_key3_ms;                       /* word 28       */
   uint32_t des_key3_ls;                       /* word 29       */
   uint32_t reserved5[98];                     /* word 30 - 127 */
} EA_SSL30_CTX;

/* IPSec ESP context format */
typedef struct {
   uint32_t reserved1[6];                      /* word  0 - 5   */
   uint32_t ipad[5];                           /* word  6 - 10  */
   uint32_t opad[5];                           /* word 11 - 15  */
   uint32_t reserved2[8];                      /* word 16 - 23  */
   uint32_t des_key1_ms;                       /* word 24       */
   uint32_t des_key1_ls;                       /* word 25       */
   uint32_t des_key2_ms;                       /* word 26       */
   uint32_t des_key2_ls;                       /* word 27       */
   uint32_t des_key3_ms;                       /* word 28       */
   uint32_t des_key3_ls;                       /* word 29       */
   uint32_t reserved3[98];                     /* word 30 - 127 */
} EA_IPSEC_CTX;

/* Status values for config/status register */
#define EA_Status_Module_Enable              0x80000000
#define EA_Status_Module_Busy                0x40000000

#define EA_Status_Q_Align_Error              0x00008000
#define EA_Status_Cmd_Complete               0x00004000
#define EA_Status_Opcode_Error               0x00002000
#define EA_Status_CMD_Read_Error             0x00001000
#define EA_Status_CMD_Write_Error            0x00000800
#define EA_Status_Data_Read_Error            0x00000400
#define EA_Status_Data_Write_Error           0x00000200
#define EA_Status_EA_Length_Error            0x00000100
#define EA_Status_Data_Length_Error          0x00000080
#define EA_Status_EA_DES_Error               0x00000040
#define EA_Status_DES_Size_Error             0x00000020
#define EA_Status_DES_Parity_Error           0x00000010
#define EA_Status_ARC4_Error                 0x00000008
#define EA_Status_MD5_Error                  0x00000004
#define EA_Status_SHA1_Error                 0x00000002
#define EA_Status_Access_Error               0x00000001

#define EA_Status_Any_Condition_Mask         0x0000ffff
#define EA_Status_Any_Error_Mask             0x0000bfff
#define EA_Status_Cmd_Error_Mask             0x000021ff
#define EA_Status_Bus_Error_Mask             0x00001e00
#define EA_Status_Halting_Error_Mask         0x0000bffe

#define EA_Enable_Module_Enable              0x80000000
#define EA_Enable_Q_Align_Error_Enable       0x00008000
#define EA_Enable_Cmd_Complete_Enable        0x00004000
#define EA_Enable_Opcode_Error_Enable        0x00002000
#define EA_Enable_CMD_Read_Error_Enable      0x00001000
#define EA_Enable_CMD_Write_Error_Enable     0x00000800
#define EA_Enable_Data_Read_Error_Enable     0x00000400
#define EA_Enable_Data_Write_Error_Enable    0x00000200
#define EA_Enable_EA_Length_Error_Enable     0x00000100
#define EA_Enable_Data_Length_Error_Enable   0x00000080
#define EA_Enable_EA_DES_Error_Enable        0x00000040
#define EA_Enable_DES_Size_Error_Enable      0x00000020
#define EA_Enable_DES_Parity_Error_Enable    0x00000010
#define EA_Enable_ARC4_Error_Enable          0x00000008
#define EA_Enable_MD5_Error_Enable           0x00000004
#define EA_Enable_SHA1_Error_Enable          0x00000002
#define EA_Enable_Access_Error_Enable        0x00000001

#define EA_Enable_All_Enable_Mask            0x0000ffff
#define EA_Enable_Error_Enable_Mask          0x0000bfff

/* Context Address register constants */
#define EA_Ctx_Addr_Read_Pending             0x80000000
#define EA_Ctx_Addr_Write_Pending            0x40000000
#define EA_Ctx_Addr_Address_Mask             0x00ffffff

#endif

