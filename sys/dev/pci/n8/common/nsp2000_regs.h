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
 * @(#) nsp2000_regs.h 1.9@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file nsp2000_regs.h
 *  @brief NSP2000 Control Register Layout
 *
 * This header contains a structure that maps all NSP2000 control registers
 * for the 4 major subcomponents - the AMBA controller, the PKE/PKH,
 * the RNG/RNH, and the CCH/CC.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/16/04 jpw   Add N8_NO_64_BURST to prevent 64bit bursts to 32 bit regs
 *                during sequential slave io accesses. Certain chipsets
 * 		  need this for proper use with the PCI-X board.
 * 03/01/02 brr   Added register definition previously in n8_enqueue_common.h
 * 01/15/02 dkm   Added amba read pointer mirror register and fixed comment.
 *                BUG 413
 * 11/12/01 msz   Made uint32_t's vuint32_t's (volatile).  Fix for BUG 281.
 * 09/07/01 mmd   Further elimination of "simon" except for line 155, which
 *                is there until the rest of the SDK has been updated to use
 *                NSP2000REGS_t instead of SIMON.
 * 08/16/01 mmd   Elimination of "simon" and relocation to driver/common.
 * 08/01/01 mmd   Migration to new common driver framework.
 * 07/25/01 mmd   Now including stdint.h for Linux, to get uint32_t types.
 * 07/19/01 mmd   Cleaned up the #include's at top of file.
 * 06/27/01 jke   changed #ifdef protections on #include bsdi_types.h so file
 *                can be included in both BSDi kernel and user-apps
 * 06/21/01 jke   added #include ../bsdi_types.h to ensure compile-ability in
 *                BSD
 * 06/21/01 mmd   Ensuring inttypes.h doesn't get included when building the
 *                Linux or BSDi drivers. One should, in either of these cases,
 *                define either the LINUXDRIVER or BSDIDRIVER symbol in the
 *                driver's makefile.
 * 06/06/01 mmd   Added missing amba_pci_timer_value register, and all CCH
 *                registers. Also reformatted.
 * 01/03/01 jw    Original version.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Control Register Layout.
 */


#ifndef NSP2000_REGS_H
#define NSP2000_REGS_H

#include "n8_pub_types.h"

typedef volatile uint8_t        vuint8_t;
typedef volatile uint16_t       vuint16_t;
typedef volatile uint32_t       vuint32_t;


#define NSP_CORE_ENABLE        0x80000000
#define NSP_CORE_BUSY          0x40000000

/* This dummy write breaks up sequential register accesses */
/* Sequential accesses may fail on the PCI-X board on some */
/* chipsets if the system attempts to turn two 32 bit      */
/* register accesses into a single 64 byte burst. */
#define N8_NO_64_BURST nsp->amba_pci_test=0xbadbeef;

/****************************************************
 * 64K of total address space                       *
 *                                                  *
 * In PCI BAR0 - Offset from hw pointer:            *
 *      0x0000 - 16K - AMBA-PCI Bridge              *
 *      0x4000 - 16K - Public Key Module            *
 *      0x8000 - 16K - Crypto Control Processor     *
 *      0xC000 - 16K - Random Number Generator      *
 ****************************************************/
#define PKE_REG_OFFSET    0x4000
#define EA_REG_OFFSET     0x8000
#define RNG_REG_OFFSET    0xC000

typedef struct
{
     /* AMBA CONTROLLER REGISTERS */
     vuint32_t amba_pci_endian_mode;          /* 0000 */
     vuint32_t amba_pci_timer_preset;         /* 0004 */
     vuint32_t amba_pci_status;               /* 0008 */
     vuint32_t amba_pci_control;              /* 000C */
     vuint32_t amba_pci_timer_value;          /* 0010 */
     vuint32_t amba_pci_test;                 /* 0014 */
     vuint32_t amba_pci_read_ptr_mirror;      /* 0018 */
     vuint32_t amba_pci_unused[4089];         /* 001C */


     /* PKH AND PKE REGISTERS */
     vuint32_t pkh_test0;                     /* 4000 */
     vuint32_t pkh_test1;                     /* 4004 */
     vuint32_t pkh_q_length;                  /* 4008 */
     vuint32_t pkh_q_ptr;                     /* 400C */
     vuint32_t pkh_q_bar1;                    /* 4010 */
     vuint32_t pkh_q_bar0;                    /* 4014 */
     vuint32_t pkh_intr_enable;               /* 4018 */
     vuint32_t pkh_control_status;            /* 401C */
     vuint32_t pkh_secure_key_storage_data;   /* 4020 */
     vuint32_t pkh_secure_key_storage_control;/* 4024 */
     vuint32_t pkh_reserved[46];              /* 4028 */

     vuint32_t pke_command_block[8];          /* 40E0 */  
     vuint32_t pkh_reserved1[960];            /* 4100 */
     vuint32_t pke_BN_cache[1024];            /* 5000 */  /* 256 "digits" */
                                                          /* of 128 bits  */
     vuint32_t pkh_reserved2[2048];           /* 6000 */


     /* CCH REGISTERS */
     vuint32_t cch_test0;                     /* 8000 */
     vuint32_t cch_test1;                     /* 8004 */
     vuint32_t cch_q_length;                  /* 8008 */
     vuint32_t cch_q_ptr;                     /* 800C */
     vuint32_t cch_q_bar1;                    /* 8010 */
     vuint32_t cch_q_bar0;                    /* 8014 */
     vuint32_t cch_intr_enable;               /* 8018 */
     vuint32_t cch_control_status;            /* 801C */
     vuint32_t cch_context_data1;             /* 8020 */
     vuint32_t cch_context_data0;             /* 8024 */
     vuint32_t cch_context_addr;              /* 8028 */
     vuint32_t cch_reserved[4085];            /* 802C */


     /* RNG REGISTERS - RNG Buffer - RNH REGISTERS */
     /* 64-bit registers in the RNG must be read/written using the Most */
     /* Significant Word first, in order to access the full 64bit value. */
     vuint32_t rng_reserved0;                 /* C000 */
     vuint32_t rng_tod_seconds;               /* C004 */
     vuint32_t rng_reserved1;                 /* C008 */
     vuint32_t rng_tod_prescale;              /* C00C */
     vuint32_t rng_tod_msw;                   /* C010 */
     vuint32_t rng_tod_lsw;                   /* C014 */
     vuint32_t rng_key1_msw;                  /* C018 */
     vuint32_t rng_key1_lsw;                  /* C01C */
     vuint32_t rng_key2_msw;                  /* C020 */
     vuint32_t rng_key2_lsw;                  /* C024 */
     vuint32_t rng_hostseed_msw;              /* C028 */
     vuint32_t rng_hostseed_lsw;              /* C02C */
     vuint32_t rng_reserved2;                 /* C030 */
     vuint32_t rng_sample_interval;           /* C034 */
     vuint32_t rng_reserved3;                 /* C038 */
     vuint32_t rng_external_clock_scalar;     /* C03C */
     vuint32_t rng_reserved4;                 /* C040 */
     vuint32_t rng_buffer_write_ptr;          /* C044 */
     vuint32_t rng_sample_seed_msw;           /* C048 */
     vuint32_t rng_sample_seed_lsw;           /* C04C */
     vuint32_t rng_reserved5;                 /* C050 */
     vuint32_t rng_lsfr_diag;                 /* C054 */
     vuint32_t rng_lsfr1_history_msw;         /* C058 */
     vuint32_t rng_lsfr1_history_lsw;         /* C05C */
     vuint32_t rng_lsfr2_history_msw;         /* C060 */
     vuint32_t rng_lsfr2_history_lsw;         /* C064 */
     vuint32_t rng_lsfr3_history_msw;         /* C068 */
     vuint32_t rng_lsfr3_history_lsw;         /* C06C */
     vuint32_t rng_lsfr4_history_msw;         /* C070 */
     vuint32_t rng_lsfr4_history_lsw;         /* C074 */
     vuint32_t rng_reserved6;                 /* C078 */
     vuint32_t rng_control_status;            /* C07C */
     vuint32_t rng_reserved7[480];            /* C080 */

     vuint32_t rng_buffer[512];               /* This buffer is to be      */
                                              /* interpreted as 256 64-bit */
                                              /* value. Software must      */
                                              /* access MSB first - 0 then */
                                              /* 1.                        */

     vuint32_t rnh_test1;                     /* D000 */
     vuint32_t rnh_test0;                     /* D004 */
     vuint32_t rnh_q_length;                  /* D008 */
     vuint32_t rnh_q_ptr;                     /* D00C */
     vuint32_t rnh_q_bar1;                    /* D010 */
     vuint32_t rnh_q_bar0;                    /* D014 */
     vuint32_t rnh_control_status;            /* D018 */
     vuint32_t rnh_reserved;                  /* D01C */
     vuint32_t rng_unused[3064];              /* D020 */
} NSP2000REGS_t;


typedef NSP2000REGS_t SIMON;

typedef struct
{
     /* Common Registers */
     volatile uint32_t test0;                     /* 0000 */
     volatile uint32_t test1;                     /* 0004 */
     volatile uint32_t q_length;                  /* 0008 */
     volatile uint32_t q_ptr;                     /* 000C */
     volatile uint32_t q_bar1;                    /* 0010 */
     volatile uint32_t q_bar0;                    /* 0014 */
     volatile uint32_t intr_enable;               /* 0018 */
     volatile uint32_t control_status;            /* 001C */
} N8CommonRegs_t;

typedef struct
{
     N8CommonRegs_t commonRegs;
     volatile uint32_t pkh_secure_key_storage_data;   /* 0020 */
     volatile uint32_t pkh_secure_key_storage_control;/* 0024 */
     volatile uint32_t pkh_reserved[46];              /* 0028 */
 
     volatile uint32_t pke_command_block[8];          /* 00E0 */
     volatile uint32_t pkh_reserved1[960];            /* 0100 */
     volatile uint32_t pke_BN_cache[1024];            /* 0000 */  /* 256 "digits" */
                                                          /* of 128 bits  */
} N8PKERegs_t;
 
 
 /* CCH REGISTERS */
typedef struct
{
     N8CommonRegs_t commonRegs;
     volatile uint32_t cch_context_data1;             /* 0020 */
     volatile uint32_t cch_context_data0;             /* 0024 */
     volatile uint32_t cch_context_addr;              /* 0028 */
} N8EARegs_t;


#define N8_SetDebugRegister(x, y)  {x->amba_pci_unused[58] = y;}


#endif /* NSP2000_REGS_H */
