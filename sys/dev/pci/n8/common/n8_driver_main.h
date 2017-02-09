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
 * @(#) n8_driver_main.h 1.27@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_driver_main.h
 *  @brief NSP2000 Device Driver main common header
 *
 * This file contains all data structures, constants, and prototypes used by
 * the NSP2000 Device Driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/12/04 bac   Fixed time-related defines to be more clear.
 * 04/21/03 brr   Move several non-configuration constants here from 
 *                n8_driver_parms.h.
 * 03/19/03 brr   Added statistics to better track IRS's.
 * 02/20/03 brr   Eliminated all references to shared memory.
 * 11/25/02 brr   Move multiply defined constants to this on file. Also
 *                reformat to conform to coding standards.
 * 10/10/02 brr   Eliminate eventWaitType.
 * 06/25/02 brr   Moved SKS constants here from n8_driver_parms.h.
 * 05/30/02 brr   Keep statistics for bus errors.
 * 05/20/02 brr   Revert to a single AMBA wait queue.
 * 05/02/02 brr   Moved SKS semaphore here from queue struture.
 * 03/19/02 msz   Shadow memory is now called shared memory.
 * 02/25/02 brr   Removed references to qmgrData.
 * 02/15/02 brr   Removed obsolete FPGA defines, added context memory support.
 * 01/15/02 msz   Support of an array of AMBA wait blocks.
 * 11/26/01 mmd   Added N8_PCIDEVICES_t type and PCIinfo field to NspInstance_t.
 *                Pruned some no-longer used fields. Renamed from simon_drv.h.
 * 11/15/01 mmd   Eliminated memory struct field.
 * 11/14/01 mmd   Moved all magic numbers to n8_driver_parms.h.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 11/06/01 mmd   Added FPGA_Type and FPGA_CrystalType fields to
 *                SimonInstance_t.
 * 10/11/01 mmd   Added programmed field to SimonInstance_t.
 * 09/25/01 mmd   Added SKSSize, ContextMemSize, HardwareVersion, and
 *                RevisionID to SimonInstance_t, and changed field name from
 *                FPGA to IsFPGA. 
 * 09/24/01 mmd   Added AMBAblock, AMBAbitmask, and AMBAirqstatus to
 *                SimonInstance_t.
 * 09/20/01 mmd   Removed pid field from SimonInstance_t.
 * 09/13/01 mmd   Added FPGAsessionID and FPGAlocked to the SimonInstance_t
 *                structure, for FPGA lock safeguarding.
 * 09/12/01 mmd   Moved declaration of NSPDeviceTable_g, NSPcount_g, and
 *                N8_FPGA_g to driverglobals.h.
 * 09/10/01 mmd   Moved declaration of global resources to this header.
 * 09/07/01 mmd   Added DEF_PSEUDO_DEVICE to implement the pseudo device.
 * 08/21/01 mmd   Added new fields to SimonInstance_t (coretype, FPGALock).
 * 08/16/01 mmd   Removed inclusion of n8_types.h..
 * 07/27/01 brr   Added PLX & LCR macros.
 * 06/17/01 mmd   Further cleanup and debugging.            
 * 05/29/01 mmd   Incorporated suggestions from code review.
 * 05/17/01 mmd   Original version.
 ****************************************************************************/

#ifndef N8_DRIVER_MAIN_H
#define N8_DRIVER_MAIN_H


#include "helper.h"
#include "n8_device_info.h"
#include "n8_driver_parms.h"
#include "contextMem.h"

/* Time-related macros */
#define N8_USECS_PER_SEC    1000000
#define N8_USECS_PER_MSEC   1000
#define N8_USECS_PER_TICK   (N8_USECS_PER_SEC/HZ)
#define N8_MINIMUM_YIELD_USECS (10 * N8_USECS_PER_MSEC)

/*****************************************************************************
 * HARDWARE IDENTIFICATION -                                                 *
 * PCI devices are identified with unique vendor/device ID's. Also, here are *
 * some memory resource parameters for FPGAs, since they don't indicate the  *
 * correct needed amount of address space for operation.                     *
 *****************************************************************************/

#define DEF_ASIC_VENDOR_ID             0x170b
#define DEF_ASIC_DEVICE_ID             0x0100
#define DEF_ASIC_PCI_MEMORY_SIZE_2     (16384 * 4)    /* 64K          */

#define N8_DEF_SKS_MEMSIZE         512
#define N8_SKS_PROM_SIZE          4096

#define N8_EA_CMD_BLK_SIZE          128
#define N8_PKE_CMD_BLK_SIZE          32
#define N8_RNG_CMD_BLK_SIZE           8


#define N8_MAX_CMD_QUE_EXP           15
#define N8_MIN_CMD_QUE_EXP            4

#define N8_DEF_RNG_QUE_SIZE       (1<<N8_DEF_RNG_QUE_EXP)

/* The following two values are inSilicon PCI test registers that need  */
/* to be set to the PCI standards rather than their defaults.           */
#define INSILICON_PCI_TRDY_TIMEOUT      0x40
#define INSILICON_PCI_RETRY_TIMEOUT     0x41

/**************************************************************************
 * DEVICE INFORMATION STRUCTURES -                                        *
 *                                                                        *
 * The NSPDeviceTable_g is an array of structures, one per hardware       *
 * instance, that retain assorted information about each instance. The    *
 * minor number of any request indexes into this array. NSPcount_g        *
 * maintains the number of detected NSP2000 hardware instances.           *
 **************************************************************************/

typedef struct
{
   unsigned long   base_address [6];
   unsigned long   base_range   [6];
   unsigned short  vendor_id;
   unsigned short  device_id;
   unsigned char   revision_id;
   unsigned char   irq;
} N8_PCIDEVICES_t;

/* Device instance information structure */
typedef struct
{
   void  		  *dev;		     /* nsp2000_softsc_t *        */
   wait_queue_head_t       RNHblock;         /* Wait queue for RNH IRQ's  */
   wait_queue_head_t       CCHblock;         /* Wait queue for CCH IRQ's  */
   wait_queue_head_t       PKHblock;         /* Wait queue for PKH IRQ's  */
   wait_queue_head_t       AMBAblock;        /* Wait queue for AMBA IRQ's */
     
   unsigned long           RNHbitmask;       /* RNH IRQ bitmask           */
   unsigned long           CCHbitmask;       /* CCH IRQ bitmask           */
   unsigned long           PKHbitmask;       /* PKH IRQ bitmask           */
   unsigned long           AMBAbitmask;      /* AMBA IRQ bitmask          */
     
   unsigned long           RNHirqstatus;     /* RNH control/status @ IRQ  */
   unsigned long           CCHirqstatus;     /* CCH control/status @ IRQ  */
   unsigned long           PKHirqstatus;     /* PKH control/status @ IRQ  */
   unsigned long           AMBAirqstatus;    /* AMBA control/status @ IRQ */

   int                     RNHbuserrors;     /* Counter for bus errors    */
   int                     PKHbuserrors;     /* Counter for bus errors    */
   int                     CCHbuserrors;     /* Counter for bus errors    */
   int                     RNHcmderrors;     /* Counter for RNH errors    */
   int                     PKHcmderrors;     /* Counter for PKH errors    */
   int                     CCHcmderrors;     /* Counter for CCH errors    */
     
   N8_PCIDEVICES_t         PCIinfo;          /* PCI config info per inst  */
     
   unsigned long           contextMemSize;   /* Size of context memory (bytes) */
   unsigned long           contextMemEntries;/* Size of context memory (entries) */
   unsigned long           contextMemNext;   /* Index to begin search for */
                                             /* next allocation.          */
   ContextMemoryMap_t     *contextMemMap_p;  /* pointer to context memory */
                                             /* allocation map.           */
   ATOMICLOCK_t            contextMemSem;    /* Mutual exclusion semaphore*/

/********************************************************************************/
   unsigned short          HardwareVersion;  /* PCI Device ID                 */
   unsigned char           RevisionID;       /* PCI Revision ID               */
   unsigned char           chip;             /* Number or this chip           */
   void                   *RNqueue_p;        /* Pointer to RN queue           */
   unsigned long           RNqueue_size;     /* Size of RN queue in bytes     */
   unsigned long           RNqueue_base;     /* Phys. base addr. of RN queue  */
   void                   *PKqueue_p;        /* Pointer to PK queue           */
   unsigned long           PKqueue_size;     /* Size of PK queue in bytes     */
   unsigned long           PKqueue_base;     /* Phys. base addr. of PK queue  */
   void                   *EAqueue_p;        /* Pointer to EA queue           */
   unsigned long           EAqueue_size;     /* Size of EA queue in bytes     */
   unsigned long           EAqueue_base;     /* Phys. base addr. of EA queue  */
   char                    SKS_map[N8_DEF_SKS_MEMSIZE]; /* SKS allocation map */
   unsigned long           SKS_size;         /* Size of SKS in bytes          */
   ATOMICLOCK_t            SKSSem;           /* resource lock for SKS map     */
   void                   *RNGparms_p;       /* Pointer to RNG parm struct    */
   unsigned long           RNGparms_size;    /* Size of RNG parm struct (byte)*/
   unsigned long           RNGparms_base;    /* Phys. base addr. of RNG parms */
   unsigned long          *NSPregs_p;        /* NSP register mapping          */
   unsigned long           NSPregs_base;     /* Phys. base addr. of NSP regs  */
   uint16_t                EAreadIndex;      /* EA read index                 */
   uint16_t                PKreadIndex;      /* PK read index                 */
   N8_Hardware_t           hardwareType;     /* Device type for this chip     */           

} NspInstance_t;



#endif    /* N8_DRIVER_MAIN_H */



