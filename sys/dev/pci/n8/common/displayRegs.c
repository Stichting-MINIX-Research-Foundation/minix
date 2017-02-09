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

static char const n8_id[] = "$Id: displayRegs.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file displayRegs.c
 *  @brief NSP2000 Device Driver register display functions
 *
 * This file displays the register set of the NSP2000 device for debugging
 * purposes.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 11/25/02 brr   Added banner to display software version.
 * 10/23/02 brr   Do not display RNG registers by default (locks up RNH).
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 03/19/02 brr   Modifed N8_DisplayRegisters to dump all registers for all 
 *                devices.
 * 02/25/02 brr   Use N8_PRINT.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 09/07/01 mmd   Further removal of references to "simon", and added support
 *                for displaying PKH registers.
 * 08/16/01 mmd   Now includes nsp2000_regs.h instead of simon.h.
 * 08/08/01 brr   Moved all OS kernel specific macros to helper.h.
 * 07/26/01 brr   Original version.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver
 */


#include "n8_OS_intf.h"
#include "helper.h"
#include "nsp2000_regs.h"
#include "n8_driver_main.h"
#include "nsp_ioctl.h"
#include "n8_driver_api.h"
#include "displayRegs.h"
#include "n8_version.h"


extern int NSPcount_g;
extern NspInstance_t NSPDeviceTable_g [DEF_MAX_SIMON_INSTANCES];
/*****************************************************************************
 * N8_DisplayRegisters
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Displays the contents of all NSP2000 control registers.
 *
 * This routine displays the contents of all NSP2000 control registers to the
 * kernel log. This routine is intended for debugging purposes.
 * 
 * @return 
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/
void N8_DisplayRegisters(void)
{
     int nspIdx;
     NspInstance_t *NSPinstance_p;
     NSP2000REGS_t *nsp;

     N8_PRINT(KERN_CRIT "\nN8_DisplayRegisters: " N8_VERSION_STRING);

     for (nspIdx = 0; nspIdx < NSPcount_g; nspIdx++)
     {
          NSPinstance_p = &NSPDeviceTable_g[nspIdx];
          nsp = (NSP2000REGS_t *)NSPinstance_p->NSPregs_p;


          N8_PRINT(KERN_CRIT "\n");
          N8_PRINT(KERN_CRIT "NSP2000: Displaying AMBA registers for chip %d:\n",
                              NSPinstance_p->chip);
          N8_PRINT(KERN_CRIT "         Timer preset    = %08lx\n",
                    (unsigned long)nsp->amba_pci_timer_preset);
          N8_PRINT(KERN_CRIT "         Status          = %08lx\n",
                    (unsigned long)nsp->amba_pci_status);
          N8_PRINT(KERN_CRIT "         Control         = %08lx\n",
                    (unsigned long)nsp->amba_pci_control);
          N8_PRINT(KERN_CRIT "         Timer value     = %08lx\n",
                    (unsigned long)nsp->amba_pci_timer_value);

          N8_PRINT(KERN_CRIT "\n");
          N8_PRINT(KERN_CRIT "NSP2000: Displaying CCH registers :\n");
          N8_PRINT(KERN_CRIT "         Test1           = %08lx\n",
                    (unsigned long)nsp->cch_test1);
          N8_PRINT(KERN_CRIT "         Test0           = %08lx\n",
                    (unsigned long)nsp->cch_test0);
          N8_PRINT(KERN_CRIT "         QueueLgth       = %08lx\n",
                    (unsigned long)nsp->cch_q_length);
          N8_PRINT(KERN_CRIT "         QueuePtrs       = %08lx\n",
                    (unsigned long)nsp->cch_q_ptr);
          N8_PRINT(KERN_CRIT "         QueueBase1      = %08lx\n",
                    (unsigned long)nsp->cch_q_bar1);
          N8_PRINT(KERN_CRIT "         QueueBase0      = %08lx\n",
                    (unsigned long)nsp->cch_q_bar0);
          N8_PRINT(KERN_CRIT "         IRQ Enable      = %08lx\n",
                    (unsigned long)nsp->cch_intr_enable);
          N8_PRINT(KERN_CRIT "         Ctrl-Stat       = %08lx\n",
                    (unsigned long)nsp->cch_control_status);
          N8_PRINT(KERN_CRIT "         Cntx Data1      = %08lx\n",
                    (unsigned long)nsp->cch_context_data1);
          N8_PRINT(KERN_CRIT "         Cntx Data0      = %08lx\n",
                    (unsigned long)nsp->cch_context_data0);
          N8_PRINT(KERN_CRIT "         Cntx Addr       = %08lx\n",
                    (unsigned long)nsp->cch_context_addr);

#ifdef DISPLAY_RNG_REGISTERS
          N8_PRINT(KERN_CRIT "\n");
          N8_PRINT(KERN_CRIT "NSP2000: Displaying RNG registers :\n");
          N8_PRINT(KERN_CRIT "         TOD Seconds     = %08lx\n",
               (unsigned long)nsp->rng_tod_seconds);
          N8_PRINT(KERN_CRIT "         TOD Prescale    = %08lx\n",
               (unsigned long)nsp->rng_tod_prescale);
          N8_PRINT(KERN_CRIT "         TOD MSW         = %08lx\n",
               (unsigned long)nsp->rng_tod_msw);
          N8_PRINT(KERN_CRIT "         TOD LSW         = %08lx\n",
               (unsigned long)nsp->rng_tod_lsw);
          N8_PRINT(KERN_CRIT "         Key 1 MSW       = %08lx\n",
               (unsigned long)nsp->rng_key1_msw);
          N8_PRINT(KERN_CRIT "         Key 1 LSW       = %08lx\n",
               (unsigned long)nsp->rng_key1_lsw);
          N8_PRINT(KERN_CRIT "         Key 2 MSW       = %08lx\n",
               (unsigned long)nsp->rng_key2_msw);
          N8_PRINT(KERN_CRIT "         Key 2 LSW       = %08lx\n",
               (unsigned long)nsp->rng_key2_lsw);
          N8_PRINT(KERN_CRIT "         Host Seed MSW   = %08lx\n",
               (unsigned long)nsp->rng_hostseed_msw);
          N8_PRINT(KERN_CRIT "         Host Seed LSW   = %08lx\n",
               (unsigned long)nsp->rng_hostseed_lsw);
          N8_PRINT(KERN_CRIT "         Sample Interval = %08lx\n",
               (unsigned long)nsp->rng_sample_interval);
          N8_PRINT(KERN_CRIT "         Ext. Clk Scalar = %08lx\n",
               (unsigned long)nsp->rng_external_clock_scalar);
          N8_PRINT(KERN_CRIT "         Buff Write Ptr  = %08lx\n",
               (unsigned long)nsp->rng_buffer_write_ptr);
          N8_PRINT(KERN_CRIT "         Sample Seed MSW = %08lx\n",
               (unsigned long)nsp->rng_sample_seed_msw);
          N8_PRINT(KERN_CRIT "         Sample Seed LSW = %08lx\n",
               (unsigned long)nsp->rng_sample_seed_lsw);
          N8_PRINT(KERN_CRIT "         LSFR Diag       = %08lx\n",
               (unsigned long)nsp->rng_lsfr_diag);
          N8_PRINT(KERN_CRIT "         LSFR Hist 1 MSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr1_history_msw);
          N8_PRINT(KERN_CRIT "         LSFR Hist 1 LSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr1_history_lsw);
          N8_PRINT(KERN_CRIT "         LSFR Hist 2 MSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr2_history_msw);
          N8_PRINT(KERN_CRIT "         LSFR Hist 2 LSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr2_history_lsw);
          N8_PRINT(KERN_CRIT "         LSFR Hist 3 MSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr3_history_msw);
          N8_PRINT(KERN_CRIT "         LSFR Hist 3 LSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr3_history_lsw);
          N8_PRINT(KERN_CRIT "         LSFR Hist 4 MSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr4_history_msw);
          N8_PRINT(KERN_CRIT "         LSFR Hist 4 LSW = %08lx\n",
               (unsigned long)nsp->rng_lsfr4_history_lsw);
          N8_PRINT(KERN_CRIT "         CtrlStatus      = %08lx\n",
               (unsigned long)nsp->rng_control_status);
#endif

          N8_PRINT(KERN_CRIT "\n");
          N8_PRINT(KERN_CRIT "NSP2000: Displaying RNH registers :\n");
          N8_PRINT(KERN_CRIT "         Test 0          = %08lx\n",
               (unsigned long)nsp->rnh_test0);
          N8_PRINT(KERN_CRIT "         Test 1          = %08lx\n",
               (unsigned long)nsp->rnh_test1);
          N8_PRINT(KERN_CRIT "         QueueLgth       = %08lx\n",
               (unsigned long)nsp->rnh_q_length);
          N8_PRINT(KERN_CRIT "         QueuePtrs       = %08lx\n",
               (unsigned long)nsp->rnh_q_ptr);
          N8_PRINT(KERN_CRIT "         QueueBase0      = %08lx\n",
               (unsigned long)nsp->rnh_q_bar0);
          N8_PRINT(KERN_CRIT "         QueueBase1      = %08lx\n",
               (unsigned long)nsp->rnh_q_bar1);
          N8_PRINT(KERN_CRIT "         CtrlStatus      = %08lx\n",
               (unsigned long)nsp->rnh_control_status);

          N8_PRINT(KERN_CRIT "\n");
          N8_PRINT(KERN_CRIT "NSP2000: Displaying PKH registers :\n");
          N8_PRINT(KERN_CRIT "         Test 0          = %08lx\n",
               (unsigned long)nsp->pkh_test0);
          N8_PRINT(KERN_CRIT "         Test 1          = %08lx\n",
               (unsigned long)nsp->pkh_test1);
          N8_PRINT(KERN_CRIT "         Cmd Q Length    = %08lx\n",
               (unsigned long)nsp->pkh_q_length);
          N8_PRINT(KERN_CRIT "         Cmd Q Pointers  = %08lx\n",
               (unsigned long)nsp->pkh_q_ptr);
          N8_PRINT(KERN_CRIT "         Cmd Q Base 1    = %08lx\n",
               (unsigned long)nsp->pkh_q_bar1);
          N8_PRINT(KERN_CRIT "         Cmd Q Base 0    = %08lx\n",
               (unsigned long)nsp->pkh_q_bar0);
          N8_PRINT(KERN_CRIT "         IRQ Enables     = %08lx\n",
               (unsigned long)nsp->pkh_intr_enable);
          N8_PRINT(KERN_CRIT "         Control/Status  = %08lx\n",
               (unsigned long)nsp->pkh_control_status);
          N8_PRINT(KERN_CRIT "         SKS Data        = %08lx\n",
               (unsigned long)nsp->pkh_secure_key_storage_data);
          N8_PRINT(KERN_CRIT "         SKS Control     = %08lx\n",
               (unsigned long)nsp->pkh_secure_key_storage_control);
     }
     return;
}

