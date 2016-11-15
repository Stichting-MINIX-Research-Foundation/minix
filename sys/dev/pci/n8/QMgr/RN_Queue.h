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
 * @(#) RN_Queue.h 1.42@(#)
 *****************************************************************************/

/*****************************************************************************
 * @file RN_Queue.h
 *  @brief Random Number Queue
 *
 * Structures and defines for the Random Number queue
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 03/26/03 brr   Remove unused defines & updated RN_POINTER_MASK macro.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 05/15/02 brr   Removed obsolete prototypes.
 * 04/01/02 msz   Most macros gone.
 * 03/18/02 msz   Don't loop forever on a hardware error.
 * 03/01/02 brr   Moved RNG_ALL_UNITS to n8_rn_common since its used by SAPI.
 * 02/22/02 spm   Converted n8_delay to n8_usleep.
 * 02/19/02 msz   Eliminated some unused macros, and unused #defines
 * 12/15/01 msz   Added RNG_CMD_Q_GET_READ_WRITE_PTR.
 * 12/04/01 mel   Fixed bug #395 : Added BM_includes.h to avoid crash for build
 *                when SUPPORTING_BM is not defined 
 * 10/05/01 msz   Added RNG_ALL_UNITS, RNG_FIRST_UNIT in support for
 *                mulitple RNG execution units.
 * 10/04/01 msz   Added RN_InitParameters
 * 09/25/01 msz   Set size of queue based on RNG_QUEUE_LEN_LOG2
 * 09/17/01 msz   Wait a bit to let some random numbers be generated.  This
 *                prevents a hardware hang in waiting for lack of busy bit.
 * 09/12/01 msz   Added External Clock Scaler Register Access.
 *                Changes for host seed support.  Access to read
 *                seed sample msw and lsw.
 * 08/31/01 msz   Fixed a bug in RNH_WAIT_WHILE_BUSY, added
 *                RN_READ_RNG_CFG_STATUS
 * 08/24/01 msz   Simplified macros a bit.
 * 08/16/01 mmd   Now including n8_driver_api.h instead of simon.h.
 * 08/16/01 msz   Code review changes
 * 08/15/01 brr   Fixed complier error when SUPPORTING_BM is not defined.
 * 08/06/01 msz   All macros now take queue_p rather than simon_p, and are
 *                capitalized, and combined sizeOfRNG_Q with sizeOfQueue.
 *                Deleted some unused macros.
 * 07/19/01 bac   Necessary to include n8_enqueue_common.h for RN_Request_t.
 * 07/18/01 mel   Added n8_OS_intf.h for system specific definitions.
 * 06/27/01 msz   Added rnh_wait_while_busy
 * 06/27/01 hml   Added some debugging to selected macro calls.
 *                added rn_read_rnh_cfg_status macro.
 *                changed rn_write_tod_init to use rng_tod_seconds instead
 *                of rng_tod_msw.
 * 06/21/01 hml   Removed chip parameter from init_rng_q routine.
 * 06/21/01 msz   Added some more documentation on macros.
 * 06/19/01 hml   Correct set of prototypes.
 * 06/13/01 msz   Added _simon_p pointer to macros, redid macros using new
 *                bmodel or fpga/simon scheme.
 * 06/08/01 hml   Added macro set for fpga.
 * 05/10/01 dws   Removed #include of inttypes.h.  This is done implicitly
 *                through sim_rn.h and n8_common.h.
 * 05/03/01 jke   fixed macro bugs for incrementing read-queue-pointer
 * 04/27/01 dkm   Corrected #define RNG_q_Write_Ptr
 * 04/11/01 bac   Removed non-ANSI-compliant comments.
 * 03/1901  jke   Original version.
 ****************************************************************************/

#ifndef RN_QUEUE_H
#define RN_QUEUE_H

#include "QMQueue.h"
#include "n8_rn_common.h"
#include "n8_common.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/


/* This defines what a long time looping wait is. */
/* It is an arbitrarily somewhat large number.    */
#define RNH_BUSY_WAIT_HARDWARE_START_COUNT 100


/**********************************************************************
 * RNG_CMD_Q_GET_READ_WRITE_PTR
 **********************************************************************/
/** @brief Read the value of the read and write q pointers.
 *
 * This next macro combines the getting of the read pointer
 * and the getting of the write pointer.  This avoids having to
 * hit the hardware and bus twice in a row to get the same information.
 *
 * @param _queue_p      RO: Pointer to the Queue Control Structure
 * @param _read_p       WO: The read pointer value.
 * @param _write_p      WO: The write pointer value.
 *
 * @par Externals:
 *  None.
 *
 * @return
 *   _read_p and _write_p values.
 *
 * @par Errors:
 *   none.
 *
 * @par Locks:
 *    This routine requires the hardwareAccessSem.
 *
 * @par Assumptions:
 *
 *********************************************************************/
#define RNG_CMD_Q_GET_READ_WRITE_PTR(_queue_p,_read_p, _write_p)      \
   {                                                                  \
      uint32_t readWrite;                                             \
      readWrite    = (_queue_p)->rngReg_p->rnh_q_ptr;                 \
      *(_read_p)   = (readWrite)     & RN_POINTER_MASK(_queue_p);     \
      *(_write_p)  = (readWrite>>16) & RN_POINTER_MASK(_queue_p);     \
   }



#define RN_POINTER_MASK(_queue_p)                            \
   ((_queue_p)->sizeMask)


#endif                                 /* RN_QUEUE_H */
