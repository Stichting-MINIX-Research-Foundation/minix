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
 * @(#) n8_device_info.h 1.22@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_device_info.h
 *  @brief NSP2000 Device Driver IOCtl resources
 *
 * This file contains all data structures, constants, and prototypes used by
 * the IOCtl handler of the NSP2000 Device Driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/08/03 brr   Add user pool count to driver info.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 04/01/03 brr   Include n8_sdk_config instead of n8_driver_parms.
 * 10/22/02 brr   Added N8_Open_t enums & mode to NSPDevice_t.
 * 09/25/02 brr   Eliminate event wait types.
 * 09/18/02 brr   Added strutures to return data needed by diagnostics.
 * 06/12/02 hml   Added field for the session id to NSPdriverInfo_t.
 * 06/10/02 hml   Added fields for request bank and request pool to 
 *                NSPdriverInfo_t.
 * 05/31/02 brr   Removed all references to the shared memory pointer.
 * 05/07/02 brr   Added bncAddress to NSPdriverInfo_t.
 * 02/25/02 brr   Reworked so N8_GetConfig returns all driver info in a single
 *                call.
 * 02/18/02 brr   Removed oboslete context memory references.
 * 01/31/02 brr   Added shared memory pointer so ISR knows the data type.
 * 01/21/02 msz   Added event wait type information.
 * 01/21/02 brr   Modified to remove the mmap on each buffer on allocation.
 * 12/06/01 brr   Correct context memory detection for NSP2000.
 * 11/11/01 mmd   Original version.
 ****************************************************************************/


#ifndef N8_DEVICE_INFO_H
#define N8_DEVICE_INFO_H

#if 0
#include "n8_common.h"
#include "n8_sdk_config.h"
#endif
#include "n8_pub_common.h"

typedef enum
{
   N8_OPEN_UTIL = 0,
   N8_OPEN_APP  = 1,
   N8_OPEN_DIAG = 2
} N8_Open_t;

typedef struct
{
     unsigned short       HardwareVersion;  /* PCI Device ID                 */
     unsigned char        RevisionID;       /* PCI Revision ID               */
     N8_Hardware_t        hardwareType;     /* Device type for this chip     */
     unsigned long        RNqueueSize;      /* Size of RN queue in (entries) */
     unsigned long        PKqueueSize;      /* Size of PK queue in (entries) */
     unsigned long        EAqueueSize;      /* Size of EA queue in (entries) */
     unsigned long        SKS_size;         /* Size of SKS in bytes          */
     unsigned long        contextMemsize;   /* Size of context memory (bytes)*/
} NSPdevice_t;

typedef struct
{
   uint32_t        requestPoolSize;        /* Size of memory pool for this
                                              process. */
   uint32_t        requestPoolBase;        /* Physical base address of the  */
                                           /* request pool for this process */
} NSPrequestPool_t;

typedef struct
{
   uint32_t        numChips;               /* Number of NSP2000 detected by */
                                           /* the driver upon installation. */
   uint32_t        driverVersion;          /* SW version of this driver.    */
   uint32_t        eaMemorySize;           /* Size of preallocated EA memory*/
   uint32_t        eaMemoryBase;           /* Physical base address of the  */
                                           /* preallocated EA kernel memory.*/
   uint32_t        pkMemorySize;           /* Size of preallocated PK memory*/
   uint32_t        pkMemoryBase;           /* Physical base address of the  */
                                           /* preallocated PK kernel memory.*/
   uint32_t        userPools;              /* Number of user pools allocated*/
                                           /* by the driver.                */
   unsigned long   bncAddress;             /* Physical address of BNC const.*/
   unsigned long   sessionID;              /* The session ID */
   int             mode;

   NSPdevice_t     chipInfo[MAX_NSP_INSTANCES];
} NSPdriverInfo_t;

typedef struct
{
   uint32_t        chip;               /* Chip number for requested info */
   uint32_t        registerSize;       /* Size of register address space */
   uint32_t        registerBase;       /* Physical base address of the   */
                                       /* NSP registers.                 */
   uint32_t        eaQueueSize;        /* Size of EA queue (in bytes)    */
   uint32_t        eaQueueBase;        /* Physical base address of the   */
                                       /* EA queue.                      */
   uint32_t        pkQueueSize;        /* Size of PK queue (in bytes)    */
   uint32_t        pkQueueBase;        /* Physical base address of the   */
                                       /* PK queue.                      */
   uint32_t        rnQueueSize;        /* Size of RN queue (in bytes)    */
   uint32_t        rnQueueBase;        /* Physical base address of the   */
                                       /* RN queue.                      */
} NSPdiagInfo_t;

#endif    /* N8_DEVICE_INFO_H */
