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

static char const n8_id[] = "$Id: nsp_ioctl.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file nsp_ioctl.c
 *  @brief NSP2000 Device Driver IOCtl Handler.
 *
 * This file implements the common IOCtl handler for the NSP2000 device driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/06/03 brr   Pass back the status returned by N8_WaitOnRequest.
 * 05/08/03 brr   Added support for multiple user pools.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 04/01/03 brr   Reverted N8_WaitOnRequest to accept timeout parameter.
 * 03/20/03 brr   Modified nspdebug to display QMgr statistics.
 * 03/19/03 brr   Modified nspdebug to display interrupt statistics.
 * 03/19/03 brr   Modified N8_WaitOnRequest to post on a specific request.
 * 03/10/03 brr   Set userSpace flag in API requests.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 10/22/02 brr   Verify driver mode in NSP_IOCTL_GET_DEVICE_RESOURCES.
 * 10/10/02 brr   Modified diagnostic ioctl to accept chip number.
 * 09/18/02 brr   Added ioctls for diagnostics & wait on request.
 * 07/24/02 jpw   Added NSP_IOCTL_QMGR_QUEUE ioctl again which QMgrQueue
 * 		  needs to avoid performance problems under Linux 2.2 with 
 * 		  the write() call
 * 07/08/02 brr   Moved QMgrQueue operation to a write call.
 * 07/02/02 brr   Added ioctl to retrieve memory statistics.
 * 06/26/02 brr   Remove bank parameter from calls to N8_PhysToVirt.
 * 06/25/02 brr   Rework user pool allocation to only mmap portion used by
 *                the individule process.
 * 06/14/02 hml   Set requestPoolSize to 0 if we fail to allocate the
 *                requestPool.
 * 06/12/02 hml   N8_PhysToVirt calls now get an additional parameter.
 * 06/10/02 hml   NSP_IOCTL_GET_DEVICE_RESOURCES now allocates the user
 *                request pool.
 * 05/30/02 brr   Removed obsolete HALT_ON_BUS_ERRORS.
 * 05/22/02 hml   Passed memCtlBank parameter to call to n8_pmalloc
 *                and n8_pfree.
 * 05/15/02 brr   Reworked RNG Ioctls to remove KMALLOC requirements.
 * 04/11/02 brr   Pass parameter to waitOnInterrupt to indicate whether 
 *                sleep should be interruptable.
 * 04/03/02 spm   Removed remnants of the reverse-ioctl that the daemon
 *                formerly used (start, finish, shutdown).  Removed #include 
 *                of n8_daemon_internals.h
 * 04/02/02 spm   SKS re-architecture.  Modified DAEMON_SYS_INIT ioctl so that
 *                SKS initialization is accomplished by a single forward
 *                ioctl, making the daemon unnecessary.
 * 03/29/02 brr   Added support to display the NSP 2000 register set.
 * 03/29/02 hml   Added handle of NSP_IOCTL_VALIDATE_CONTEXT (BUGS 657 658).
 * 03/26/02 hml   n8_contextfree/n8_contextalloc return a status code. (BUG 637)
 * 03/22/02 hml   NSP_IOCTL_ALLOCATE_CONTEXT now calls the correct function
 *                and NSP_IOCTL_DEBUG_MESSAGES prints the context 
 *                debugging information.
 * 03/21/02 mmd   Implemented NSP_IOCTL_QUERY_QUEUE_STATS.
 * 03/21/02 brr   Removed unneeded api include file.
 * 03/18/02 brr   Reduce parameters & pass sessionID to resource allocation
 *                functions.
 * 03/01/02 msz   Don't copy RN requests, they too are now KMALLOCed
 * 02/25/02 brr   Fix daemon start/stop. Do not copy API requests since they
 *                are now KMALLOC'ed.
 * 02/27/02 msz   Fixes in N8_WaitOnInterrupt call
 * 02/25/02 brr   Removed copying of all parameters TO/FROM user space on every
 *                call. Also deleted obsolete IOCTL's.
 * 02/22/02 spm   Converted printk's to DBG's.
 * 02/25/02 msz   Added SKS ioctl's.
 * 02/19/02 msz   Added RNG ioctl's.
 * 02/18/02 brr   Added QMgr ioctl's.
 * 02/15/02 brr   Removed FPGA specific ioctl's.
 * 02/13/02 brr   Only use DAEMON IOCTLS's if not VxWorks.
 * 01/21/02 spm   Added NSP_IOCTL_N8_DAEMON_SYS_INIT ioctl to handle
 *                any initialization that requires the N8 daemon to
 *                be running already (like SKS initialization using
 *                the file system).
 * 01/19/02 spm   Changed n8_daemon_kernel.h to n8_daemon_internals.h
 * 01/17/02 spm   Added NSP_IOCTL_N8_DAEMON_SHUTDOWN ioctl to handle shutdown
 *                requests from userspace (uninstall script).
 * 01/17/02 brr   Support new memory allocation scheme.
 * 01/16/02 brr   Removed obsolete include file.
 * 01/16/02 spm   Added NSP_IOCTL_N8_DAEMON_START, NSP_IOCTL_N8_DAEMON_FINISH
 *                ioctls for N8 daemon support.
 * 01/02/02 hml   Changed to use N8_API_Parms_t instead of API_PARMS_t.
 * 12/18/01 brr   Added display of kernal memory structures.
 * 12/14/01 brr   Support memory management performance improvements.
 * 11/27/01 mmd   Eliminated NSP_IOCTL_ENABLE_IRQ.
 * 11/29/01 brr   Added n8_dispatchAPI
 * 11/15/01 mmd   Moved admxrc_set_clock calls to admxrc_load_fpga.
 * 11/14/01 mmd   Updated NSP_IOCTL_DEBUG_MESSAGES to allow applications to
 *                force the driver to release or re-allocate static resources.
 *                Mainly for diags to release resources and use the memory
 *                pool as they see fit.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 11/06/01 mmd   Migrated ADMXRC_PLX_EEPROM_RD into NSP_IOCTL_GET_FPGA_INFO,
 *                and ADMXRC_FPGA_LOAD into NSP_IOCTL_PROGRAM_FPGA. Eliminated
 *                NSP_IOCTL_FPGA_LOCK, NSP_IOCTL_FPGA_UNLOCK, and
 *                ADMXRC_SET_CLOCK.
 * 10/26/01 mmd   Updated call to n8_DeallocateBuffer.
 * 10/16/01 mmd   Revised debug info to a single parm - debug. Revised all
 *                branches accordingly, and added new branch for
 *                NSP_IOCTL_DEBUG_MESSAGES.
 * 10/12/01 mmd   Renamed Atomic*() routines to N8_Atomic*().
 * 10/12/01 mmd   Now only returns -EINVAL in case of failure.
 * 10/02/01 mmd   Modified NSP_IOCTL_FPGA_LOCK to return a busy indicator
 *                instead of doing a while loop, to eliminate the potential
 *                for a nasty kernel hang.
 * 09/26/01 mmd   Fixed segfault in N8_IOCtlHandler where the FPGAflag field
 *                of NspInstance_t was always getting referenced even if
 *                a pseudo device was being used.
 * 09/25/01 mmd   Added NSP_IOCTL_GET_CONFIG_ITEM branch. Eliminated FPGAflag
 *                as a parameter - now using the IsFPGA field of
 *                NspInstance_t.
 * 09/24/01 mmd   Fixed bug in NSP_IOCTL_WAIT_ON_INTERRUPT - not returning
 *                the correct sampled control/status register value, and added
 *                support for blocking on AMBA interrupts, and corrected
 *                return values.
 * 09/19/01 mmd   Creation.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver IOCtl Handler.
 */


#include "helper.h"
#include "n8_driver_main.h"
#include "n8_driver_parms.h"
#include "n8_malloc_common.h"
#include "irq.h"
#include "displayRegs.h"
#include "config.h"
#include "nsp_ioctl.h"
#include "n8_daemon_common.h"
#include "n8_sksInit.h"
#include "n8_memory.h"
#include "n8_enqueue_common.h"
#include "n8_SKSManager.h"
#include "QMQueue.h"
#include "RN_Queue.h"
#include "QMgrInit.h"
#include "QMUtil.h"
#include "n8_pub_common.h"
#include "userPool.h"
#include "n8_driver_api.h"


/* Instance, indexed by minor number */
extern NspInstance_t NSPDeviceTable_g [DEF_MAX_SIMON_INSTANCES];
extern int driverMode;

int n8_ioctlHandler( unsigned int     cmd, 
                     NspInstance_t   *NSPinstance_p,
                     unsigned long    SessionID,
                     unsigned char   *debug,
                     unsigned long    arg);
/*****************************************************************************
 * n8_ioctlHandler
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief ioctl handler.
 *
 * This routine is the entry point for ioctl requests made to the driver. The
 * handler is basically a large switch statement that branches on the specified
 * IOCTL code. See nsp_ioctl.h for a list of all currently-supported ioctl
 * codes. 
 *
 * Note that some branches use N8_FROM_USER and N8_TO_USER upon entry and
 * exit, for access to the user data being passed between the user process and
 * this ioctl handler. In these cases, a user-space pointer is being passed
 * into the handler as the 4th parameter (arg). After typecasting it as a
 * caddr_t, these two copy operations allow us to copy contents from a
 * user-space pointer to a local kernel-space pointer, to actually use the
 * data. Upon exiting, if data is to be returned to the calling user process,
 * the data is copied back to the space pointed to by the user-space pointer.
 *
 * @param cmd   RO: Standard parameter for an ioctl handler, containing the
 *                  ioctl code specified in the user-space ioctl system call.
 * @param NSPinstance_p RO: Pointer to the data structure for this instance.
 * @param SessionID     RO: The PID of the calling process.
 * @param debug         RO: The value of the debug flag.
 * @param arg   RW: Standard parameter for an ioctl handler, and is a
 *                  user-defined entity. The NSP2000 driver interprets it as a
 *                  pointer to a parameter structure, and typecasts it to a
 *                  pointer before copying to or from it, allowing user
 *                  processes to pass parameters on the ioctl call, and allowing
 *                  the driver to return result data to the user process.
 *
 * @par Externals:
 *    N8_Debug_g        RO: #define - If general debug messages are not <BR>
 *                          enabled, this routine immediately exits,    <BR>
 *                          because it's a debug routine.               <BR>
 *    NSPcount_g        RO: Number of detected hardware instances.      <BR>
 *    NSPDeviceTable_g  RO: Array of information structures, one per    <BR>
 *                          hardware instance.
 *
 * @return 
 *    -EINVAL  General failure
 *         0   Success
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

int n8_ioctlHandler( unsigned int     cmd, 
                     NspInstance_t   *NSPinstance_p,
                     unsigned long    SessionID,
                     unsigned char   *debug,
                     unsigned long    arg)
{
   int retval = 0;

   /* Branch on command code */
   switch(cmd)
   {
      case NSP_IOCTL_DEBUG_MESSAGES:

         /* Apply debug message related bits */
         *debug = arg;
         n8_memoryDebug(arg & N8_DBG_BIGALLOC);

         if (arg & N8_DBG_DISPLAY_MEMORY)
         {
            /* Display hardware memory resources */
            n8_memoryDisplay(N8_MEMBANK_QUEUE);
            n8_memoryDisplay(N8_MEMBANK_EA);
            n8_memoryDisplay(N8_MEMBANK_PK);
            userPoolDisplay();
         }

         if (arg & N8_DBG_DISPLAY_CONTEXT)
         {
            /* Display hardware context resources */
            n8_contextDisplay();
         }
         if (arg & N8_DBG_DISPLAY_REGISTERS)
         {
            /* Display NSP2000 control register set in kernel log */
            N8_DisplayRegisters();
         }
         if (arg & N8_DBG_IRQ)
         {
            /* Display NSP2000 IRQ Statistics in the kernel log */
            n8_DisplayIRQ();
         }
         if (arg & N8_DBG_DISPLAY_QMGR)
         {
            /* Display QMgr information in kernel log */
            displayQMgr();
         }

         break;


      case NSP_IOCTL_GET_DEVICE_RESOURCES:
      {
         NSPdriverInfo_t   driverInfo;
	 int               mode;

         N8_FROM_USER(&driverInfo, (void *)arg, sizeof(NSPdriverInfo_t));
	 mode = driverInfo.mode;

         /* Get the existing config */
         N8_GetConfig(&driverInfo);

	 if ((mode != N8_OPEN_UTIL) && (mode != driverInfo.mode))
	 {
	    if (driverInfo.mode == N8_OPEN_UTIL)
	    { 
	       driverMode = mode;
	    }
	    else
	    {
	       retval = N8_INCOMPATIBLE_OPEN;
	    }
	 }

         /* Copy device info struct to user-space buffer */
         N8_TO_USER((void *)arg, &driverInfo, sizeof(NSPdriverInfo_t));
      }
      break;


      case NSP_IOCTL_ALLOCATE_BUFFER:
         
         /* Make call to n8_AllocateBuffer */
         retval = n8_pmalloc(N8_MEMBANK_EA, arg, SessionID);
         break;
         
      case NSP_IOCTL_ALLOCATE_BUFFER_PK:
         
         /* MAKE CALL TO n8_AllocateBuffer */
         retval = n8_pmalloc(N8_MEMBANK_PK, arg, SessionID);
         break;
         
      case NSP_IOCTL_FREE_BUFFER:
         n8_pfree(N8_MEMBANK_EA, (void *)arg);
         break;
         
      case NSP_IOCTL_FREE_BUFFER_PK:
         n8_pfree(N8_MEMBANK_PK, (void *)arg);
         break;
         
      case NSP_IOCTL_ALLOCATE_USER_POOL:
      {
         NSPrequestPool_t  requestPoolInfo;

         retval = N8_MALLOC_FAILED;

         /* Allocate the user pool for the process */
         requestPoolInfo.requestPoolSize = (DEF_USER_POOL_SIZE * N8_ONE_MEGABYTE);
         requestPoolInfo.requestPoolBase = userPoolAlloc(SessionID);

         if (requestPoolInfo.requestPoolBase)
         {
            retval = N8_STATUS_OK;
         }

         /* Copy device info struct to user-space buffer */
         N8_TO_USER((void *)arg, &requestPoolInfo, sizeof(NSPrequestPool_t));
      }
      break;

      case NSP_IOCTL_WAIT_ON_INTERRUPT:      /* REAL-ONLY */
      {
         PARMSTRUCT_t parms;

         N8_FROM_USER(&parms, (void *)arg, sizeof(PARMSTRUCT_t));
     
         /* Block for notification of specified interrupts */
         parms.invalid_handle = 0;
         retval = waitOnInterrupt( parms.chip,
                                   parms.coretype,
                                   parms.bitmask,
                                   parms.timeout,
                                   TRUE );
         if (retval == 1)
         {
            /* Successful irq receipt */
            parms.timeout = 1;
            retval = 0;
         }
         else if (retval == 0)
         {
            /* Timeout */
            parms.timeout = 0;
            retval = 0;
         }
         else
         {
            /* Invalid core type */
            parms.invalid_handle = 1;
         }

         /* Return receipt or timeout to caller */
         parms.irqstatus = 0;
         if (parms.coretype == N8_DAPI_PKE)
         {
            parms.irqstatus = NSPinstance_p->PKHirqstatus;
         }
         else if (parms.coretype == N8_DAPI_RNG)
         {
            parms.irqstatus = NSPinstance_p->RNHirqstatus;
         }
         else if (parms.coretype == N8_DAPI_EA)
         {
            parms.irqstatus = NSPinstance_p->CCHirqstatus;
         }
         else if (parms.coretype == N8_DAPI_AMBA)
         {
            parms.irqstatus = NSPinstance_p->AMBAirqstatus;
         }
         N8_TO_USER((void *)arg, &parms, sizeof(PARMSTRUCT_t));
      }
      break;

      case NSP_IOCTL_N8_DAEMON_SYS_INIT:
      {
          n8_DaemonMsg_t parms;
             
         /* This is where we do any system
          * initialization that requires
          * the N8 userspace daemon to
          * be already running.  This
          * ioctl is triggered by a
          * second userspace program.
          */

         /* currently, all we do here is
          * initialize the SKS PROM allocation
          * units using data on the file
          * system
          */
         
         N8_FROM_USER(&parms, (void *)arg, sizeof(n8_DaemonMsg_t));
         
         n8_SKSInitialize(&parms);
         
         break;
      }

      case NSP_IOCTL_ALLOCATE_CONTEXT:
      {
         PARMSTRUCT_t parms;

         N8_FROM_USER(&parms, (void *)arg, sizeof(PARMSTRUCT_t));
     
         /* Allocate a context entry */
         retval =
            n8_contextalloc(&parms.chip, 
                            SessionID, 
                            (unsigned int *)&parms.contextIndex);
         N8_TO_USER((void *)arg, &parms, sizeof(PARMSTRUCT_t));
      }
      break;
     
      case NSP_IOCTL_FREE_CONTEXT:
      {
         PARMSTRUCT_t parms;

         N8_FROM_USER(&parms, (void *)arg, sizeof(PARMSTRUCT_t));
     
         /* Free the context entry */
         retval = n8_contextfree(parms.chip, SessionID, parms.contextIndex);
      }
      break;

      case NSP_IOCTL_VALIDATE_CONTEXT:
      {
         PARMSTRUCT_t parms;

         N8_FROM_USER(&parms, (void *)arg, sizeof(PARMSTRUCT_t));
     
         /* Free the context entry */
         retval = n8_contextvalidate(parms.chip, SessionID, parms.contextIndex);
      }
      break;
     
      case NSP_IOCTL_QMGR_QUEUE:                                       
      {                                                                
         API_Request_t *apiRequest_p;                                  
                                                                       
         /* Convert physical address to kernel virtual address. */     
         apiRequest_p = N8_PhysToVirt(arg);                            
         apiRequest_p->userSpace = N8_TRUE;
         retval = N8_QMgrQueue(apiRequest_p);                          
      }                                                                
      break;                                                           

      case NSP_IOCTL_RN_QUEUE:
      {
         RN_Request_t rn_request;

         /* Copy request parameters to kernel space. */
         N8_FROM_USER(&rn_request, (void *)arg, sizeof(RN_Request_t));
         rn_request.userRequest = N8_TRUE;
         retval = Queue_RN_request(&rn_request);
      }
      break;

      case NSP_IOCTL_RN_SET_PARMS:
      {
         N8_RNG_Parameter_t *parms_p;

         retval = N8_MALLOC_FAILED;

         /* Allocate and copy the request to kernel space */
         parms_p = N8_UMALLOC(sizeof(N8_RNG_Parameter_t));
         if (parms_p)
         {
            N8_FROM_USER(parms_p,
                         (void *)arg,
                         sizeof(N8_RNG_Parameter_t));

            /* Queue the request */
            retval = RN_SetParameters(parms_p,N8_RNG_UNIT);
            N8_UFREE(parms_p);
         }
      }
      break;

      case NSP_IOCTL_RN_GET_PARMS:
      {
         N8_RNG_Parameter_t *parms_p;

         retval = N8_MALLOC_FAILED;

         /* Allocate and copy the request to kernel space */
         parms_p = N8_UMALLOC(sizeof(N8_RNG_Parameter_t));
         if (parms_p)
         {
            /* Queue the request */
            retval = RN_GetParameters(parms_p,N8_RNG_UNIT);
            N8_TO_USER((void *)arg,
                       parms_p,
                       sizeof(N8_RNG_Parameter_t));
            N8_UFREE(parms_p);
         }
      }
      break;

      case NSP_IOCTL_SKS_WRITE:
      {

         n8_SKSWriteParams_t  params;

         /* Get the parameters from the user.    */
         N8_FROM_USER(&params,
                      (void *)arg,
                      sizeof(n8_SKSWriteParams_t));

         /* At this point we know that we need to do a copy in   */
         /* of data_p data, so set fromUser to TRUE in the call  */
         /* to n8_SKSWrite.                                      */
         retval = n8_SKSWrite(params.targetSKS,
                              params.data_p,
                              params.data_length,
                              params.offset,
                              TRUE);
      }
      break;

      case NSP_IOCTL_SKS_RESET_UNIT:
      {
         /* No parameters are changed, and we are only passing   */
         /* a single parameter (targetSKS, which is equivalent   */
         /* to an int.  This makes the call pretty trivial.      */
         const N8_Unit_t targetSKS = (N8_Unit_t) arg;
         retval = n8_SKSResetUnit(targetSKS);
      }
      break;

      case NSP_IOCTL_SKS_ALLOCATE:
      {
         N8_SKSKeyHandle_t keyHandle;

         /* Copy in the key handle.                      */
         N8_FROM_USER(&keyHandle,
                      (void *)arg,
                      sizeof(N8_SKSKeyHandle_t));

         /* Do the SKS Allocate.                         */
         retval = n8_SKSAllocate(&keyHandle);

         /* Copy out any changes to the key handle.      */
         N8_TO_USER((void *)arg,
                    &keyHandle,
                    sizeof(N8_SKSKeyHandle_t));
      }
      break;

      case NSP_IOCTL_SKS_SET_STATUS:
      {

         n8_setStatusParams_t  params;
         N8_SKSKeyHandle_t keyHandle;

         /* Get the parameters from the user.     */
         N8_FROM_USER(&params,
                      (void *)arg,
                      sizeof(n8_setStatusParams_t));
         N8_FROM_USER(&keyHandle,
                      (void *)params.keyHandle_p,
                      sizeof(N8_SKSKeyHandle_t));

         retval = n8_SKSsetStatus(&keyHandle,
                                  params.status);

         /* KeyHandle land status are not changed by the  */
         /* call to set status.  Therefore no copy out is */
         /* needed.                                       */
      }
      break;

      case NSP_IOCTL_QUERY_QUEUE_STATS:
      {
         N8_QueueStatistics_t stats;
     
         N8_FROM_USER(&stats, (void *)arg, sizeof(N8_QueueStatistics_t));
     
         retval = N8_QMgrQueryStatistics(&stats);
     
         N8_TO_USER((void *)arg, &stats, sizeof(N8_QueueStatistics_t));
     
         break;
      }

      case NSP_IOCTL_MEMORY_STATS:
      {
         MemStats_t memStats[N8_MEMBANK_MAX+DEF_USER_POOL_BANKS];
     
         retval = N8_QueryMemStatistics(&memStats[0]);
         if (retval == N8_STATUS_OK)
         {
            userPoolStats(&memStats[N8_MEMBANK_USERPOOL]);
         }
     
         N8_TO_USER((void *)arg, &memStats, 
                     sizeof(MemStats_t) * (N8_MEMBANK_USERPOOL+userPoolCount()));
     
         break;
      }

      case NSP_IOCTL_DIAGNOSTIC:
      {
         NSPdiagInfo_t   diagInfo;
	 int             chip;

         N8_FROM_USER(&diagInfo, (void *)arg, sizeof(NSPdiagInfo_t));
	 chip = diagInfo.chip;

         /* Get the existing config */
         diagInfo.registerBase = NSPDeviceTable_g[chip].NSPregs_base;
         diagInfo.registerSize = NSPDeviceTable_g[chip].PCIinfo.base_range[0];
         diagInfo.eaQueueBase  = NSPDeviceTable_g[chip].EAqueue_base;
         diagInfo.eaQueueSize  = NSPDeviceTable_g[chip].EAqueue_size;
         diagInfo.pkQueueBase  = NSPDeviceTable_g[chip].PKqueue_base;
         diagInfo.pkQueueSize  = NSPDeviceTable_g[chip].PKqueue_size;
         diagInfo.rnQueueBase  = NSPDeviceTable_g[chip].RNqueue_base;
         diagInfo.rnQueueSize  = NSPDeviceTable_g[chip].RNqueue_size;

         /* Copy diag info struct to user-space buffer */
         N8_TO_USER((void *)arg, &diagInfo, sizeof(NSPdiagInfo_t));
      }
      break;

      case NSP_IOCTL_WAIT_ON_REQUEST:

         retval = N8_WaitOnRequest(arg);                          
	 break;

      default: retval = -EINVAL;
   }
   return (retval);
} /* n8_ioctlHandler */


