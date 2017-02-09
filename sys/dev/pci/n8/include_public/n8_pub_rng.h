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
 * @(#) n8_pub_rng.h 1.5@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_rng
 *  @brief Public declarations for random number operations.
 *
 * Public header file for random number operations.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/15/03 brr   Add N8_RNG_MAX_REQUEST.
 * 10/12/01 dkm   Original version. Adapted from n8_rncommon.h.
 ****************************************************************************/
#ifndef N8_PUB_RNG_H
#define N8_PUB_RNG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "n8_pub_common.h"

/* Maximum number of random bytes permitted in a single request. */
#define N8_RNG_MAX_REQUEST         (8 * 1024)

/*
 * Seed source values.
 * These values will be modified to the expected values set by the hardware
 * spec in N8_SetRNGParameters().   
 */
typedef enum
{
   N8_RNG_SEED_INTERNAL,
   N8_RNG_SEED_EXTERNAL,
   N8_RNG_SEED_HOST,
} N8_RNG_Seed_t;


/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/

typedef struct
{

  /* The following are entities in the control status register.  They   */
  /* should always be set.                                              */

   /* Time of day counter enable.  Enables time of day counter to       */
   /* operate.  This should be set to N8_TRUE.                           */
   N8_Boolean_t      todEnable;

   /* Enables use of external clock signal for an external seed source. */
   /* Should be set N8_TRUE only if/when the exteranl seed source is     */
   /* selected.                                                         */
   N8_Boolean_t      use_external_clock;

   /* Data source of seed values.                                       */
   uint32_t       seed_source;

   /* Iteratation count is the number of random 64-bit values generated */
   /* from each key.  The value may range from 1-256, and this value    */
   /* must be in that range.  Note that in the hardware the value       */
   /* stored in the control register is actually 1 less than the value  */
   /* specified here.                                                   */
   unsigned short iteration_count;

   /* Pre-scale value for TOD.  Should be set to the clock frequency    */
   /* of the chip.                                                      */
   uint32_t       TOD_prescale;

   /* Tells whether or not to use the initial_TOD_seconds field below.  */
   /* will be set to N8_FALSE on a get of this value.                    */
   N8_Boolean_t      set_TOD_counter;

   /* DES keys for the X9.17 algorithm.  Will be forced to valid parity */
   /* These should be set to random values.                             */
   uint8_t        key1[N8_DES_KEY_LENGTH];
   uint8_t        key2[N8_DES_KEY_LENGTH];

  /* The following entities may be ignored depending on the values      */
  /* set above.                                                         */

   /* Host seed for X9.17 algorithm                                     */
   /* If seed_source is set to host seed, then the user must specify    */
   /* each seed value used to generate random values.  After each       */
   /* iteration_count number of 64-bit random values have been          */
   /* generated, a new hostSeed value must be supplied in order to      */
   /* continue generating more values.  If seed_source is not set to    */
   /* host seed then this value should not be set and is ignored.       */
   uint8_t        hostSeed[N8_DES_KEY_LENGTH];

   /* Value to initialize the Time Of Day value (seconds).              */
   /* Ignored if set_TOD_counter is N8_FALSE.                            */
   uint32_t       initial_TOD_seconds;

   /* The external clock scaler is the number of systems clock cycles   */
   /* cycles in each external clock cycle.  See the NSP 2000 Data       */
   /* Sheet for details.  This value is only used when seed source is   */
   /* set to external source, and then external clock should also be    */
   /* set to N8_TRUE.  Only the lower 20 bits of this are valid.         */
   uint32_t       externalClockScaler;

  /* The following entities are ignored on a set, and are valid on a    */
  /* get.                                                               */

   /* Host seed valid.  A N8_TRUE indication means that the host seed    */
   /* has not been consumed.  A N8_FALSE indication means that the host  */
   /* seed has been exhausted.  The user should check that this is      */
   /* N8_FALSE before writing a host seed to insure that a previous host */
   /* seed has won't be overwritten.  If this value is N8_FALSE the RNG  */
   /* will eventually stop providing random numbers until a new host    */
   /* seed is provided.                                                 */
   N8_Boolean_t      hostSeedValid;

   /* The RNG core sets this bit (N8_TRUE) when a seed duplication error */
   /* is detected.  When this bit is set, and set_diagnostic_mode is    */
   /* not set (N8_FALSE), the RNG core will be disabled (halted.)        */
   N8_Boolean_t      seedErrorFlag;

   /* The RNG core sets this bit (N8_TRUE) when an X9.17 duplication     */
   /* error is detected. When this bit is set, and set_diagnostic_mode  */
   /* is not set (N8_FALSE), the RNG core will be disabled (halted.)     */
   N8_Boolean_t      x9_17_errorFlag;

   /* This returns the value of the last valid seed produced by the     */
   /* seed generator.                                                   */
   uint32_t       seedValue_ms;
   uint32_t       seedValue_ls;

} N8_RNG_Parameter_t;

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
/*
 * Set Random Number Generator parameters
 */
N8_Status_t N8_SetRNGParameters(N8_RNG_Parameter_t *p);
N8_Status_t N8_GetRNGParameters(N8_RNG_Parameter_t *p);
N8_Status_t N8_GetRandomBytes(int num_bytes, char *buf, N8_Event_t *event_p);

#ifdef __cplusplus
}
#endif

#endif


