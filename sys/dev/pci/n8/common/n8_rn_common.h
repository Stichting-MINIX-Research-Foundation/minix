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
 * @(#) n8_rn_common.h 1.23@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_rn_common.h
 *  @brief Random number functionality
 *
 * Common random number functions.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/15/03 brr   Added definition for DEF_RNH_BIGERROR_IRQ_BITS.
 * 11/25/02 brr   Removed definition of RNG_ALL_UNITS.
 * 03/27/02 hml   Removed obsolete include file
 * 03/01/02 brr   Moved RNG_ALL_UNITS to this file since its used by SAPI.
 * 10/23/01 dkm   Moved seed defines to n8_pub_rng.h.
 * 10/12/01 dkm   Moved public portion to n8_pub_rng.h.
 * 09/17/01 bac   Removed buffer_lenLog2 from N8_RNG_Parameter_t.
 * 09/17/01 bac   Removed buffer_source from N8_RNG_Parameter_t.
 * 09/14/01 msz   Fixed circular dependency.
 * 09/14/01 bac   Changed seed source values from enums to #defines so that they
 *                cannot inadvertantly change again.  Removed N8_RNG_BufferSrc_t
 *                as it is no longer needed.
 * 09/07/01 msz   Changes to support host seed, and cleanup work
 * 09/06/01 bac   Renumbered enums to start with non-zero (BUG #190).
 * 07/19/01 bac   Moved RN_Request_t to n8_enqueue_common.h.
 * 06/28/01 bac   Added userBuffer_p and numBytesRequested to RN_Request_t in
 *                order to facilitate the callback function.
 * 06/21/01 msz   Added RN_QUEUE_INIT_OPEN_FAILED
 * 06/19/01 hml   Added requestStatus, numElementsProvided, unit and chip to
 *                the RN_Request_t.  Moved protos to RN_queue.h.
 * 05/07/01 bac   Added RNG_Status_Seed_Shift.
 * 04/24/01 dws   Fixed bracket style for RNG_Sample_t.
 * 04/11/01 bac   Standardization by changing type names to include _t.
 * 04/09/01 jke   Original version.
 ****************************************************************************/

#ifndef N8_RN_COMMON_H
#define N8_RN_COMMON_H

#include "n8_common.h"
#include "n8_pub_rng.h"

typedef struct 
{
   uint32_t high;
   uint32_t low;
} RNG_Sample_t;

/* RNG control/status register values */
#define RNG_Status_RNG_Enable                   0x80000000
#define RNG_Status_TOD_Enable                   0x40000000
#define RNG_Status_Ext_Clock_Enable             0x20000000
#define RNG_Status_Diag_Enable                  0x10000000
#define RNG_Status_Buffer_Use_Host3             0x0c000000
#define RNG_Status_Buffer_Use_Host2             0x08000000
#define RNG_Status_Buffer_Use_Seed_Generator    0x04000000
#define RNG_Status_Buffer_Use_X917              0x00000000
#define RNG_Status_Buffer_Mask                  0x0c000000
#define RNG_Status_Seed_Use_Host3               0x03000000
#define RNG_Status_Seed_Use_Host2               0x02000000
#define RNG_Status_Seed_Use_External            0x01000000
#define RNG_Status_Seed_Use_Internal            0x00000000
#define RNG_Status_Seed_Mask                    0x03000000
#define RNG_Status_Seed_Shift                   24
#define RNG_Status_Seed_Error                   0x00008000
#define RNG_Status_X917_Error                   0x00004000
#define RNG_Status_Key1_Parity_Error            0x00002000
#define RNG_Status_Key2_Parity_Error            0x00001000
#define RNG_Status_Host_Seed_Valid              0x00000400
#define RNG_Status_Buffer_Write_Ready           0x00000200
#define RNG_Status_Iteration_Count_Mask         0x000000ff

#define RNG_Status_Writeable_Mask               0xff0000ff
#define RNG_Status_Any_Condition_Mask           0x00f00000

/* RNH control/status register values */
#define RNH_Status_Transfer_Enable              0x80000000
#define RNH_Status_Transfer_Busy                0x40000000
#define RNH_Status_Key_Error                    0x00800000 /* Parity error on loaded key */
#define RNH_Status_Bus_Error                    0x00400000 /* PCI bus mastering error    */
#define RNH_Status_Duplicate_Error              0x00200000 /* Duplicate successive seeds */
#define RNH_Status_Access_Error                 0x00100000 /* Mem access error or busy   */
#define RNH_Status_Key_Error_Enable             0x00080000
#define RNH_Status_Bus_Error_Enable             0x00040000
#define RNH_Status_Duplicate_Error_Enable       0x00020000
#define RNH_Status_Access_Error_Enable          0x00010000

#define RNH_Status_Any_Condition_Mask           0x00f00000
#define RNH_Status_All_Enable_Mask              0x000f0000
#define RNH_Status_Enables_Shift                8
#define RNH_Status_Halting_Error_Mask           0x00f00000
#define DEF_RNH_BIGERROR_IRQ_BITS               0x00b00000


/* RNG REGISTERS - RNG Buffer - RNH REGISTERS */
/* 64-bit registers in the RNG must be read/written using the Most */
/* Significant Word first, in order to access the full 64bit value. */

typedef struct
{
     volatile uint32_t rng_reserved0;                 /* C000 */
     volatile uint32_t rng_tod_seconds;               /* C004 */
     volatile uint32_t rng_reserved1;                 /* C008 */
     volatile uint32_t rng_tod_prescale;              /* C00C */
     volatile uint32_t rng_tod_msw;                   /* C010 */
     volatile uint32_t rng_tod_lsw;                   /* C014 */
     volatile uint32_t rng_key1_msw;                  /* C018 */
     volatile uint32_t rng_key1_lsw;                  /* C01C */
     volatile uint32_t rng_key2_msw;                  /* C020 */
     volatile uint32_t rng_key2_lsw;                  /* C024 */
     volatile uint32_t rng_hostseed_msw;              /* C028 */
     volatile uint32_t rng_hostseed_lsw;              /* C02C */
     volatile uint32_t rng_reserved2;                 /* C030 */
     volatile uint32_t rng_sample_interval;           /* C034 */
     volatile uint32_t rng_reserved3;                 /* C038 */
     volatile uint32_t rng_external_clock_scalar;     /* C03C */
     volatile uint32_t rng_reserved4;                 /* C040 */
     volatile uint32_t rng_buffer_write_ptr;          /* C044 */
     volatile uint32_t rng_sample_seed_msw;           /* C048 */
     volatile uint32_t rng_sample_seed_lsw;           /* C04C */
     volatile uint32_t rng_reserved5;                 /* C050 */
     volatile uint32_t rng_lsfr_diag;                 /* C054 */
     volatile uint32_t rng_lsfr1_history_msw;         /* C058 */
     volatile uint32_t rng_lsfr1_history_lsw;         /* C05C */
     volatile uint32_t rng_lsfr2_history_msw;         /* C060 */
     volatile uint32_t rng_lsfr2_history_lsw;         /* C064 */
     volatile uint32_t rng_lsfr3_history_msw;         /* C068 */
     volatile uint32_t rng_lsfr3_history_lsw;         /* C06C */
     volatile uint32_t rng_lsfr4_history_msw;         /* C070 */
     volatile uint32_t rng_lsfr4_history_lsw;         /* C074 */
     volatile uint32_t rng_reserved6;                 /* C078 */
     volatile uint32_t rng_control_status;            /* C07C */
     volatile uint32_t rng_reserved7[480];            /* C080 */

     volatile uint32_t rng_buffer[512];       /* This buffer is to be      */
                                              /* interpreted as 256 64-bit */
                                              /* value. Software must      */
                                              /* access MSB first - 0 then */
                                              /* 1.                        */

     volatile uint32_t rnh_test1;                     /* D000 */
     volatile uint32_t rnh_test0;                     /* D004 */
     volatile uint32_t rnh_q_length;                  /* D008 */
     volatile uint32_t rnh_q_ptr;                     /* D00C */
     volatile uint32_t rnh_q_bar1;                    /* D010 */
     volatile uint32_t rnh_q_bar0;                    /* D014 */
     volatile uint32_t rnh_control_status;            /* D018 */
     volatile uint32_t rnh_reserved;                  /* D01C */
     volatile uint32_t rng_unused[3064];              /* D020 */
} N8RNGRegs_t;


#endif
