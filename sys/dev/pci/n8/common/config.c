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

static char const n8_id[] = "$Id: config.c,v 1.4 2015/04/13 16:33:25 riastradh Exp $";
/*****************************************************************************/
/** @file config.c
 *  @brief NSP2000 Device Driver Configuration Manager.
 *
 * This file implements the configuration management routines for the NSP2000
 * device driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/15/04 jpw   Dynamically allocate queue size based on the number of
 * 		  chips installed as counted by NSPcount_g. Note that FreeBSD
 * 		  currently requires this value to be forced since it doesn't
 *		  know how to properly count devices.
 * 05/15/03 brr   Enable RNG interrupt, enable interrupts only in 
 *                n8_enableInterrupts().
 * 05/09/03 brr   Add user pool count to driver infomation.
 * 05/07/03 brr   Do not fail HW resource allocation if SKS is not present.
 * 04/30/03 brr   Reconcile differences between 2.4 & 3.0 baselines.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 03/19/03 brr   Modified N8_ConfigInterrupts to always enble the AMBA timer
 *                and the PK command complete interrupt.
 * 03/03/03 jpw   Added N8_EnableAmbaTimer_g param to N8_ConfigInterrupts
 * 03/11/03 brr   Added N8_CloseDevice so N8_InitializeAPI can be common.
 * 03/10/03 brr   Added N8_OpenDevice so N8_InitializeAPI can be common.
 * 02/20/03 brr   Removed references to shared memory.
 * 11/25/02 brr   Reworked initialization to us common functions to allocate
 *                all resources for NSP2000 driver and each chip.
 * 10/23/02 brr   Modified N8_ConfigInterrupts to accept parameter for
 *                the AMBA timer preset.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 10/22/02 brr   Retrieve driverMode in N8_GetConfig.
 * 09/25/02 brr   Eliminate references to EVENT_WAIT_TYPE.
 * 09/10/02 brr   Conditionally enable either the amba timer interrupt or
 *                the command completion interrupts.
 * 07/08/02 brr   Added N8_GetFD to return NULL fd for kernel API.
 * 06/26/02 brr   Remove bank parameter from calls to N8_PhysToVirt.
 * 06/25/02 brr   Rework user pool allocation to only mmap portion used by
 *                the individual process.
 * 06/12/02 hml   Calls to N8_PhysToVirt now take the bank parameter.
 * 06/10/02 hml   Calls to pmalloc now use the bank parameter
 * 06/07/02 brr   Call SKS_Remove to deallocate SKS resources.
 * 05/30/02 brr   Enable interrupts for error conditions.
 * 05/22/02 hml   Passed memCtlBank parameter to call to n8_pmalloc
 *                and n8_pfree.
 * 05/09/02 brr   Allocate and setup BNC constants.
 * 05/02/02 brr   Call SKS_Init to perform all SKS initialization. Do not
 *                allocate SKS map from memory pool.
 * 04/03/02 brr   Use N8_VERSION as defined in n8_version.h.
 * 04/03/02 spm   Removed #include of daemon headers.
 * 03/29/02 brr   Modified N8_ConfigInterrupts to use N8_AMBA_TIMER_CHIP.
 * 03/20/02 msz   Don't do consecutive 32 bit writes.
 * 03/20/02 brr   Modified N8_ConfigInterrupts to configure & enable 
 *                interrupts on all detected devices.
 * 03/19/02 msz   Shadow memory is now called shared memory, and is not
 *                allocated here.
 * 03/18/02 brr   Pass null session ID into pmalloc function.
 * 03/18/02 msz   We no longer need the shadow queue nor its memory
 * 03/12/02 brr   Moved daemon initialization to base.c
 * 03/08/02 brr   Memset SKS & shadow memory allocations to zero since 
 *                n8_pmalloc is no longer clearing allocations.
 * 03/01/02 brr   Added N8_DisableInterrupts.
 * 02/22/02 spm   Converted printk's to DBG's.  Added include of n8_OS_intf.h
 * 02/25/02 brr   Removed references to qmgrData & modified N8_GetConfig to 
 *                return all driver information in a single call.
 * 02/15/02 brr   Moved context memory functions to contextMem.c.
 * 02/14/02 brr   Reconcile memory management differences with 2.0.
 * 02/04/02 msz   Enable interrupts for errors, pass hwidx so timer is enabled
 *                only on first nsp.
 * 01/25/02 bac   Added include of bigalloc_support.h as required.
 * 01/30/02 brr   Modified to support changes in bigalloc.
 * 01/21/02 msz   Set up event wait type based on header options
 * 01/21/02 brr   Modified to remove the mmap on each buffer on allocation.
 * 02/13/02 brr   Conditionally initialize the daemon.
 * 02/15/02 brr   Moved context memory functions to contextMem.c.
 * 01/21/02 spm   Moved SKS_Prom2Cache to n8_sksInit.c.
 * 01/19/02 spm   Changed n8_daemon_kernel.h to n8_daemon_internals.h
 * 01/17/02 spm   Added call to n8_daemon_init to N8_AllocateHardwareResources.
 *                This does initialization of the N8 daemon.
 * 01/17/02 brr   Updated for new memory management scheme.
 * 01/16/02 brr   Removed FPGA support.
 * 01/14/02 brr   Correctly pass shadow register pointers to QMGR.
 * 01/10/02 mmd   Removed fixed context memory size. Modified
 *                N8_GetContextMemSize to simply return a size, and no longer
 *                return any kind of error. If Context Memory accesses fail,
 *                it now indicates 0 bytes available. Corrected the Context
 *                memory read/write routines to correctly read/write (no longer
 *                clearing pending ops, and no longer sleeping, but instead
 *                expecting any pending ops to complete within a reasonable
 *                time).
 * 01/10/02 msz   Disable bridge timer when we are done.
 * 01/03/02 brr   Setup and enable bridge timer.
 * 12/21/01 brr   Fix context memory size to 64MB.
 * 12/20/01 brr   Perform all static allocation with sessionID 0.
 * 12/18/01 brr   Perform all static allocation from n8_pmalloc.
 * 12/14/01 brr   Support dynamic queue sizing & memory management performance
 *                improvements.
 * 12/06/01 brr   Added seperate define for RNG queue sizing, moved SKS init
 *                to the driver, and correct context memory detection.
 * 12/05/01 brr   Move queue initialization to the driver.
 * 11/26/01 mmd   Updated parms for N8_ConfigInit to accomodate new PCIinfo
 *                field of NspInstance_t. Generally updated N8_ConfigInit and
 *                N8_GetConfig to ensure full support of both ASIC and FPGA.
 * 11/14/01 mmd   Using global parm defines from n8_driver_parms.h.
 * 11/13/01 mmd   Implemented N8_AllocateHardwareResources and
 *                N8_ReleaseHardwareResources;
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 11/06/01 mmd   Now calls admxrc_eeprom_read to determine values for
 *                FPGA_Type and FPGA_CrystalType.
 * 10/29/01 mmd   N8_OpenRulesChecker now uses N8_UTIL_MALLOC instead of
 *                N8_VIRT_MALLOC. N8_CloseRulesChecker now uses N8_UTIL_FREE.
 * 10/22/01 mmd   Implemented N8_ClaimHardwareInstance and
 *                N8_ReleaseHardwareInstance.
 * 10/12/01 mmd   Renamed Atomic*() routines to N8_Atomic*().
 * 10/12/01 mmd   Implemented N8_OpenRulesChecker, N8_CloseRulesChecker, and
 *                N8_PurgeNextRelatedSession, as well as the global process
 *                table. Also now initializing the programmed field of
 *                NspInstance_t.
 * 09/25/01 mmd   Creation.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver Configuration Manager.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/endian.h>
#ifdef __NetBSD__
  #define letoh16 htole16
  #define letoh32 htole32
#endif
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>
#include <sys/md5.h>
#include <sys/sha1.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include "n8_pub_errors.h"
#include "n8_driver_main.h"
#include "irq.h"
#include "n8_manage_memory.h"
#include "n8_pk_common.h"
#include "n8_rn_common.h"
#include "n8_ea_common.h"
#include "n8_sksInit.h"
#include "QMgrInit.h"
#include "config.h"
#include "nsp2000_regs.h"
#include "n8_version.h"
#include "n8_memory.h"
#include "userPool.h"
#include "n8_driver_api.h"
#if 0
#include "n8_OS_intf.h"
#include "helper.h"
#include "n8_driver_main.h"
#include "n8_driver_parms.h"
#include "irq.h"
#include "displayRegs.h"
#include "n8_memory.h"
#include "nsp_ioctl.h"
#include "n8_daemon_common.h"
#include "n8_sksInit.h"
#include "contextMem.h"
#include "n8_version.h"
#include "n8_pk_common.h"
#include "n8_rn_common.h"
#include "config.h"
#include "n8_driver_api.h"
#include "userPool.h"
#include "QMgrInit.h"
#include "n8_semaphore.h"
#endif

extern int NSPcount_g;
extern NspInstance_t NSPDeviceTable_g [];

int driverMode = 0;
unsigned long bncAddr = 0;
n8_WaitQueue_t requestBlock;
n8_WaitQueue_t nsp2000_wait;
/*****************************************************************************
 * n8_chipInit
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Allocates and initializes resources used by each nsp2000 core.
 *
 * This function allocates and initializes resources used by each nsp2000 core.
 * It should be called once per chip detected as the driver is initialized.
 * 
 *
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param HWidx           RO: Specifies the chip index.
 * @param queueSize       RO: The size of the command queue to allocate
 *                            for the EA & PK cores.
 * @param Debug           RO: The value of the N8_Debug_g flag.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

int n8_chipInit( NspInstance_t *NSPinstance_p,
                 int            HWidx,
                 int            queueSize,
                 unsigned char  Debug)
{
   volatile NSP2000REGS_t *nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;
   int  queueLength;      /* Number of entries */

   /* First store model identifiers */
   DBG(("store model id\n"));
   NSPinstance_p->hardwareType    = N8_NSP2000_HW;
   NSPinstance_p->HardwareVersion = NSPinstance_p->PCIinfo.device_id;
   NSPinstance_p->RevisionID      = NSPinstance_p->PCIinfo.revision_id;

   /* Initialize bus error statistics */
   DBG(("bus error stats\n"));
   NSPinstance_p->RNHbuserrors = 0;
   NSPinstance_p->PKHbuserrors = 0;
   NSPinstance_p->CCHbuserrors = 0;

   /* Set the amba PCI endian mode register */
   nsp->amba_pci_endian_mode = 0;

   queueLength = 1 << queueSize;

   /* Initialize sizes for allocated resources */
   NSPinstance_p->RNqueue_size    = 0;
   NSPinstance_p->PKqueue_size    = 0;
   NSPinstance_p->EAqueue_size    = 0;

   /* Initialize physical base address of each queue and struct */
   NSPinstance_p->RNqueue_base    = 0;
   NSPinstance_p->PKqueue_base    = 0;
   NSPinstance_p->EAqueue_base    = 0;

   NSPinstance_p->chip          = HWidx;

   /* Allocate static resources */
   if (bncAddr == 0)
   {
      bncAddr = n8_pmalloc(N8_MEMBANK_QUEUE, PKDIGITS_TO_BYTES(2), 0);
      DBG(("n8_pmalloc(N8_MEMBANK_QUEUE) -> 0x%x\n",
	      (uint32_t)bncAddr));
      if (bncAddr)
      {
         char *bnc_one_p;
         bnc_one_p = N8_PhysToVirt(bncAddr);
         DBG(("bncAddr 0x%x -> virt 0x%x\n",
		 	(uint32_t)bncAddr,
		 	(uint32_t)bnc_one_p));
         memset(bnc_one_p, 0x0, PKDIGITS_TO_BYTES(2));
         bnc_one_p[PK_Bytes_Per_BigNum_Digit - 1] = 1;
      }
      else
      {
         DBG(( "NSP2000: Failed BNC constant allocation.\n"));
         return 0;
      }
   }
   NSPinstance_p->RNqueue_base = n8_pmalloc(N8_MEMBANK_QUEUE,
                                 N8_DEF_RNG_QUE_SIZE*sizeof(RNG_Sample_t),
                                 0);
   if (!NSPinstance_p->RNqueue_base)
   {
      N8_PRINT(KERN_CRIT "NSP2000: Failed RN queue allocation.\n");
      return 0;
   }
   NSPinstance_p->PKqueue_base = n8_pmalloc(N8_MEMBANK_QUEUE,
                                 queueLength*sizeof(PK_CMD_BLOCK_t),
                                 0);
   if (!NSPinstance_p->PKqueue_base)
   {
      N8_PRINT(KERN_CRIT "NSP2000: Failed PK queue allocation.\n");
      return 0;
   }
   NSPinstance_p->EAqueue_base = n8_pmalloc(N8_MEMBANK_QUEUE,
                                 queueLength*sizeof(EA_CMD_BLOCK_t),
                                 0);
   if (!NSPinstance_p->EAqueue_base)
   {
      N8_PRINT(KERN_CRIT "NSP2000: Failed EA queue allocation.\n");
      return 0;
   }

   /* Update sizes of allocated resources */
   NSPinstance_p->RNqueue_size    = N8_DEF_RNG_QUE_SIZE;
   NSPinstance_p->PKqueue_size    = queueLength;
   NSPinstance_p->EAqueue_size    = queueLength;


   /* Convert physical base address of each struct to a kernel virtual address*/
   NSPinstance_p->RNqueue_p  = N8_PhysToVirt(NSPinstance_p->RNqueue_base);
   NSPinstance_p->PKqueue_p  = N8_PhysToVirt(NSPinstance_p->PKqueue_base);
   NSPinstance_p->EAqueue_p  = N8_PhysToVirt(NSPinstance_p->EAqueue_base);

   /* EAshared_p and PKshared_p are owned by QMgr and are thus set by QMgr */

   /* Set the queue pointers */
   nsp->pkh_q_bar1 = 0;
   nsp->cch_q_bar1 = 0;
   nsp->rnh_q_bar1 = 0;

   nsp->pkh_q_bar0 = NSPinstance_p->PKqueue_base;
   nsp->cch_q_bar0 = NSPinstance_p->EAqueue_base;
   nsp->rnh_q_bar0 = NSPinstance_p->RNqueue_base;

   nsp->pkh_q_length = queueSize;
   nsp->cch_q_length = queueSize;
   nsp->rnh_q_length = N8_DEF_RNG_QUE_EXP;

   /* Initialize the SKS data structures. */
   if (!SKS_Init(NSPinstance_p))
   {
      N8_PRINT(KERN_CRIT "NSP2000: Failed to Initialize SKS.\n");
   }

   /* Enable the EA & PK Queues since they need no parameters to start. */
   nsp->pkh_control_status |= NSP_CORE_ENABLE;
   nsp->cch_control_status |= NSP_CORE_ENABLE;

   if (Debug)
   {
     /* Announce allocations */
     N8_PRINT(KERN_CRIT "NSP2000: Allocated RN queue        - %ld entries @ %08lx.\n",
              NSPinstance_p->RNqueue_size, NSPinstance_p->RNqueue_base);
     N8_PRINT(KERN_CRIT "NSP2000: Allocated PK queue        - %ld entries @ %08lx.\n",
              NSPinstance_p->PKqueue_size, NSPinstance_p->PKqueue_base);
     N8_PRINT(KERN_CRIT "NSP2000: Allocated EA queue        - %ld entries @ %08lx.\n",
              NSPinstance_p->EAqueue_size, NSPinstance_p->EAqueue_base);
   }
     
   /* Allocate resources to manage the context memory */
   N8_ContextMemInit(HWidx);

   /* Initialize QMgr. */
   QMgrInit(HWidx);

   /* Initialize wait queue for each core */
   N8_InitWaitQueue(&NSPinstance_p->RNHblock);
   N8_InitWaitQueue(&NSPinstance_p->CCHblock);
   N8_InitWaitQueue(&NSPinstance_p->PKHblock);
   N8_InitWaitQueue(&NSPinstance_p->AMBAblock);

   return 1;
}



/*****************************************************************************
 * n8_chipRemove
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Deallocates resources for each chip.
 *
 * This funciton deallocates the resources used by each chip. Upon driver exit
 * this function should be called once for each chip initialized.
 * 
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param HWidx           RO: Specifies the chip index.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

N8_Status_t  n8_chipRemove(NspInstance_t *NSPinstance_p, int HWidx)
{

   volatile NSP2000REGS_t *nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;

   /* Disable each of the Cores */
   nsp->pkh_control_status = 0;
   nsp->cch_control_status = 0;
   nsp->rnh_control_status = 0;

   /* Reset the queue pointers */
   nsp->pkh_q_bar0 = 0;
   nsp->cch_q_bar0 = 0;
   nsp->rnh_q_bar0 = 0;

   /* Deallocate the resources used by the context memory management */
   N8_ContextMemRemove(HWidx);

   /* Remove the SKS data structures. */
   SKS_Remove(NSPinstance_p);

   /* Free static resources */
   if (bncAddr)
   {
      n8_pfree(N8_MEMBANK_QUEUE, (void *)bncAddr);
      bncAddr = 0;
   }
   n8_pfree(N8_MEMBANK_QUEUE, (void *)NSPinstance_p->RNqueue_base);
   n8_pfree(N8_MEMBANK_QUEUE, (void *)NSPinstance_p->PKqueue_base);
   n8_pfree(N8_MEMBANK_QUEUE, (void *)NSPinstance_p->EAqueue_base);

   /* Reset sizes for allocated resources */
   NSPinstance_p->RNqueue_size    = 0;
   NSPinstance_p->PKqueue_size    = 0;
   NSPinstance_p->EAqueue_size    = 0;

   /* Reset physical base address of each queue and struct */
   NSPinstance_p->RNqueue_base    = 0;
   NSPinstance_p->PKqueue_base    = 0;
   NSPinstance_p->EAqueue_base    = 0;

   /* Disable the timer and its interrupt */
   nsp->amba_pci_timer_preset = 0;
   nsp->amba_pci_control = 0;

   QMgrRemove(HWidx);

   /* Deallocate wait queue for each core */
   N8_DelWaitQueue(NSPinstance_p->RNHblock);
   N8_DelWaitQueue(NSPinstance_p->CCHblock);
   N8_DelWaitQueue(NSPinstance_p->PKHblock);
   N8_DelWaitQueue(NSPinstance_p->AMBAblock);

   return(N8_STATUS_OK);
}

/*****************************************************************************
 * N8_GetConfig
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Configuration manager.
 *
 * This routine retrieves information about the device(s) installed and running
 * under this driver and writes it to the location requested by the caller.
 *
 * @param *driverInfo_p RO: Address where the caller has requested the data
 *                          to be written.
 *
 * @par Externals:
 *
 * @return 
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void N8_GetConfig(NSPdriverInfo_t *driverInfo_p)
{

   int ctr;

   /* Make sure the pointer is not NULL */
   if (driverInfo_p)
   {

      /* Collect information about this driver installation. */
      driverInfo_p->numChips = NSPcount_g;
      driverInfo_p->driverVersion = N8_VERSION;

      /* Collect information about each memory region. */
      driverInfo_p->eaMemoryBase = memBankCtl_gp[N8_MEMBANK_EA]->memBaseAddress;
      driverInfo_p->eaMemorySize = memBankCtl_gp[N8_MEMBANK_EA]->allocSize;
      driverInfo_p->pkMemoryBase = memBankCtl_gp[N8_MEMBANK_PK]->memBaseAddress;
      driverInfo_p->pkMemorySize = memBankCtl_gp[N8_MEMBANK_PK]->allocSize;

      driverInfo_p->bncAddress = bncAddr;
      driverInfo_p->sessionID  = N8_GET_SESSION_ID;
      driverInfo_p->userPools  = userPoolCount();

      /* Collect information about each chip. */
      for (ctr = 0; ctr < NSPcount_g; ctr++)
      {
         driverInfo_p->chipInfo[ctr].contextMemsize  = NSPDeviceTable_g[ctr].contextMemSize;
         driverInfo_p->chipInfo[ctr].SKS_size        = NSPDeviceTable_g[ctr].SKS_size;
         driverInfo_p->chipInfo[ctr].HardwareVersion = NSPDeviceTable_g[ctr].HardwareVersion;
         driverInfo_p->chipInfo[ctr].hardwareType    = NSPDeviceTable_g[ctr].hardwareType;
         driverInfo_p->chipInfo[ctr].RevisionID      = NSPDeviceTable_g[ctr].RevisionID;
         driverInfo_p->chipInfo[ctr].RNqueueSize     = NSPDeviceTable_g[ctr].RNqueue_size;
         driverInfo_p->chipInfo[ctr].PKqueueSize     = NSPDeviceTable_g[ctr].PKqueue_size;
         driverInfo_p->chipInfo[ctr].EAqueueSize     = NSPDeviceTable_g[ctr].EAqueue_size;
      }

      driverInfo_p->mode = driverMode;
   }
    
   return;
}

/*****************************************************************************
 * N8_OpenDevice
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Configuration manager.
 *
 * This routine mirrors the user level function that is required to open the   
 * NSP2000 device and mmap its resources.
 *
 * @param *driverInfo_p RO: Address where the caller has requested the data
 *                          to be written.
 *
 * @par Externals:
 *
 * @return 
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

N8_Status_t N8_OpenDevice(NSPdriverInfo_t *driverInfo_p,
                          N8_Boolean_t     allocUserPool,
                          N8_Open_t        openMode)
{
   N8_GetConfig(driverInfo_p);
   return (N8_STATUS_OK);
}


/*****************************************************************************
 * N8_CloseDevice
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Configuration manager.
 *
 * This routine mirrors the user level function that is required to close the   
 * NSP2000 device and deallocates its resources.
 *
 * @return 
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void N8_CloseDevice(void)
{
   return;
}


/*****************************************************************************
 * n8_enableInterrupts
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Configuration of interrupts
 *
 * Configures and enables interrupts all detected NSP2000's.  Must be called 
 * after * the driver has registered its interrupt handler with the OS.
 *
 * @param
 *
 * @return 
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void n8_enableInterrupts(int timer_preset)
{

   int            counter;
   NspInstance_t *NSPinstance_p;
   unsigned long  amba_pci_control;
   volatile NSP2000REGS_t *nsp;

   for (counter = 0; counter < NSPcount_g; counter++)
   {
      NSPinstance_p      = &NSPDeviceTable_g[counter];
      nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;

      /* Set up for error interrupts */
      amba_pci_control = AMBAIRQ_PKP | AMBAIRQ_CCH | AMBAIRQ_RNG;

      /* Configure the timer interrupt on the first NSP2000. */
      if (counter == N8_AMBA_TIMER_CHIP)
      {
         /* Set up the timer preset */
         nsp->amba_pci_timer_preset = timer_preset;
      }

      /* Enable interrupt on error and PK command complete */
      nsp->pkh_intr_enable = PK_Status_Halting_Error_Mask | 
	                     PK_Enable_Cmd_Complete_Enable;
      nsp->cch_intr_enable = EA_Status_Halting_Error_Mask;

      nsp->amba_pci_control = amba_pci_control;

   }
}

/*****************************************************************************
 * n8_disableInterrupts
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Configuration of interrupts
 *
 * Disables interrupts on the hardware.  Must be called before deallocating
 * the resources referenced by the IRQ.
 *
 * @return 
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void n8_disableInterrupts(void)
{

   unsigned long           amba_pci_control;
   volatile NSP2000REGS_t *nsp;
   int                     ctr;

   /* Set up for error interrupts */
   amba_pci_control = 0;

   for (ctr = 0; ctr < NSPcount_g; ctr++)
   {

      /* Disable all interrupts for each device */
      nsp = (NSP2000REGS_t *)NSPDeviceTable_g[ctr].NSPregs_p;
      nsp->amba_pci_control = amba_pci_control;
   }

}

#if 0
/*****************************************************************************
 * N8_GetFD
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Return NULL file descriptor.
 *
 * Returns a NULL file descriptor for the kernel API.
 *
 * @return 
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

int N8_GetFD(void)
{
   return(0);
}
#endif


/*****************************************************************************
 * n8_driverInit
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Allocation and initialization of drivers resources.
 *
 * This function should be called once at driver installation. It is 
 * responsible for the allocation and initialization of global structures
 * used by the NSP2000 device driver.
 *
 * @param eaPoolSize  R0: Size (in KB) of the EA memory pool.
 *        pkPoolSize  R0: Size (in KB) of the PK memory pool.
 *
 * @par Externals:
 *
 * @return 
 *    0 - Success
 *
 * @par Errors:
 *    See return section for error information.
 *
 * NOTE: All resources allocated here should be removed in n8_driverRemove.
 *****************************************************************************/

int n8_driverInit(int eaPoolSize, int pkPoolSize)
{

   /* Allocate physically contiguous blocks of memory that are locked down */
   /* This pool of memory will be accessed by the NSP2000's DMA engine.    */

   /* The first allocation is for the command queues */
   if (!n8_memoryInit(N8_MEMBANK_QUEUE,
                      N8_QUEUE_POOL_SIZE * N8_ONE_KILOBYTE * NSPcount_g,
                      N8_QUEUE_GRANULARITY))
   {
      return -ENOMEM;
   }
   /* The second allocation is for the EA requests */
   if (!n8_memoryInit(N8_MEMBANK_EA,
                      eaPoolSize * N8_ONE_KILOBYTE,
                      N8_EA_GRANULARITY))
   {
      n8_memoryRelease(N8_MEMBANK_QUEUE);
      return -ENOMEM;
   }

   /* The third allocation is for the PK requests */
   if (!n8_memoryInit(N8_MEMBANK_PK,
                      pkPoolSize * N8_ONE_KILOBYTE,
                      N8_PK_GRANULARITY))
   {
      n8_memoryRelease(N8_MEMBANK_EA);
      n8_memoryRelease(N8_MEMBANK_QUEUE);
      return -ENOMEM;
   }

   /* Create the user pool */
   userPoolInit( DEF_USER_POOL_BANKS,
                 DEF_USER_POOL_SIZE,
                 DEF_USER_GRANULARITY);

#if 0
   /* Initialize the process init semaphore */
   n8_create_process_init_sem();
#endif

   /* Initialize driver wait queues */
   N8_InitWaitQueue(&nsp2000_wait);
   N8_InitWaitQueue(&requestBlock);

   return 0;
}


/*****************************************************************************
 * n8_driverRemove
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Deallocation and removal of drivers resources.
 *
 * This function should be called once at driver removal. It is 
 * responsible for the deallocation and removal of global structures
 * used by the NSP2000 device driver.
 *
 * @param 
 *
 * @par Externals:
 *
 * @return 
 *    0 - Success
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

int n8_driverRemove(void)
{

#if 0
   /* Delete the process init semaphore */
   n8_delete_process_init_sem();
#endif

   /* Remove the user pool */
   userPoolRelease();

   /* Remove the driver memory pools */
   DBG(("n8_driverRemove: n8_memoryRelease(PK=%d)\n", N8_MEMBANK_PK));
   n8_memoryRelease(N8_MEMBANK_PK);
   DBG(("n8_driverRemove: n8_memoryRelease(EA=%d)\n", N8_MEMBANK_EA));
   n8_memoryRelease(N8_MEMBANK_EA);
   DBG(("n8_driverRemove: n8_memoryRelease(QUEUE=%d)\n", N8_MEMBANK_QUEUE));
   n8_memoryRelease(N8_MEMBANK_QUEUE);

   /* Deallocate driver wait queues */
   N8_DelWaitQueue(nsp2000_wait);
   N8_DelWaitQueue(requestBlock);
   return 0;
}



