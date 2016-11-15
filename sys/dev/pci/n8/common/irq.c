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

static char const n8_id[] = "$Id: irq.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file irq.c
 *  @brief NSP2000 Device Driver interrupt handling functions
 *
 * This file contains all secondary interrupt handling routines for the
 * NSP2000 Device Driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 01/06/04 brr   Clear the PKH & CCH status register prior to reading queue
 *                pointer register to ensure write completes before exiting the
 *                interrupt handler. Only write AMBA status register on AMBA
 *                timer interrupts. (Bug 995)
 * 07/15/03 brr   Moved CCH processing to a Linux tasklet.
 * 07/01/03 brr   Clear CCH interrupts before reenabling the core.
 * 05/15/03 brr   Enable RNG interrupt, remove references to duplicate register
 *                definitions in irq.h
 * 04/08/03 brr   Bypass AMBA mirror registers.
 * 04/03/03 brr   Minor modifications to improve performance.
 * 04/01/03 brr   Reverted N8_WaitOnRequest to accept timeout parameter.
 * 03/31/03 brr   Do not rely on atomic_inc_and_test.
 * 03/24/03 brr   Fix race condition when conditionally resetting AMBA timer.
 * 03/21/03 brr   Only check for completed EA commands on AMBA timer interrupt.
 * 03/19/03 brr   Added Interrupt statistics and display function, eliminating
 *                IRQ prints. Added reload_AMBA_timer function in order support
 *                running AMBA timer only when there are requests queued. 
 *                Modified and moved N8_WaitOnRequest to QMGR.
 * 03/18/03 brr   Do not read PKH/CCH status register if no errors are reported
 *                in the AMBA status registers, assume command complete.
 * 02/02/03 brr   Updated command completion determination to use the number
 *                of commands completed instead of queue position. This
 *                elimated the need for forceCheck & relying on the request
 *                state to avoid prematurely marking a command complete.
 * 12/09/02 brr   Remove duplicate read in of read index in CCHInterrupt.
 * 11/01/02 brr   Reinstate forceCheck.
 * 10/23/02 brr   Remove forceCheck since interrupts are now disabled during
 *                the queue operation.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 10/11/02 brr   Added timeout parameter to N8_WaitOnRequest.
 * 09/18/02 brr   Added N8_WaitOnRequest & modified ISR to keep track of the
 *                number of requests completed.
 * 09/10/02 brr   Modified PKH & CCH interrupt service routines to handle the
 *                command completion interrupt.
 * 06/10/02 brr   Call QMgrCheckQueue if forceCheck is set.
 * 06/05/02 mmd   Eliminated unused debug messages, and ensured that when
 *                waitOnInterrupt is called with interruptible == FALSE,
 *                that AMBAbitmask is updated with the specified bitmask. 
 * 05/30/02 brr   Enable interrupts for error conditions and handle them in the
 *                ISR.
 * 05/20/02 brr   Revert to a single AMBA wait queue.
 * 04/30/02 brr   Minor revisions to improve performance & incorporate comments
 *                from code review.
 * 04/17/02 msz   Don't call QMgr if there is nothing new done.
 * 04/15/02 hml   Added a call to N8_DOWN_KERNELSEM when we get an AMBA
 *                interrupt in kernel mode.
 * 04/11/02 brr   Pass parameter to waitOnInterrupt to indicate whether
 *                sleep should be interruptable.
 * 04/03/02 brr   Fix spurious interrupt by clearing the interrupt before
 *                reading the amba_pci_control.
 * 03/19/02 msz   Shadow memory is now called shared memory.
 * 02/26/02 brr   Modified the timer interrupt to update the queue pointers
 *                and complete the API requests.
 * 02/27/02 msz   Fixes for N8_WaitOnInterrupt support.
 * 02/22/02 spm   Converted printk's to DBG's.  Added #include of n8_OS_intf.h
 * 02/22/02 brr   Removed references to qmgrData.
 * 02/22/02 msz   Fix for BUG 620, CCH should be using the EAshadow_p
 * 02/15/02 brr   Removed FPGA references. 
 * 02/06/02 msz   Moved where we set shadow hw status.
 * 01/31/02 brr   On error, save status to shared memory.
 * 01/15/02 msz   Support for array of AMBA wait blocks.  Also we now always
 *                do the wakeup call for AMBA interrupts once something has
 *                waited on an AMBA interrupt.
 * 01/03/02 brr   Reset bridge timer upon expiration.
 * 01/16/02 brr   Removed FPGA support. 
 * 12/03/01 mmd   Accidentally always performing 11-12 PCI accesses when using
 *                ASIC, regardless of whether or not we have an IRQ.
 * 11/30/01 mmd   Forgot to wrap LCR manipulation with check for FPGA.
 * 11/27/01 mmd   Eliminated N8_EnableInterrupts. Modified 
 *                n8_MainInterruptHandler to only bother with PLX if FPGA.
 *                Beyond that, everything is and should be treated identically.
 *                Renamed from simon_irq.c.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 10/17/01 mmd   Revised all debug messages to use N8_IRQprint/N8_IRQPRINT
 *                except for N8_EnableInterrupts and n8_WaitOnInterrupt,
 *                neither of which are called from Interrupt time.
 * 10/12/01 mmd   In n8_WaitOnInterrupt, corrected handling of return values
 *                from N8_BlockWithTimeout. 
 * 09/24/01 mmd   Added support in n8_WaitOnInterrupt and
 *                N8_MainInterruptHandler for AMBA interrupts, and adjusted
 *                return values for n8_WaitOnInterrupt. Corrected bug where
 *                the return code from N8_BlockWithTimeout was being
 *                misinterpreted. Updated call to N8_BlockWithTimeout, when
 *                passing a wait_queue_head_t* type.
 * 09/20/01 mmd   Implemented N8_MainInterruptHandler.
 * 09/07/01 mmd   Cleanup and revision of WaitOnInterrupt routine.
 * 08/16/01 mmd   No longer includes n8_types.h.
 * 08/16/01 mmd   Now includes nsp2000_regs.h instead of simon.h..
 * 08/08/01 brr   Moved all OS kernel specific macros to helper.h.
 * 08/02/01 brr   Fixed debug statements in WaitOn functions.
 * 07/31/01 mmd   Added SIMON_EnableInterrupts call.
 * 06/27/01 jke   added to debug print statements.  Changed logic of if state-
 *                ments in IRQs to handle non-debug-execution
 * 06/21/01 jke   added use of BSDIDRIVER and WakeUp macro, altered N8_dbgPrt
 *                macro to encorporate "if (N8_IRQ_Debug_g)", making the 
 *                resulting source more readable and compact. 
 * 06/12/01 jke   altered to run under BSDi
 * 05/29/01 mmd   Incorporated suggestions from code review.
 * 05/17/01 mmd   Original version.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver
 */


#include "helper.h"
#include "n8_driver_main.h"
#include "n8_driver_api.h"
#include "irq.h"
#include "nsp2000_regs.h"
#include "nsp_ioctl.h"
#include "n8_OS_intf.h"
#include "QMUtil.h"
#include "n8_ea_common.h"
#include "n8_pk_common.h"
#include <sys/proc.h>

/* FORWARD PROTOTYPES */
void N8_PKHInterruptHandler     ( NspInstance_t   *NSPinstance_p,
                                  uint32_t         reg_amba );
void N8_RNHInterruptHandler     ( NspInstance_t   *NSPinstance_p );
void N8_CCHInterruptHandler     ( NspInstance_t   *NSPinstance_p,
                                  uint32_t         reg_amba );

/* INSTANCE, INDEXED BY MINOR NUMBER                                    */
extern NspInstance_t NSPDeviceTable_g [DEF_MAX_SIMON_INSTANCES];
 
/* NSPcount_g MAINTAINS THE NUMBER OF DETECTED HARDWARE INSTANCES */
extern int NSPcount_g;
extern wait_queue_head_t requestBlock;

/* IRQ statistics */
static int n8_IRQs_g = 0;
static int n8_AMBA_IRQs_g = 0;
static int n8_CCH_IRQs_g = 0;
static int n8_PKH_IRQs_g = 0;
int ambaTimerActive = FALSE;

#define N8_RELOAD_AMBA_MASK  (AMBAIRQ_PKP | AMBAIRQ_CCH | AMBAIRQ_RNG | \
                              AMBAIRQ_Timer | AMBA_Timer_Reload)

void cch_do_tasklet (unsigned long unused);
#ifdef __linux
DECLARE_TASKLET(cch_tasklet, cch_do_tasklet, 0);
#endif

void cch_do_tasklet (unsigned long unused)
{
   int                     nspIdx;
   NSP2000REGS_t          *nspRegPtr;
   NspInstance_t          *localNSPinstance_p;
   int                     eaReqsComplete = 0;
   uint32_t                cch_read;
   uint16_t                newReadIndex;
   uint16_t                cmdsComplete;

   /* The bridge timer interrupt is only enabled for the first */
   /* NSP, so when it fires, update the read pointer for each  */
   /* device installed in the system.                          */
   for (nspIdx = 0; nspIdx < NSPcount_g; nspIdx++)
   {
      localNSPinstance_p = &NSPDeviceTable_g[nspIdx];
      nspRegPtr = (NSP2000REGS_t *)localNSPinstance_p->NSPregs_p;
 
      /* Read & store the read pointers on each timer interrupt. */
      cch_read = (nspRegPtr->cch_q_ptr>>16);
 
      /* Check for completed EA command blocks on */
      /* the AMBA timer interrupt                 */
      newReadIndex = cch_read;
      if (newReadIndex != localNSPinstance_p->EAreadIndex)
      {
         cmdsComplete = (newReadIndex - localNSPinstance_p->EAreadIndex) &
                        (localNSPinstance_p->EAqueue_size-1);
         localNSPinstance_p->EAreadIndex = newReadIndex;
         eaReqsComplete += QMgrCheckQueue(N8_EA, nspIdx, cmdsComplete);
      }
   }

   /* Reload the AMBA timer only if there were */
   /* outstanding EA requests                  */
   if ((eaReqsComplete) || QMgrCount(N8_EA))
   {
      ambaTimerActive = TRUE;
      localNSPinstance_p = &NSPDeviceTable_g[N8_AMBA_TIMER_CHIP];
      nspRegPtr = (NSP2000REGS_t *)localNSPinstance_p->NSPregs_p;
      nspRegPtr->amba_pci_control = N8_RELOAD_AMBA_MASK;
   }

   if (eaReqsComplete)
   {
      WakeUp(&requestBlock);
   }
}



/*****************************************************************************
 * N8_MainInterruptHandler
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Main NSP2000 interrupt handler.
 *
 * This routine performs the main interrupt handling of the NSP2000.
 *
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param debug           RO: Specifies whether debug messages are enabled.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *
 * @par Assumptions:
 *    We are assuming that AMBA interrupts are used the same way in
 *    each user process that might use them.
 *
 *****************************************************************************/

#define DEF_NSP_IRQ_ACTIVE   (AMBAIRQ_PKP | AMBAIRQ_CCH | AMBAIRQ_RNG | \
                              AMBAIRQ_Bridge | AMBAIRQ_Timer)

void n8_MainInterruptHandler(NspInstance_t *NSPinstance_p)
{
     NSP2000REGS_t  *nsp      = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;
     uint32_t        reg_amba = nsp->amba_pci_status;
#ifdef __linux
     uint32_t        temp;
#endif


     /* CHECK FOR ACTIVE NSP2000 INTERRUPT */
     if (reg_amba & DEF_NSP_IRQ_ACTIVE)
     {

          /* Count the receipt of interrupt */
#ifdef N8_IRQ_COUNT
          n8_IRQs_g++;
#endif

          /* If this interrupt is from the Bridge Timer, reload it */
          if (reg_amba & AMBAIRQ_Timer)
          {
               /* Clear the AMBA timer interrupt */
               nsp->amba_pci_status = reg_amba;

#ifdef N8_IRQ_COUNT
               n8_AMBA_IRQs_g++;
#endif
               ambaTimerActive = FALSE;
#ifdef __linux
	       tasklet_schedule(&cch_tasklet);
	       /* Ensure PCI write completes by reading register */
	       /* before exiting the ISR. */
               temp = nsp->amba_pci_status;
#else
	       cch_do_tasklet(n8_IRQs_g);
#endif
          }

          /* IF APPROPRIATE, RELEASE ANYONE BLOCKED ON AMBA INTERRUPTS */

          /* Note that usage of a single AMBAbitmask doesn't allow for  */
          /* different processes to wait on different masks.  However,  */
          /* we only use the AMBA timer, and all processes that wait on */
          /* AMBA interrupts are just waiting on the timer.             */
          if (NSPinstance_p->AMBAbitmask & reg_amba)
          {
               /* RELEASE BLOCKED CALL */
               /* We are using the timer of the AMBA to generate a      */
               /* periodic interrupt.                                   */
               WakeUp( &NSPinstance_p->AMBAblock );
          }

          /* HANDLE ANY RNG/PKE/EA INTERRUPTS */
          if (reg_amba & AMBAIRQ_PKP)
          {
#ifdef N8_IRQ_COUNT
               n8_PKH_IRQs_g++;
#endif
               N8_PKHInterruptHandler(NSPinstance_p, reg_amba);
          }
          if (reg_amba & AMBAIRQ_CCH)
          {
#ifdef N8_IRQ_COUNT
               n8_CCH_IRQs_g++;
#endif
               N8_CCHInterruptHandler(NSPinstance_p, reg_amba);
          }
          if (reg_amba & AMBAIRQ_RNG)
          {
               N8_RNHInterruptHandler(NSPinstance_p);
          }


     }

     return;
}



/*****************************************************************************
 * N8_PKHInterruptHandler
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief PKH interrupt handler.
 *
 * This routine is called by the main interrupt handler to handle PKH
 * interrupts. It first samples the PKH control/status register for future
 * reference. It then clears all active bits, to handle all active interrupts.
 * If debug messages are enabled, it also translates the meaning of each
 * active bit.
 *
 * Bits 0-17 indicate PKH interrupts. An active bit is cleared by writing 1
 * to it.
 *
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param reg_amba        RO: Value reported in the AMBA Bridge Status Register
 *
 * @par Externals:
 *    PKHIRQ_*   RO: #define - Constants that identify each bit of the <BR>
 *                   PKH Control/Status register.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void N8_PKHInterruptHandler(NspInstance_t *NSPinstance_p, 
                           uint32_t       reg_amba)
{
   NSP2000REGS_t          *nsp;
   unsigned long           reg, newreg;
   uint16_t                readIndx;
   uint16_t                cmdsComplete;
   int                     reqsComplete = 0;

   nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;

   /* Check for an error */
   if (reg_amba & AMBAPKH_Error)
   {
      /* An error has been detected, read and save PKH status register */
      reg = (unsigned long)nsp->pkh_control_status;
      NSPinstance_p->PKHirqstatus = reg;

      if (reg & PK_Status_Any_Error_Mask)
      {
         /* CLEAR PKH ENABLE TO ALLOW PKE TO STOP */
         nsp->pkh_control_status = 0;
         while (nsp->pkh_control_status & PK_Status_PKH_Busy)
         {
              /* WAITING FOR PKE_BUSY TO CLEAR */
         }

         /* CLEAR ALL SIGNALLING IRQS AND DISABLE PKH */
         newreg = reg & PK_Enable_All_Enable_Mask;

         /* If it is a problem with the command, call the QMgr to noop command blocks */
         if (reg & PK_Status_Cmd_Error_Mask)
         {
            NSPinstance_p->PKHcmderrors++;

            /* Read & store the current read pointer. */
            readIndx = nsp->pkh_q_ptr>>16;
 
            /* Check for completed PK command blocks */
            if (readIndx != NSPinstance_p->PKreadIndex)
            {
               cmdsComplete = (readIndx - NSPinstance_p->PKreadIndex) &
                      (NSPinstance_p->PKqueue_size-1);
               NSPinstance_p->PKreadIndex = readIndx;
               /* Process any completed commands */
               reqsComplete = QMgrCheckQueue(N8_PKP, NSPinstance_p->chip, 
			                     cmdsComplete);
            }

            /* Process the errored command */
            QMgrCmdError(N8_PKP, NSPinstance_p->chip, readIndx, reg);
         }

         /* Bus errors require no QMgr action, just update counter */
         else if (reg & PK_Status_Bus_Error_Mask)
         {
            NSPinstance_p->PKHbuserrors++;
         }

         /* update register & reenable the PKH */
         nsp->pkh_control_status = newreg | PK_Status_PKH_Enable;

         /* IF APPROPRIATE, RELEASE ANYONE BLOCKED ON PKH INTERRUPTS */
         if (NSPinstance_p->PKHbitmask & reg)
         {
            /* RELEASE BLOCKED CALL */
            WakeUp(&(NSPinstance_p->PKHblock));

            /* PREVENT REENTRY OF THE BLOCKING FUNCTIONALITY */
            NSPinstance_p->PKHbitmask = 0;
         }
      }
   }
   else /* No Errors, this is a command complete interrupt */
   {
      /* Clear the command complete bit */
      nsp->pkh_control_status = PK_Enable_PKH_Enable | 
	                        PK_Enable_Cmd_Complete_Enable;

      /* Read & store the current read pointer. */
      readIndx = nsp->pkh_q_ptr>>16;
 
      /* Process the completed PK command blocks */
      cmdsComplete = (readIndx - NSPinstance_p->PKreadIndex) &
                (NSPinstance_p->PKqueue_size-1);
      NSPinstance_p->PKreadIndex = readIndx;
      reqsComplete = QMgrCheckQueue(N8_PKP, NSPinstance_p->chip, cmdsComplete);

   }
   if (reqsComplete)
   {
      WakeUp(&requestBlock);
   }
}



/*****************************************************************************
 * N8_RNHInterruptHandler
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief RNH interrupt handler.
 *
 * This routine is called by the main interrupt handler to handle RNH
 * interrupts. It first samples the RNH control/status register for future
 * reference. It then clears all active bits, to handle all active interrupts.
 * If debug messages are enabled, it also translates the meaning of each
 * active bit.
 *
 * Bits 20-23 indicate RNH interrupts. An active bit is cleared by writing 1
 * to it.
 *
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 *
 * @par Externals:
 *    RNHIRQ_*   RO: #define - Constants that identify each bit of the <BR>
 *                   RNH Control/Status register.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void N8_RNHInterruptHandler(NspInstance_t *NSPinstance_p)
{
     NSP2000REGS_t           *nsp;
     unsigned long            reg, newreg;
     unsigned char            reenable = 1;

     /* READ AND SAVE RNH STATUS REGISTER */
     nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;
     reg = (unsigned long)nsp->rnh_control_status;
     NSPinstance_p->RNHirqstatus = reg;

     /* CLEAR RNH ENABLE TO ALLOW RNG TO STOP */
     nsp->rnh_control_status = 0;
     while (nsp->rnh_control_status & RNH_Status_Transfer_Busy)
     {
          /* WAITING FOR XFER_BUSY TO CLEAR */
     }

     /* CLEAR ALL SIGNALLING IRQS AND RE-ENABLE RNH */
     newreg = reg & RNH_Status_Any_Condition_Mask;

     /* DISABLE RNH IF BIG ERROR */
     if (reg & DEF_RNH_BIGERROR_IRQ_BITS)
     {
          /* IRQPRINT(("NSP2000: Serious error - RNG execution halted.\n")); */
          reenable = 0;
     }

     /* Record the bus error */
     else if (reg & RNH_Status_Bus_Error)
     {
          NSPinstance_p->RNHbuserrors++;
     }

     /* UPDATE REGISTER */
     nsp->rnh_control_status = newreg;
     if (reenable)
     {
          nsp->rnh_control_status = RNH_Status_Transfer_Enable;
     }

     /* IF APPROPRIATE, RELEASE ANYONE BLOCKED ON RNH INTERRUPTS */
     if (NSPinstance_p->RNHbitmask & reg)
     {
          /* RELEASE BLOCKED CALL */
          WakeUp(&(NSPinstance_p->RNHblock));

          /* PREVENT REENTRY OF THE BLOCKING FUNCTIONALITY */
          NSPinstance_p->RNHbitmask = 0;
     }
     return;
}  



/*****************************************************************************
 * N8_CCHInterruptHandler
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief CCH interrupt handler.
 *
 * This routine is called by the main interrupt handler to handle CCH
 * interrupts. It first samples the CCH control/status register for future
 * reference. It then clears all active bits, to handle all active interrupts.
 * If debug messages are enabled, it also translates the meaning of each
 * active bit.
 *
 * Bits 0-15 indicate CCH interrupts. An active bit is cleared by writing 1
 * to it.
 *
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param reg_amba        RO: Value reported in the AMBA Bridge Status Register
 *
 * @par Externals:
 *    CCHIRQ_*   RO: #define - Constants that identify each bit of the <BR>
 *                   CCH Control/Status register.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void N8_CCHInterruptHandler(NspInstance_t *NSPinstance_p,
                           uint32_t       reg_amba)
{
   NSP2000REGS_t          *nsp;
   unsigned long           reg;
   uint16_t                readIndx;
   uint16_t                cmdsComplete;

   nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;

   /* Check for an error */
   if (reg_amba & AMBACCH_Error)
   {
      /* An error has been detected, read and save CCH status register */
      reg = (unsigned long)nsp->cch_control_status;
      NSPinstance_p->CCHirqstatus = reg;

      if (reg & EA_Status_Any_Error_Mask)
      {
         /* CLEAR CCH ENABLE TO ALLOW CC TO STOP */
         nsp->cch_control_status = 0;
         while (nsp->cch_control_status & EA_Status_Module_Busy)
         {
               /* WAITING FOR CCH_BUSY TO CLEAR */
         }

         /* Clear all outstanding IRQS */
         nsp->cch_control_status = reg & EA_Status_Any_Condition_Mask;

         /* If it is a problem with the command, call the QMgr to noop command blocks */
         if (reg & EA_Status_Cmd_Error_Mask)
         {
            NSPinstance_p->CCHcmderrors++;
            /* Read & store the current read pointer. */
            readIndx = nsp->cch_q_ptr >> 16;
 
            /* Check for completed EA command blocks */
            if (readIndx != NSPinstance_p->EAreadIndex)
            {
               cmdsComplete = (readIndx - NSPinstance_p->EAreadIndex) &
                      (NSPinstance_p->EAqueue_size-1);
               NSPinstance_p->EAreadIndex = readIndx;
               /* Process any completed commands */
               QMgrCheckQueue(N8_EA, NSPinstance_p->chip, cmdsComplete);
            }

            /* Process the errored command */
            QMgrCmdError(N8_EA, NSPinstance_p->chip, readIndx, reg);
         }

         /* Bus errors require no QMgr action, just update counter */
         else if (reg & EA_Status_Bus_Error_Mask)
         {
            NSPinstance_p->CCHbuserrors++;
         }

         /* Reenable the CCH */
         nsp->cch_control_status = EA_Enable_Module_Enable;

         /* IF APPROPRIATE, RELEASE ANYONE BLOCKED ON CCH INTERRUPTS */
         if (NSPinstance_p->CCHbitmask & reg)
         {
            /* RELEASE BLOCKED CALL */
            WakeUp(&(NSPinstance_p->CCHblock));

            /* PREVENT REENTRY OF THE BLOCKING FUNCTIONALITY */
            NSPinstance_p->CCHbitmask = 0;
         }
      }
   }
   else /* No Errors, this is a command complete interrupt */
   {
      /* Clear the command complete bit */
      nsp->cch_control_status = EA_Enable_Module_Enable | 
                                EA_Enable_Cmd_Complete_Enable;

      /* Read & store the current read pointer. */
      readIndx = nsp->cch_q_ptr >> 16;
 
      /* Process the completed EA command blocks */
      cmdsComplete = (readIndx - NSPinstance_p->EAreadIndex) &
                  (NSPinstance_p->EAqueue_size-1);
      NSPinstance_p->EAreadIndex = readIndx;
      QMgrCheckQueue(N8_EA, NSPinstance_p->chip, cmdsComplete);

   }
   return;
}



/*****************************************************************************
 * waitOnInterrupt
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Interrupt notification routine.
 *
 * This routine allows a process to block for interrupt notification for the
 * PKE, EA, RNG, or AMBA. This routine initializes a wait queue and blocks on
 * it with a timeout value. If the specified execution core signals an IRQ that
 * matches one of the set bits in the bitmask, the IRQ handler will release
 * this blocked process for completion. Otherwise, if no satisfactory IRQ is
 * received, this routine will time out and return with appropriate error code.
 *
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param bitmask         RO: Bitmask to filter received interrupts.
 * @param coretype        RO: Specifies which execution core to monitor.
 * @param timeout         RO: Timeout value for blocking, in seconds.
 * @param debug           RO: Specifies whether debug messages are enabled.
 * @param interruptable   RO: Specifies whether the wait should be interruptable
 *
 * @par Externals:
 *    N8_DAPI_*    RO: #define - Constants to specify an execution core.
 *
 * @return 
 *    -EINVAL    Invalid core specified.
 *          0    Timeout - interrupt not received.
 *          1    Success - interrupt received.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

int waitOnInterrupt ( N8_Unit_t        chip,
                      unsigned char    coretype,
                      unsigned long    bitmask,
                      unsigned long    timeout,
                      int              interruptable )
{
     unsigned char  rc;
     unsigned char  debug = 0; /* was passed from DEBUG_IRQ in nsp_ioctl.c */
     NspInstance_t *NSPinstance_p = &NSPDeviceTable_g[chip];

     if (interruptable == FALSE) 
     {
        if (coretype == N8_DAPI_AMBA) 
        {
           NSPinstance_p->AMBAbitmask = bitmask;
           return 1;
        }
        else
        {
          return -EINVAL;
        }
     }
     if (coretype == N8_DAPI_PKE)
     {
          NSPinstance_p->PKHbitmask = bitmask;
          rc = N8_BlockWithTimeout(&(NSPinstance_p->PKHblock), 
                                     timeout, debug);
     }
     else if (coretype == N8_DAPI_RNG)
     {
          NSPinstance_p->RNHbitmask = bitmask;
          rc = N8_BlockWithTimeout(&(NSPinstance_p->RNHblock), 
                                     timeout, debug);
     }
     else if (coretype == N8_DAPI_EA)
     {
          NSPinstance_p->CCHbitmask = bitmask;
          rc = N8_BlockWithTimeout(&(NSPinstance_p->CCHblock), 
                                     timeout, debug);
     }
     else if (coretype == N8_DAPI_AMBA) 
     {
          NSPinstance_p->AMBAbitmask = bitmask;

          /* We are using the timer of the AMBA to generate a      */
          /* periodic interrupt.                                   */
          rc = N8_BlockWithTimeout(
                  &(NSPinstance_p->AMBAblock),
                  timeout,
                  debug);

          /* Note that the AMBAbitmask will not be cleared.        */
          /* There is no hard (other than some amount of overhead) */
          /* of doing extra wake-ups.  If we do clear the bitmask  */
          /* then it would need to be on a per process basis,      */
          /* because otherwise, one process could starting to do a */
          /* block, while the other is doing a clear.  Then the    */
          /* wake up will never occur, as the bitmask is 0.        */
          /* NSPinstance_p->AMBAbitmask = 0;                       */

          /* The advance to the next wait block occurs in the isr. */

     }
     else
     {
          return -EINVAL;
     }

     return rc;
}



/*****************************************************************************
 * n8_WaitOnInterrupt
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Interrupt notification routine.
 *
 * This routine allows a process to block for interrupt notification for the
 * PKE, EA, RNG, or AMBA. This routine initializes a wait queue and blocks on
 * it with a timeout value. If the specified execution core signals an IRQ that
 * matches one of the set bits in the bitmask, the IRQ handler will release
 * this blocked process for completion. Otherwise, if no satisfactory IRQ is
 * received, this routine will time out and return with appropriate error code.
 *
 * @param NSPinstance_p   RO: Pointer to the information structure for an
 *                            NSP2000 hardware instance, containing a
 *                            pointer to its control register set.
 * @param bitmask         RO: Bitmask to filter received interrupts.
 * @param coretype        RO: Specifies which execution core to monitor.
 * @param timeout         RO: Timeout value for blocking, in seconds.
 * @param debug           RO: Specifies whether debug messages are enabled.
 *
 * @par Externals:
 *    N8_DAPI_*    RO: #define - Constants to specify an execution core.
 *
 * @return 
 *    -EINVAL    Invalid core specified.
 *          0    Timeout - interrupt not received.
 *          1    Success - interrupt received.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

N8_Status_t N8_WaitOnInterrupt ( N8_Unit_t        chip,
                                 unsigned char    coretype,
                                 unsigned long    bitmask,
                                 unsigned long    timeout )
{
     return waitOnInterrupt(chip, coretype, bitmask, timeout, FALSE);
}


N8_Status_t N8_WaitOnRequest ( int timeout )
{

   if (N8_BlockWithTimeout(&requestBlock, timeout, 0))
   {
      return (N8_STATUS_OK);
   }
   return (N8_TIMEOUT);

}



/*****************************************************************************
 * reload_AMBA_timer
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief This function reloads the AMBA timer.
 *
 * @return 
 *    N/A
 *
 * @par Errors:
 *    N/A
 *****************************************************************************/
void reload_AMBA_timer(void)
{
   NspInstance_t *NSPinstance_p;
   NSP2000REGS_t *nspRegPtr;

   if (ambaTimerActive == FALSE)
   {
      NSPinstance_p = &NSPDeviceTable_g[N8_AMBA_TIMER_CHIP];
      nspRegPtr = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;
      ambaTimerActive = TRUE;
      nspRegPtr->amba_pci_control = N8_RELOAD_AMBA_MASK;
   }
}

 
/*****************************************************************************
 * n8_DisplayIRQ
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Displays the IRQ statistics
 *
 * This routine displays the NSP2000's IRQ statistics to the
 * kernel log. This routine is intended for debugging purposes.
 * 
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/
void n8_DisplayIRQ(void)
{
   int nspIdx;
   NspInstance_t *NSPinstance_p;

   N8_PRINT(KERN_CRIT "\n");
   N8_PRINT(KERN_CRIT "NSP2000: Displaying IRQ statistics:\n\n");

   N8_PRINT(KERN_CRIT "    AMBA  IRQ's = %d\n", n8_AMBA_IRQs_g);
   N8_PRINT(KERN_CRIT "    CCH   IRQ's = %d\n", n8_CCH_IRQs_g);
   N8_PRINT(KERN_CRIT "    PKH   IRQ's = %d\n", n8_PKH_IRQs_g);
   N8_PRINT(KERN_CRIT "    Total IRQ's = %d\n", n8_IRQs_g);

   for (nspIdx = 0; nspIdx < NSPcount_g; nspIdx++)
   {
      NSPinstance_p = &NSPDeviceTable_g[nspIdx];

      N8_PRINT(KERN_CRIT "\n");
      N8_PRINT(KERN_CRIT "NSP2000: Displaying IRQ statistics for chip %d:\n",
                              NSPinstance_p->chip);
      N8_PRINT(KERN_CRIT "         CCH Bus errors  = %d\n", NSPinstance_p->CCHbuserrors);
      N8_PRINT(KERN_CRIT "         PKH Bus errors  = %d\n", NSPinstance_p->PKHbuserrors);
      N8_PRINT(KERN_CRIT "         CCH Cmd errors  = %d\n", NSPinstance_p->CCHcmderrors);
      N8_PRINT(KERN_CRIT "         PKH Cmd errors  = %d\n", NSPinstance_p->PKHcmderrors);
   }
}

