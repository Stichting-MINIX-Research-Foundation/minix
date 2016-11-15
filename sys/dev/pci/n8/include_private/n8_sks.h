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
 * @(#) n8_sks.h 1.23@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file SKS_Management_Interface
 *  @brief Header file for the SKS Management Interface library.
 *
 * This file contains all of the needed SKS Management Interface definitions
 * and data types..
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 07/03/02 brr   Define SKS directory for VxWorks since Windows based 
 *                compiler cannot pass the string in correctly.
 * 05/23/02 brr   Moved definition of SKS directory to Makefile.defaults.
 * 05/02/02 brr   Removed all references to the queue structure.
 * 03/15/02 bac   Removed all references to SUPPORTING_BM
 * 01/30/02 bac   Rewrote macros that depend on SUPPORTING_BM to not reference
 *                the behavioral model routines at all.
 * 01/13/02 brr   Revamp to build in 2.1 environment.
 * 11/13/01 mel   Fixed bug #297: register access macros in n8_sks.c will 
 *                only work on the behavioral model 
 *                - added SKS_READ/WRITE definitions
 * 11/02/01 bac   Moved location of SKS_KEY_NODE_PATH.
 * 10/12/01 dkm   Moved public portion to n8_pub_common.h.
 * 10/10/01 jdj   Fixed bug where SKS busy bits were not being polled 
 *                  correctly.
 * 10/01/01 hml   jdj's code review changes, plus some fixes.
 * 06/14/01 jdj   Changed API to reflect new Allocate/Free calls. 
 * 06/04/01 jdj   Original version.
 ****************************************************************************/

#ifndef N8_SKS_H
#define N8_SKS_H

#include "n8_pub_errors.h"
#include "n8_common.h"

#ifdef VX_BUILD
/* This is defined here for VxWorks since the Windows based compiler */
/* will not accept the definition on the command line in the Makefile */
#undef  SKS_KEY_NODE_PATH
#define SKS_KEY_NODE_PATH               "/opt/NetOctave/sks/"
#endif

#define SKS_PROM_MAX_OFFSET             (4096)
#define SKS_PROM_MIN_OFFSET             (0)
#define SKS_WORDS_PER_ALLOC_UNIT        (8)
#define SKS_ALLOC_UNITS_PER_PROM        ( SKS_PROM_MAX_OFFSET / SKS_WORDS_PER_ALLOC_UNIT )

#define SKS_INUSE                       (0x1)
#define SKS_FREE                        (0x0)

/* Length for SKS_*_DATA_LENGTH is in SKS words (32 bits each). */
#define SKS_RSA_DATA_LENGTH(kl)          (( (kl) * 18 ) + 8 )
#define SKS_DSA_DATA_LENGTH(kl)          (( (kl) * 8 ) + 20 )

#define SKS_RSA_P_OFFSET(kl)            ( 0 )
#define SKS_RSA_Q_OFFSET(kl)            ( 2 * (kl))
#define SKS_RSA_DP_OFFSET(kl)           ( 4 * (kl))
#define SKS_RSA_DQ_OFFSET(kl)           ( 6 * (kl))
#define SKS_RSA_RMODP_OFFSET(kl)        ( 8 * (kl))
#define SKS_RSA_RMODQ_OFFSET(kl)        ( 10 * (kl))
#define SKS_RSA_N_OFFSET(kl)            ( 12 * (kl))
#define SKS_RSA_PINV_OFFSET(kl)         ( 16 * (kl))
#define SKS_RSA_CP_OFFSET(kl)           ( 18 * (kl))
#define SKS_RSA_CQ_OFFSET(kl)           (( 18 * (kl)) + 4)

#define SKS_RSA_P_LENGTH(kl)            (( (kl)/2 ) * 4)
#define SKS_RSA_Q_LENGTH(kl)            (( (kl)/2 ) * 4)
#define SKS_RSA_DP_LENGTH(kl)           (( (kl)/2 ) * 4)
#define SKS_RSA_DQ_LENGTH(kl)           (( (kl)/2 ) * 4)
#define SKS_RSA_RMODP_LENGTH(kl)        (( (kl)/2 ) * 4)
#define SKS_RSA_RMODQ_LENGTH(kl)        (( (kl)/2 ) * 4)
#define SKS_RSA_N_LENGTH(kl)            (( kl ) * 4)
#define SKS_RSA_PINV_LENGTH(kl)         (( (kl)/2 ) * 4)
#define SKS_RSA_CP_LENGTH(kl)           (( 1 ) * 4)
#define SKS_RSA_CQ_LENGTH(kl)           (( 1 ) * 4)


#define SKS_DSA_P_OFFSET(kl)            ( 0 )
#define SKS_DSA_GRMODP_OFFSET(kl)       ( 4 * (kl))
#define SKS_DSA_Q_OFFSET(kl)            ( 8 * (kl)) 
#define SKS_DSA_X_OFFSET(kl)            (( 8 * (kl)) + 8) 
#define SKS_DSA_CP_OFFSET(kl)           (( 8 * (kl)) + 16) 

#define SKS_DSA_P_LENGTH(kl)            (( kl ) * 4)
#define SKS_DSA_GRMODP_LENGTH(kl)       (( kl ) * 4)
#define SKS_DSA_Q_LENGTH(kl)            (( 2 ) * 4)
#define SKS_DSA_X_LENGTH(kl)            (( 2 ) * 4)
#define SKS_DSA_CP_LENGTH(kl)           (( 1 ) * 4)

#define SKS_MULTIPLE_OF_16_BYTES    16

/**********************************************************************
 *  SKS_READ_CONTROL 
 **********************************************************************/
/** @brief Read control register
 *
 * @param _NSPinstance_p  RO: Pointer to the NSP Structure for this chip.
 *
 *********************************************************************/
#define SKS_READ_CONTROL(_NSPinstance_p)                                  \
      ( ((NSP2000REGS_t *)(_NSPinstance_p)->NSPregs_p)->pkh_secure_key_storage_control )

/**********************************************************************
 *  SKS_WRITE_CONTROL 
 **********************************************************************/
/** @brief Read control register
 *
 * @param _NSPinstance_p  RO: Pointer to the NSP Structure for this chip.
 *
 *********************************************************************/
/*#define pk_write_sks_control(A)     pksim_write_sks_control(A)*/
#define SKS_WRITE_CONTROL(_NSPinstance_p, A)                              \
      (((NSP2000REGS_t *)(_NSPinstance_p)->NSPregs_p)->pkh_secure_key_storage_control = (A));

/**********************************************************************
 *  SKS_READ_CONTROL 
 **********************************************************************/
/** @brief Read control register
 *
 * @param _NSPinstance_p  RO: Pointer to the NSP Structure for this chip.
 *
 *********************************************************************/
#define SKS_READ_DATA(_NSPinstance_p)                                     \
      (((NSP2000REGS_t *)(_NSPinstance_p)->NSPregs_p)->pkh_secure_key_storage_data )
      
/**********************************************************************
 *  SKS_WRITE_CONTROL 
 **********************************************************************/
/** @brief Read control register
 *
 * @param _NSPinstance_p  RO: Pointer to the NSP Structure for this chip.
 *
 *********************************************************************/
#define SKS_WRITE_DATA(_NSPinstance_p, A)                                 \
      (((NSP2000REGS_t *)(_NSPinstance_p)->NSPregs_p)->pkh_secure_key_storage_data = (A));



/****************************
 *    Function Prototypes   *
 ****************************/
/* These have been moved to n8_sks_proto.h in order to prevent a circular 
   dependency with n8_rsa.h */

#endif
