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
 * @(#) nsp_ioctl.h 1.34@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file nsp_ioctl.h
 *  @brief NSP2000 Device Driver IOCtl handler
 *
 * This header contains the prototype of the common IOCtl handler.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 04/21/03 brr   Added support for multiple memory banks.
 * 03/20/03 brr   Added debug message selector for queue manager information.
 * 09/18/02 brr   Added IOCTL codes for diagnostics & wait on request complete.
 * 07/17/02 brr   Eliminate unused ioctl codes.
 * 07/02/02 brr   Added IOCTL code for memory statistics.
 * 05/30/02 brr   Moved queue stat structure to n8_enqueue_common & removed
 *                #defines for obsolete ioctl's.
 * 05/20/02 brr   Added stat for preempted requests.
 * 05/15/02 brr   Removed obsolete Ioctl code NSP_IOCTL_QMGR_CHECK_REQ.
 * 04/03/02 spm   Removed remnants of the reverse-ioctl that the daemon
 *                formerly used (start, finish, shutdown).
 * 03/29/02 brr   Removed obsolete defines & strutures.
 * 03/29/02 hml   Added ioctl N8_IOCTL_VALIDATE_CONTEXT.
 * 03/25/02 hml   Deleted obsolete debug bits.
 * 03/22/02 hml   Added new Debug bit for displaying context.
 * 03/21/02 mmd   Added N8_QueueStatistics_t, NSP_IOCTL_QUERY_QUEUE_STATS,
 *                and N8_DBG_QUERY_QUEUE_STATS. 
 * 02/18/02 brr   Added new IOCTL's for QMgr functions.
 * 01/22/02 spm   Added sys init ioctl for handling inits that need
 *                the N8 userspace daemon to be connected to the driver.
 * 01/17/02 spm   Added shutdown ioctl for handling shutdown requests
 *                from userspace (uninstall script).  Also added
 *                test ioctls for testing N8 daemon capabilities.
 * 01/15/02 spm   Added NSP_IOCTL_N8_DAEMON_START, NSP_IOCTL_N8_DAEMON_FINISH
 *                ioctls for N8 daemon support.
 * 01/02/02 hml   Modified N8_API_Parms_t structure.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 10/16/01 mmd   Revised debug info to a single parm - debug.
 * 09/25/01 mmd   Eliminated FPGAflag parameter.
 * 09/19/01 mmd   Creation.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver IOCtl handler
 */

#ifndef NSP_IOCTL_H
#define NSP_IOCTL_H


#include "n8_device_info.h"
#include "n8_pub_common.h"


/******************************************************************************
 * IOCTL HANDLER RESOURCES -                                                  *
 *                                                                            *
 * ParmStruct is a basic data structure used for passing paramters and        *
 * results between a user process and the ioctl handler on an ioctl call.     *
 * Each supported IOCTL code uses a different combination of the fields, so   *
 * the fields don't necessarily have any intrinsic significance.              *
 *                                                                            *
 * Three additional parameter structures are used in the same way, for some   *
 * of the legacy routines appropriated from Alpha-Data's sample driver, and   *
 * are only used in FPGA mode.                                                *
 *                                                                            *
 * The supported IOCTL codes are defined here as constants. There's no        *
 * particular significance to their actual values, beyond each constant being *
 * unique.                                                                    *
 *                                                                            *
 ******************************************************************************/
 
 
/* GENERIC IOCtl PARAMETER STRUCTURE */
typedef struct
{
     unsigned char    coretype;
     unsigned long    bitmask;
     unsigned long    timeout;
     N8_Unit_t        chip;
     unsigned char    invalid_handle;
     unsigned long    contextIndex;
     unsigned long    irqstatus;
} PARMSTRUCT_t;


/* IOCtl COMMAND CODES */
#define NSP_IOCTL_GET_DEVICE_RESOURCES 21
#define NSP_IOCTL_ALLOCATE_USER_POOL   26
#define NSP_IOCTL_ALLOCATE_BUFFER      27
#define NSP_IOCTL_FREE_BUFFER          28
#define NSP_IOCTL_MEMORY_STATS         30
#define NSP_IOCTL_WAIT_ON_INTERRUPT    31
#define NSP_IOCTL_WAIT_ON_REQUEST      32
#define NSP_IOCTL_DEBUG_MESSAGES       34

#define NSP_IOCTL_ALLOCATE_BUFFER_PK   35
#define NSP_IOCTL_FREE_BUFFER_PK       36

#define NSP_IOCTL_N8_DAEMON_SYS_INIT   39
#define NSP_IOCTL_ALLOCATE_CONTEXT     40
#define NSP_IOCTL_FREE_CONTEXT         41
#define NSP_IOCTL_QMGR_QUEUE           42
#define NSP_IOCTL_RN_QUEUE             44
#define NSP_IOCTL_RN_SET_PARMS         45
#define NSP_IOCTL_RN_GET_PARMS         46
#define NSP_IOCTL_SKS_WRITE            47
#define NSP_IOCTL_SKS_RESET_UNIT       48
#define NSP_IOCTL_SKS_ALLOCATE         49
#define NSP_IOCTL_SKS_SET_STATUS       50
#define NSP_IOCTL_QUERY_QUEUE_STATS    51
#define NSP_IOCTL_VALIDATE_CONTEXT     52
#define NSP_IOCTL_DIAGNOSTIC           53


/* CORE SELECTORS FOR IOCtl PARAMETERS */
#define N8_DAPI_PKE             1
#define N8_DAPI_RNG             2
#define N8_DAPI_EA              4
#define N8_DAPI_AMBA            8



/* DEBUG MESSAGE BIT SELECTORS */
#define N8_DBG_GENERAL            1
#define N8_DBG_IRQ                2
#define N8_DBG_BIGALLOC           4
#define N8_DBG_DISPLAY_MEMORY     8
#define N8_DBG_DISPLAY_CONTEXT    16
#define N8_DBG_DISPLAY_REGISTERS  32
#define N8_DBG_DISPLAY_QMGR       64


#endif   /* NSP_IOCTL_H */



