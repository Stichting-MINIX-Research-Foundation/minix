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
 * @(#) irq.h 1.14@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file irq.h
 *  @brief NSP2000 Device Driver interrupt handling functions
 *
 * This file contains all secondary interrupt handling routines for the
 * NSP2000 Device Driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/15/03 brr   Removed bit definitions which were also defined in each core
 *                specific .h file.
 * 03/18/03 brr   Correct bit definitions in Bridge Status Register.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 10/10/02 brr   Deleted prototype for N8_WaitOnInterrupt.
 * 05/30/02 brr   Added defines for command errors.
 * 04/11/02 brr   Pass parameter to waitOnInterrupt to indicate whether
 *                sleep should be interruptable.
 * 03/08/02 brr   Moved prototype of N8_BlockWithTimeout from helper.h.
 * 01/03/02 brr   Added Bridge timer reload constant.
 * 11/27/01 mmd   Eliminated N8_EnableInterrupts. Renamed from simon_irq.h.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 09/20/01 mmd   Implementation of N8_MainInterruptHandler.
 * 09/07/01 mmd   Cleanup and revision of prototypes.
 * 07/30/01 mmd   Creation.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver
 */


#ifndef IRQ_H
#define IRQ_H

#include "n8_driver_main.h"


/******************************************************************************
 * INTERRUPT HANDLER RESOURCES -                                              *
 *                                                                            *
 * These constants are bitmasks for the AMBA control/status registers, and    *
 * are used by the interrupt handler to determine the cause of an interrupt,  *
 * and to clear the corresponding bit to clear the interrupt.                 *
 ******************************************************************************/

/* AMBA IRQ REGISTER BITMASKS */
#define AMBAIRQ_PKP         0x80000000  /* BIT 31 - PKP IRQ                  */
#define AMBAIRQ_CCH         0x40000000  /* BIT 30 - CCH IRQ                  */
#define AMBAIRQ_RNG         0x20000000  /* BIT 29 - RNG IRQ                  */
#define AMBAIRQ_Bridge      0x10000000  /* BIT 28 - Bridge IRQ               */
#define AMBAIRQ_Timer       0x08000000  /* BIT 27 - Bridge Timer IRQ         */
                                        /* BIT 26:25 - Reserved              */
#define AMBACCH_Enable      0x01000000  /* BIT 24 - Mirror from E/A          */
#define AMBACCH_Busy        0x00800000  /* BIT 23 - Mirror from E/A          */
#define AMBACCH_Error       0x00400000  /* BIT 22 - Mirror from E/A          */
#define AMBACCH_Rd_Pending  0x00200000  /* BIT 21 - Mirror from E/A          */
#define AMBACCH_Wr_Pending  0x00100000  /* BIT 20 - Mirror from E/A          */
#define AMBAPKH_Enable      0x00080000  /* BIT 19 - Mirror from PKH          */
#define AMBAPKH_Busy        0x00040000  /* BIT 18 - Mirror from PKH          */
#define AMBAPKH_Error       0x00020000  /* BIT 17 - Mirror from PKH          */
#define AMBAPKH_SKS_Go_Busy 0x00010000  /* BIT 16 - Mirror from PKH          */
                                        /* BIT 15:14 - Reserved              */
#define AMBAIRQ_HRESP       0x00002000  /* BIT 13 - AHB slave HRESP=ERROR    */
#define AMBAIRQ_HBURST      0x00001000  /* BIT 12 - CCM illegal burst type   */
#define AMBAIRQ_HSIZE       0x00000800  /* BIT 11 - CCM illegal data size    */
#define AMBAIRQ_PCIACC      0x00000400  /* BIT 10 - Non-32 bit access        */
#define AMBAIRQ_RSVMEM      0x00000200  /* BIT  9 - Bridge reserve mem space */
#define AMBAIRQ_TRCV        0x00000100  /* BIT  8 - CS6464AF data parity err */
#define AMBAIRQ_PCIPERR     0x00000080  /* BIT  7 - inSilicon parity error   */
#define AMBA_Timer_Reload   0x00000001  /* BIT  0 - Bridge Timer Reload      */



/* INTERRUPT HANDLER */
void n8_MainInterruptHandler( NspInstance_t   *NSPinstance_p );

/* INTERRUPT NOTIFICATION ROUTINES */
int waitOnInterrupt ( N8_Unit_t        chip,
                      unsigned char    coretype,
                      unsigned long    bitmask,
                      unsigned long    timeout,
                      int              interruptable );

/*****************************************************************************
 * N8_BlockWithTimeout
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Block for notification or specified timeout.
 *
 * This routine abstracts the Linux system call to block until release or
 * timeout.
 *
 * @param *WaitQueue  RO:  Specifies the block element
 * @param  timeout    RO:  Specifies the timeout
 * @param  debug      RO:  Specifies whether to display debug messages
 *
 * @par Externals:
 *    N/A
 *
 * @return
 *    0 = success.
 *    1 = failure
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/
 
unsigned char N8_BlockWithTimeout( wait_queue_head_t *WaitQueue,
                                   unsigned long      timeout,
                                   unsigned char      debug);
void reload_AMBA_timer(void);
void n8_DisplayIRQ(void);

#endif     /* IRQ_H */


