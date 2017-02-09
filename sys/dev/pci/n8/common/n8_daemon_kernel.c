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

static char const n8_id[] = "$Id: n8_daemon_kernel.c,v 1.1 2008/10/30 12:02:14 darran Exp $";
/*****************************************************************************/
/** @file n8_daemon_kernel.c
 *  @brief This file implements the driver half of the N8 daemon ioctl
 *  mechanism.
 *
 *----------------------------------------------------------------------------
 * Key Handle File Functional Interfaces
 * (See sdk/common/include_private/n8_daemon_sks.h)
 *----------------------------------------------------------------------------
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
 * 10/25/02 brr   Clean up function prototypes & include files.
 * 04/03/02 spm   Removed use of daemon.  Changed existing interfaces to
 *                return N8_FILE_ERROR.  Removed init interface.
 * 03/18/02 bac   Added debugging information for daemon failures.
 * 03/13/02 brr   Make n8_daemon_init reentrant to support BSD initialization. 
 * 03/08/02 msz   No more system semaphores.
 * 02/22/02 spm   Changed keyEntryName_p type class to const in sks_write.
 * 02/22/02 spm   Converted printk's to DBG's.
 * 02/05/02 spm   Removed const typing.  Reverted to using N8_Status_t,
 *                as opposed to the original daemon status.
 * 01/28/02 spm   Removed finish semaphore and replaced it with n8_usleep.
 *                This will cause the SKS daemon interfaces to sleep for
 *                a specified timeout, rather than potentially blocking the
 *                caller forever.
 * 01/22/02 spm   Added include of n8_sks.h.  NOTE:  we will want
 *                to move this header to common/include_private.
 * 01/20/02 spm   Serialized access to daemon.  Split interface
 *                into internal and functional halves.
 * 01/17/02 spm   Fixed some debug statements.
 * 01/16/02 spm   Revamp for integration w/ /wing/SDK-2.1
 * 12/17/01 spm   Original version.
 ****************************************************************************/
/** @defgroup subsystem_name Subsystem Title (not used for a header file)
 */

#include "n8_pub_common.h"
#include "n8_daemon_sks.h"


/*****************************************************************************
 * n8_daemon_sks_read
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Read a key handle from an SKS entry. 
 *
 * Reads from a file, formatted for the keyhandle contents.  Note that
 * this function does not know what the key entry name is.  It is the
 * responsibility of the caller to set this field in the key handle.  We
 * only set the key type, length, unitID, and offset.
 *
 * WARNING: THIS FUNCTION IS NOT IMPLEMENTED IN THE KERNEL!
 *
 * @param *keyHandle_p RW: A key handle pointer.
 * @param *keyEntryPath_p RO: key entry path (full pathname)
 *
 * @par Externals: None.
 *
 * @return 
 *    N8_FILE_ERROR
 *
 * @par Errors:
 *    N8_FILE_ERROR
 *   
 * @par Locks:  None.
 *
 * @par Assumptions: None.
 *****************************************************************************/
N8_Status_t n8_daemon_sks_read(N8_SKSKeyHandle_t* keyHandle_p,  
                               char *keyEntryPath_p) 
{
   
    return N8_FILE_ERROR;

} /* n8_daemon_sks_read */


/*****************************************************************************
 * n8_daemon_sks_write
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Write a key handle to an SKS entry. 
 *
 * Writes to a file, formatted for the keyhandle contents.
 *
 * WARNING: THIS FUNCTION IS NOT IMPLEMENTED IN THE KERNEL!
 *
 * @param *sks_key_p RW: key handle pointer.
 * @param *keyEntryName_p RO: key entry name (filename, NOT path)
 *
 * @par Externals: None.
 *
 * @return 
 *    N8_FILE_ERROR
 *
 * @par Errors:
 *    N8_FILE_ERROR
 *   
 * @par Locks: None.
 *     
 * @par Assumptions: None.
 *****************************************************************************/
N8_Status_t n8_daemon_sks_write(N8_SKSKeyHandle_t *sks_key_p,
                                const char *keyEntryName_p) 
{
    
    return N8_FILE_ERROR;

} /* n8_daemon_sks_write */


/*****************************************************************************
 * n8_daemon_sks_delete
 *****************************************************************************/
/** @ingroup substem_name
 * @brief Deletes a key handle file.
 *
 *
 * WARNING: THIS FUNCTION IS NOT IMPLEMENTED IN THE KERNEL!
 *
 * @param keyEntryPath_p RO: key entry path (full pathname)
 *
 * @par Externals: None.
 *
 * @return 
 *    N8_FILE_ERROR
 *
 * @par Errors:
 *    N8_FILE_ERROR
 *   
 * @par Locks: None.
 *
 * @par Assumptions: None.
 *****************************************************************************/
N8_Status_t n8_daemon_sks_delete(char *keyEntryPath_p)
{
    
    return N8_FILE_ERROR;

} /* n8_daemon_sks_delete */ 

/*****************************************************************************
 * n8_daemon_sks_reset
 *****************************************************************************/
/** @ingroup n8_sks
 * @brief Remove all the key handle files from the host file system
 *
 * WARNING: THIS FUNCTION IS NOT IMPLEMENTED IN THE KERNEL!
 *
 *  @param targetSKS           RO:  execution unit to reset
 *
 * @par Externals: None.
 *
 * @return 
 *    N8_FILE_ERROR
 *
 * @par Errors:
 *    N8_FILE_ERROR
 *   
 * @par Locks: None.
 *
 * @par Assumptions: None.
 *****************************************************************************/
N8_Status_t n8_daemon_sks_reset(N8_Unit_t targetSKS)
{
    
    return N8_FILE_ERROR;

} /* n8_daemon_sks_reset */







