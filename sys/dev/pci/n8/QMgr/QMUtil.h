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
 * @(#) QMUtil.h 1.6@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file QMUtil.h
 *  @brief Include file for the queue manager utilities.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 03/20/03 brr   Added prototype for displayQMgr.
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 06/05/02 brr   Removed obsolete prototypes.
 * 02/21/02 msz   Deleted now unused QMgr_QueueInitialized
 * 02/20/02 msz   Added QMgrCheckRequests
 * 12/05/01 msz   Moved some common RNQueue, QMQueue into here QMUtil.c
 * 10/05/01 msz   Added QMgr_QueueGetChipAndUnit
 * 10/03/01 msz   Include the include files needed to compile this header.
 * 09/24/01 hml   Converted QMgr_get_valid_unit_num to use N8_Unit_t.
 * 09/20/01 hml   Added QMgr_get_valid_unit_num.
 * 09/06/01 hml   Added QMgr_get_psuedo_device_handle proto.
 * 08/27/01 hml   Added QMgr_get_num_control_structs function.
 * 09/26/01 msz   Added QMgr_QueueInitialized
 * 07/30/01 bac   Added QMCopy function.
 * 06/15/01 hml   Original version.
 ****************************************************************************/
#ifndef _QMUTIL_H
#define _QMUTIL_H

#include "n8_enqueue_common.h"
#include "n8_pub_common.h"
#include "n8_common.h"
#include "QMQueue.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/

N8_Status_t
QMgr_get_chip_for_request (QueueControl_t  **queue_pp, 
                           N8_Component_t    unit );

N8_Status_t
QMgr_get_control_struct(QueueControl_t  **queue_pp, 
                        N8_Component_t    unit,
                        int               chip);

N8_Status_t
QMgr_get_valid_unit_num(N8_Component_t type, 
                        N8_Unit_t      unitRequest, 
                        N8_Unit_t     *unitReturn);
void displayQMgr(void);


#endif
