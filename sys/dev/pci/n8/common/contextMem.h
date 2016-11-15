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
 * @(#) contextMem.h 1.8@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file contextMem.h
 *  @brief Contains declarations for Context management Interface.
 *
 *
 *****************************************************************************/
 

/*****************************************************************************
 * Revision history:   
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 03/29/02 hml   Added proto for n8_contextvalidate. (BUGS 657 658).
 * 03/26/02 hml   Added proto for n8_contextfree.
 * 03/22/02 hml   Added new protos and a define.
 * 02/18/02 brr   Support chip selection.
 * 02/05/02 brr   Original version.
 ***************************************************************************/
 
#ifndef CONTEXTMEM_H
#define CONTEXTMEM_H
#include "n8_pub_common.h"

#define N8_CONTEXT_MAX_PRINT 20
typedef struct 
{
  int     userID;
} ContextMemoryMap_t;

void N8_ContextMemInit  (int chip);
void N8_ContextMemRemove(int chip);
void N8_ContextMemFreeAll(N8_Unit_t chip, unsigned long sessionID);
N8_Status_t n8_contextalloc(N8_Unit_t *chip, 
                            unsigned long sessionID, 
                            unsigned int *index_p);
N8_Status_t 
n8_contextfree(int chip, unsigned long sessionID, unsigned long entry);
N8_Status_t 
n8_contextvalidate(N8_Unit_t chip, unsigned long sessionID, unsigned int entry);
void n8_contextDisplay(void);


#endif /* CONTEXTMEM_H */

