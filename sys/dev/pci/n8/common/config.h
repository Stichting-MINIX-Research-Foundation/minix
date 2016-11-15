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
 * @(#) config.h 1.20@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file config.h
 *  @brief NSP2000 Device Driver Configuration Manager
 *
 * This header contains the prototypes for the configuration management
 * routines for the NSP2000 device driver.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 04/30/03 brr   Reconcile differences between 2.4 & 3.0 baselines.
 * 03/03/03 jpw   Added N8_EnableAmbaTimer_g param to N8_ConfigInterrupts      
 * 11/25/02 brr   Renamed prototypes to more close match function
 *                descriptions.
 * 10/23/02 brr   Modified N8_ConfigInterrupts to accept parameter for
 *                the AMBA timer preset.
 * 03/20/02 brr   Modified N8_ConfigInterrupts to configure & enable
 *                interrupts on all detected devices.
 * 03/01/02 brr   Added N8_DisableInterrupts.
 * 02/15/02 brr   Modified N8_GetConfig to return all driver information in a 
 *                single call.
 * 12/14/01 brr   Support memory management performance improvements.
 * 11/26/01 mmd   Updated parms for N8_ConfigInit to accomodate new PCIinfo
 *                field of NspInstance_t.
 * 11/13/01 mmd   Implemented N8_AllocateHardwareResources and
 *                N8_ReleaseHardwareResources.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 10/22/01 mmd   Implemented N8_ClaimHardwareInstance and
 *                N8_ReleaseHardwareInstance.
 * 10/12/01 mmd   Added prototypes for N8_OpenRulesChecker,
 *                N8_CloseRulesChecker, and N8_PurgeNextRelatedSession.
 * 09/25/01 mmd   Creation.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver Configuration Manager
 */


#ifndef CONFIG_H
#define CONFIG_H

#include "n8_driver_main.h"
#include "n8_driver_parms.h"

/************************************************************************
 * The Constants below define the minimum and maximum values permitted  *
 * for the EA & PK command queue sizes.                                 *
 ************************************************************************/
#define N8_MIN_CMD_QUE_EXP            4
#define N8_MAX_CMD_QUE_EXP           15

#define N8_DEF_RNG_QUE_SIZE       (1<<N8_DEF_RNG_QUE_EXP)

extern int n8_chipInit(NspInstance_t *NSPinstance_p,
                       int            HWidx,
                       int            queueSize,
                       unsigned char  Debug);

extern N8_Status_t  n8_chipRemove(NspInstance_t *NSPinstance_p, int HWidx);

extern void n8_enableInterrupts (int timer_preset);
extern void n8_disableInterrupts(void );
extern void N8_GetConfig(NSPdriverInfo_t *driverInfo_p);

extern int n8_driverInit(int eaPoolSize, int pkPoolSize);
extern int n8_driverRemove(void);

#endif   /* CONFIG_H */



