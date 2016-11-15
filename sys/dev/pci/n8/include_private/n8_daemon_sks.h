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
 * @(#) n8_daemon_sks.h 1.8@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_daemon_sks.h
 *  @brief N8 daemon SKS interface include file
 *
 * This header exports an interface that allows the n8_sks module to 
 * accomplish filesystem related tasks through the use of the N8
 * daemon.
 *
 *----------------------------------------------------------------------------
 * N8 Daemon Functional Interfaces
 *----------------------------------------------------------------------------
 *
 * n8_daemon_sks_init - Initializes SKS allocation mapping for a single
 *                      execution unit using the key handle files found
 *                      on the host file system.
 *
 * n8_daemon_sks_read - Reads data from the specified key handle file into
 *                      a key handle structure.
 *
 * n8_daemon_sks_write - Write data in key handle specified into a key
 *                       handle file with specified name.
 *
 * n8_daemon_sks_delete - Deletes specified key handle file from the host
 *                        file system.
 *
 * n8_dameon_sks_reset - Deletes all key handle files from host
 *                       file system.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/22/02 spm   Changed keyEntryName_p type class to const in sks_write.
 * 02/05/02 spm   Removed const typing.
 * 01/19/02 spm   Original version.
 ****************************************************************************/

#ifndef _N8_DAEMON_SKS_H
#define _N8_DAEMON_SKS_H

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/

/* Functional interfaces provided to kernel tasks that need to use the 
 * N8 daemon
 */
N8_Status_t n8_daemon_sks_init(N8_Unit_t numberSKS,
                               N8_Buffer_t *alloc_map);
N8_Status_t n8_daemon_sks_read(N8_SKSKeyHandle_t* keyHandle_p,  
                       char *keyEntryPath_p);
N8_Status_t n8_daemon_sks_write(N8_SKSKeyHandle_t *sks_key_p,
                       const char *keyEntryName_p);
N8_Status_t n8_daemon_sks_delete(char *keyEntryPath_p);
N8_Status_t n8_daemon_sks_reset(N8_Unit_t targetSKS);


#endif /* _N8_DAEMON_SKS_H */

