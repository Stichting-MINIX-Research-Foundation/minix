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
 * @(#) displayRegs.h 1.8@(#)
 *****************************************************************************/

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
 * 03/29/02 brr   Updated prototype for N8_DisplayRegisters.
 * 11/10/01 brr   Modified to support static allocations of persistant data
 *                by the driver. 
 * 09/07/01 mmd   Further removal of references to "simon".
 * 08/16/01 mmd   Now includes nsp2000_regs.h instead of simon.h.
 * 07/26/01 brr   Original version.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver
 */


#ifndef DISPLAYREGS_H
#define DISPLAYREGS_H


#include "nsp2000_regs.h"
#include "n8_driver_main.h"


extern void N8_DisplayRegisters(void);



#endif   /* DISPLAYREGS_H */


