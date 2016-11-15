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
 * @(#) n8_driver_parms.h 1.46@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_driver_parms.h
 *  @brief NSP2000 Device Driver parameters
 *
 * This file contains all global parameters used in configuring or tuning
 * the NSP2000 Device Driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/15/04 jpw   Change default queue pool size to support one chip. Linux
 *		  and vxworks will properly compute the total number of chips
 * 		  and queue size required. FreeBSD must have this number 
 *		  increased by hand to support more than one chip unless
 * 		  a kernel patch is applied.
 * 10/28/03 pjm   Removed incorrect comment on optimum performance tuning.
 * 05/08/03 brr   Remove VxWorks conditional compile flag & updated comments.
 * 04/22/03 brr   Bump N8_EA_POOL_SIZE to 4MB.
 * 04/21/03 brr   Added defines for size and granularity for multiple memory
 *                regions. Also moved non-configuration constants out of here.
 * 04/01/03 brr   Moved defines needed by user space to n8_sdk_config.
 * 04/01/03 brr   Eliminated N8_ENABLE_CMD_COMPLETE and changed default value
 *                for the AMBA timer.
 * 03/19/03 brr   Renamed N8_USE_AMBA_TIMER to N8_ENABLE_CMD_COMPLETE since
 *                the AMBA will always run with outstanding request and we
 *                conditionally configure the command complete interrupt.
 * 03/03/03 jpw   Changed USE_AMBA_TIMER to N8_USE_AMBA_TIMER (default = 1)
 * 03/10/03 brr   Added support for API callbacks.
 * 12/09/02 brr   Added N8_RNG_UNIT parameter.
 * 11/19/02 brr   Moved several nonconfiguration defines to more appopriate
 *                files and updated commments.
 * 10/10/02 brr   Elimated N8_EVENT_WAIT_TYPE.	
 * 09/10/02 brr   Added optional definition of USE_AMBA_TIMER
 * 06/25/02 brr   Renamed constants used to define the user memory pool.
 * 06/12/02 hml   Reduced size of request bank so BSD machine will boot.
 * 06/10/02 hml   Added parameters for size and granularity of request 
 *                bank and process request pool.
 * 05/02/02 brr   Removed AMBANumWaitBlocks.
 * 03/29/02 brr   Added N8_AMBA_TIMER_CHIP define.
 * 02/26/02 brr   Break circular dependency.
 * 02/18/02 brr   Removed obsolete context memory defines.
 * 02/01/02 bac   Changed the default for DEF_BIGALLOC_POOL_SIZE from 8 to 6.
 * 01/21/02 msz   Config of event wait type
 * 01/15/02 msz   Support of an array of AMBA wait blocks.
 * 01/03/01 brr   Added Bridge timer preset constant.
 * 12/14/01 brr   Support queue resizing on install & changed memory 
 *                allocation granularity.
 * 12/06/01 brr   Added seperate define for RNG queue sizing, moved SKS init
 *                to the driver, and correct context memory detection.
 * 12/05/01 brr   Moved queue initialization to the driver.
 * 11/15/01 mmd   Updated ASIC device/vendor IDs.
 * 11/14/01 mmd   Original version.
 ****************************************************************************/

#ifndef N8_DRIVER_PARMS_H
#define N8_DRIVER_PARMS_H

#include "n8_sdk_config.h"

/*****************************************************************************
 * DRIVER NAME -                                                             *
 * This null-terminated ASCII string is the name by which the driver is      *
 * registered upon being loaded and started.                                 *
 *****************************************************************************/

#define DEF_SIMON_DRIVER_NAME          "nsp2000"


/*****************************************************************************
 * MAJOR/MINOR NUMBERS -                                                     *
 * The default major number is significant mainly for Linux, where 0 allows  *
 * the system to dynamically assign a major number to the driver.            *
 *****************************************************************************/

#define DEF_DEFAULT_MAJOR_NUMBER       0


/*****************************************************************************
 * MEMORY RESOURCES -                                                        *
 * These values dictate the amounts of memory to be allocated for certain    *
 * memory resources required by the hardware - command queues, Context       *
 * Memory, SKS, and RNG parameters.                                          *
 *****************************************************************************/

/* The POOL_SIZE constants determine this size of the contiguous memory region
 * allocated for each NSP2000 core. If zero copy is enabled or used in either the
 * API, openssl, or ipsec, this number must be increased to the total number
 * of MB used to support a number of simultaneous connections or in use 
 * buffers.
*/
#define N8_EA_POOL_SIZE     4096    /* kilobytes of memory in EA pool */
#define N8_EA_GRANULARITY   1024

#define N8_PK_POOL_SIZE      512    /* kilobytes of memory in PK pool */
#define N8_PK_GRANULARITY   1024


/* The N8_QUEUE_POOL_SIZE constant determines this size of the contiguous memory 
 * region allocated for the NSP2000's command queues. By default the 425 value
 * supports a single chip. Linux/VxWorks will automatically multiply the value
 * by the number of chips autodiscovered. FreeBSD can only autodiscover with 
 * a kernel change. To support two chips under FreeBSD - double the 425 value. 
 * If the queue sizes are modifed, the pool size should also be changed.
 */
#define N8_QUEUE_POOL_SIZE      425 /* kilobytes of memory for command queues */
#define N8_QUEUE_GRANULARITY   4096 /* This should ensure page alignment to   */
                                    /* support diagnostic mmap of cmd queues. */

/* The following defines are power of 2 representations of the command queue */
/* lengths. The N8_DEF_CMD_QUE_EXP applies to both the EA & PK command queues */
#define N8_DEF_CMD_QUE_EXP          10
#define N8_DEF_RNG_QUE_EXP          15

#define MAX_API_REQS               128   /* Specifies the depth of the      */
                                         /* circular queue of API requests  */
                                         /* THIS MUST BE A POWER OF 2!!     */


/* User pool banks map chunks of memory into user space for a process to use 
 * Single processes may benefit from this, but multithreaded applications  
 * may be slower - By default USER_POOL_BANKS is 0 which disables user pooling.
 * A non-zero bank number will enable pooling and may require the POOL_SIZE
 * to be increased for each user space allocated pool
*/
#define DEF_USER_POOL_SIZE            1    /* megs allocated for each bank   */
#define DEF_USER_POOL_BANKS           0    /* number of banks allocated      */
#define DEF_USER_GRANULARITY       4096


/*****************************************************************************
 * AMBA TIMER                                                                *
 * The AMBA timer value is computed as:                                      *
 * at a chip clock of 167Mhz the AMBA bus runs at 83Mhz so each tick is 12ns.*
 * A timer preset value of 0x4000 hex = 16384 dec which means that           *
 * 16384 ticks * 12 nanosecond tick length / 1000 = 196 microseconds tick    *
 * interval. An interrupt will be generated every 196 microseconds.          *
 *****************************************************************************/

#define N8_CONTEXT_MEMORY_TIMEOUT   30

#define N8_AMBA_TIMER_PRESET       0x2BF2

/* This constant defines which chip in the system will have the AMBA timer */
/* enabled. If it is set to something other and zero, systems with fewer   */
/* chips than this value will not be operational.                          */

#define N8_AMBA_TIMER_CHIP          0

#endif    /* N8_DRIVER_PARMS_H */

