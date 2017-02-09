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

static char const n8_id[] = "$Id: n8_daemon_sks.c,v 1.2 2014/03/25 16:19:14 christos Exp $";
/*****************************************************************************/
/** @file n8_daemon_sks.c
 *  @brief This file implements the user side of the daemon's SKS
 *         functional interfaces.  These should be called directly from a
 *         userspace SKS API (n8_sks.c) or VxWorks environment, since the
 *         reverse-ioctl is not necessary in these cases.
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
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 04/05/02 brr   Fixed warning.
 * 04/04/02 bac   Fixed a bug in the reset routine where a null directory
 *                descriptor was trying to close.
 * 04/04/02 spm   Fixed bug 683.  Now sks subdirectories are not created in
 *                the init call (which is now always run as root), but in
 *                the key handle file write call (which will typically be run
 *                as a normal user like admin).  This allows the adminstrator
 *                to maintain proper file permissions for these subdirectories. 
 * 03/26/02 spm   Simplified n8_daemon_sks_init.
 * 03/14/02 bac   Changed printfs to DBGs.
 * 02/22/02 spm   Changed keyEntryName_p type class to const in sks_write.
 * 02/14/01 spm   Fixed string handling bugs.
 * 02/05/01 spm   Original version.
 ****************************************************************************/
/** @defgroup n8_sks_daemon SKS Daemon Routines
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "n8_pub_common.h"
#include "n8_daemon_common.h"
#include "n8_sks.h"
#include "n8_sks_util.h"
#include "n8_OS_intf.h"


/*****************************************************************************
 * n8_setStatus
 *****************************************************************************/
/** @ingroup n8_sks_daemon
 * @brief Sets the status of the SKS allocation units spanning an SKS key
 *
 * @param keyHandle_p RO: key handle
 * @param status RO: free or in use
 * @param alloc_map RW: SKS allocation descriptor table 
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    A text description of all of the return codes of the function.
 *
 * @par Errors:
 *    return status of SKS_ComputeKeyLength call
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
static N8_Status_t n8_setStatus(N8_SKSKeyHandle_t *keyHandle_p,
                                unsigned int status,
                                N8_Buffer_t *alloc_map)
{
    int i;
    unsigned int alloc_units_to_free;
    unsigned int num_sks_words;
    unsigned int sks_alloc_unit_offset;
    N8_Status_t ret;

    ret = n8_ComputeKeyLength(keyHandle_p->key_type,
                              keyHandle_p->key_length,
                              &num_sks_words);
    if (ret != N8_STATUS_OK)
    {
        DBG(("n8_setStatus: n8_ComputeKeyLength returned an error\n"));
        return ret;
    }

    DBG(("num_sks_words:\t%d\n",num_sks_words));

    /* given the number SKS words, compute the number of allocation units */
    alloc_units_to_free = CEIL(num_sks_words, SKS_WORDS_PER_ALLOC_UNIT);

    DBG(("alloc_units_to_free;\t%d\n",alloc_units_to_free));

    /* given the offset in words, find the first allocation unit */
    sks_alloc_unit_offset = keyHandle_p->sks_offset / SKS_WORDS_PER_ALLOC_UNIT;

    DBG(("sks_alloc_unit_offset:\t%d\n",sks_alloc_unit_offset));

    for (i = 0; i < alloc_units_to_free; i++)
    {
        alloc_map[sks_alloc_unit_offset + i] = status;
    }

    return ret;
} /* n8_setStatus */


/*****************************************************************************
 * printSKSKeyHandle
 *****************************************************************************/
/** @ingroup n8_sks_daemon
 * @brief Display a key handle for an SKS PROM.
 *
 * @param keyHandle_p RO: A pointer to the key handle to be printed.
 *
 * @par Externals:
 *    None
 *
 * @return 
 *    None.
 *
 * @par Errors:
 *    None.
 *
 * @par Assumptions:
 *    None.
 *****************************************************************************/
static void printSKSKeyHandle(N8_SKSKeyHandle_t *keyHandle_p)
{

    DBG(("Key Handle:\n\tKey Type   %08x\n", 
         keyHandle_p->key_type));
    DBG(("\tKey Length %08x\n\tSKS Offset %08x\n", 
         keyHandle_p->key_length,
         keyHandle_p->sks_offset));
    DBG(("\tTarget SKS %08x\n", 
         keyHandle_p->unitID));
} /* printSKSKeyHandle */



/*****************************************************************************
 * n8_daemon_sks_write
 *****************************************************************************/
/** @ingroup n8_sks_daemon
 * @brief Write a key handle to an SKS entry. 
 *
 * Writes to a file, formatted for the keyhandle contents.
 *
 * @param sks_key_p RW: SKS key handle to write
 * @param keyEntryName_p RW: Name of key handle file
 *
 * @return 
 *    N8_STATUS_OK if the file scan returned successfully
 *    N8_FILE_ERROR if there was an error writing the file.
 *
 * @par Assumptions:
 *    That the file descriptor is valid.
 *****************************************************************************/
N8_Status_t n8_daemon_sks_write(N8_SKSKeyHandle_t *sks_key_p,
                                const char *keyEntryName_p) 
{
    FILE *fd;
    char key_entry[N8_DAEMON_MAX_PATH_LEN];
    char sub_dir[N8_DAEMON_MAX_PATH_LEN];
    char key_entry_name[N8_SKS_ENTRY_NAME_MAX_LENGTH];
    DIR *dir_p = NULL;

    strncpy(key_entry_name, keyEntryName_p, N8_SKS_ENTRY_NAME_MAX_LENGTH); 

    /* Write the key entry. */
    N8_TRUNC_STR(key_entry_name, N8_SKS_ENTRY_NAME_MAX_LENGTH-1);

    /* make sure that the SKS unit this key is on has a subdirectory
     * in /opt/NetOctave/sks
     *
     * NOTE: when we construct the full path for this directory,
     * we don't terminate it with '/'.  This is because on BSDi
     * you can't create a directory whose name appended with '/'
     */
    snprintf(sub_dir, sizeof(sub_dir), "%s%i", SKS_KEY_NODE_PATH,
            sks_key_p->unitID);

    if ((dir_p = opendir(sub_dir)) == NULL)
    {
        /* the directory for this SKS does not exist.  create it. */
        DBG(("Directory '%s' does not exist.  Creating it.\n", sub_dir));

        if ((N8_mkdir(sub_dir, S_IRWXU | S_IRWXG | S_IROTH )) == -1)
        {
            DBG(("Cannot N8_mkdir '%s'... make sure %s exists "
                 "and this process is run as owner or group\n",
                 sub_dir, 
                 SKS_KEY_NODE_PATH));
            return N8_FILE_ERROR;
        }
    }
    else
    {
        /* we know the directory exists, so close it */
        closedir(dir_p);
    }
    
    /* build full path name for key handle file */
    snprintf(key_entry, sizeof(key_entry), "%s%i/%s", 
            SKS_KEY_NODE_PATH,
            sks_key_p->unitID,
            key_entry_name);

    if ((fd = fopen(key_entry, "w")) == NULL)
    {
        DBG(("n8_daemon_sks_write: error opening file %s\n", key_entry));
        return N8_FILE_ERROR;
    }

    if (fprintf(fd, "%i\n%08x\n%i\n%08x\n",
                (int) sks_key_p->key_type, sks_key_p->key_length,
                sks_key_p->unitID, sks_key_p->sks_offset) <= 0)
    {
        DBG(("n8_daemon_sks_write: error "
             "writing to file %s\n", key_entry));
        fclose(fd);
        return N8_FILE_ERROR;
    }
    else
    {
        fclose(fd);
        return N8_STATUS_OK;

    }
} /* n8_daemon_sks_write */

/*****************************************************************************
 * n8_daemon_sks_read
 *****************************************************************************/
/** @ingroup n8_sks_daemon
 * @brief Read a key handle from an SKS entry. 
 *
 * Reads from a file, formatted for the keyhandle contents.
 *
 * @param *keyHandle_p RO: A Read Only parameter
 * @param *keyEntryPath_p RW: A key handle file path.
 *
 * @return 
 *    N8_STATUS_OK if the file scan returned successfully
 *    N8_FILE_ERROR if there was an error scanning the sks entry.
 *
 * @par Assumptions:
 *    That the file descriptor is valid.
 *****************************************************************************/
N8_Status_t n8_daemon_sks_read(N8_SKSKeyHandle_t* keyHandle_p,
                               char *keyEntryPath_p) 
{
    FILE *fd;
    int ret;
    char key_entry_path[N8_DAEMON_MAX_PATH_LEN];

    strncpy(key_entry_path, keyEntryPath_p, N8_DAEMON_MAX_PATH_LEN);

    /* force string delimiting */
    N8_TRUNC_STR(key_entry_path, N8_DAEMON_MAX_PATH_LEN-1);

    if ((fd = fopen(key_entry_path, "r")) == NULL)
    {
        DBG(("n8_daemon_sks_read: error opening file %s\n", key_entry_path));
        return N8_FILE_ERROR;
    }


    if ((ret = fscanf(fd, "%i\n%08x\n%i\n%08x",
                      (int*) &keyHandle_p->key_type, &keyHandle_p->key_length,
                      (int*) &keyHandle_p->unitID, &keyHandle_p->sks_offset)) == 4)
    {
        fclose(fd);

        /* force string delimiting */
        N8_TRUNC_STR(keyHandle_p->entry_name, N8_SKS_ENTRY_NAME_MAX_LENGTH-1);

        return N8_STATUS_OK;
    }

    DBG(("n8_daemon_sks_read: fscanf returned error code "
         "%d on file %s\n", ret, key_entry_path));
    fclose(fd);
    return N8_FILE_ERROR;

} /* n8_daemon_sks_read */


/*****************************************************************************
 * n8_daemon_sks_delete
 *****************************************************************************/
/** @ingroup n8_sks_daemon
 * @brief One line description of the function.
 *
 * More detailed description of the function including any unusual algorithms
 * or suprising details.
 *
 * @param keyEntryPath_p RW: Full path to key handle file
 *
 * @par Externals:
 *    None.
 *
 * @return 
 *    N8_STATUS_OK on success
 *
 * @par Errors:
 *    N8_FILE_ERROR if remove fails.  remove
 *    returns 0 on success and -1 on failure.
 *   
 * @par Locks:
 *    None.
 *
 * @par Assumptions:
 *****************************************************************************/
N8_Status_t n8_daemon_sks_delete(char *keyEntryPath_p)
{
    char key_entry_path[N8_DAEMON_MAX_PATH_LEN];

    strncpy(key_entry_path, keyEntryPath_p, N8_DAEMON_MAX_PATH_LEN);

    /* force string delimiting */
    N8_TRUNC_STR(key_entry_path, N8_DAEMON_MAX_PATH_LEN-1);

    /* Delete the existing key entry. */
    if (remove(key_entry_path) == 0)
    {
        return N8_STATUS_OK;
    }
    else
    {
        return N8_FILE_ERROR;
    }

} /* n8_n8_daemon_sks_delete */


/*****************************************************************************
 * n8_daemon_sks_init
 *****************************************************************************/
/** @ingroup n8_sks_daemon
 * @brief Initialize an instance of the SKS management interface API.
 *
 * The current system file entries must be read, and if they exist, the
 * descriptor tables created for the calling application.
 *
 * @param targetSKS RO: the SKS unit desc. table to initialize
 * @param alloc_map RW: descriptor table buffer to initialize
 *
 * These and all other SKS interface calls should be considered 
 * DESTRUCTIVE with respect to the contents of the SKS PROM.
 *
 * @par Externals:
 *      None.
 *
 * @return 
 *    N8_STATUS_OK indicates that the API has been initialixed, or that all
 *      intialization proceeded without errors.
 *    N8_FILE_ERROR indicates that either an SKS entry could not be
 *      read, or that the descriptor tables could not be created.
 *
 *****************************************************************************/
N8_Status_t n8_daemon_sks_init(N8_Unit_t targetSKS,
                               N8_Buffer_t *alloc_map)
{
    char sks_entry[N8_DAEMON_MAX_PATH_LEN];
    char sks_entry_name[N8_SKS_ENTRY_NAME_MAX_LENGTH];

    DIR *dir_p = NULL;
    struct dirent *dirent_p;
    N8_Status_t ret = N8_STATUS_OK;
    N8_SKSKeyHandle_t keyHandle;

    DBG(("n8_daemon_sks_init\n"));
    do
    {
        /* Initialize the discriptor mapping to zero */
        memset(&alloc_map[0],
               SKS_FREE, 
               SKS_ALLOC_UNITS_PER_PROM * sizeof(N8_Buffer_t));

        /* Open the SKS entry directory.  Read all the key files and 
         * allocate descriptor space for them.
         */
        snprintf(sks_entry_name, sizeof(sks_entry_name), "%s%i/",
	    SKS_KEY_NODE_PATH, targetSKS);

        DBG(("Target sks dev node is '%s'.\n\n", sks_entry_name));

        if ((dir_p = opendir(sks_entry_name)) == NULL)
        {
            /* this just means that no key has been allocated
             * to this target SKS, so just return OK
             * (the allocation map we return has already been
             * initialized)
             */
            return ret;
        }

        /* read the entries */
        DBG(("Reading sks entries in '%s'.\n\n", sks_entry_name));
        while ((dirent_p = readdir(dir_p)) &&
               (dirent_p->d_name != NULL))
        {
            if ((strcmp(dirent_p->d_name, ".") != 0) &&
                (strcmp(dirent_p->d_name, "..") != 0))
            {
                snprintf(sks_entry, sizeof(sks_entry), "%s%s", sks_entry_name,
                        dirent_p->d_name);

                ret = n8_daemon_sks_read(&keyHandle,
                                         sks_entry);
                CHECK_RETURN(ret);

                DBG(("Allocating descriptor space for key entry %s.\n",
                     dirent_p->d_name));

                printSKSKeyHandle(&keyHandle);

                ret = n8_setStatus(&keyHandle,
                                   SKS_INUSE,
                                   alloc_map);
                CHECK_RETURN(ret);
            }
        }
    } while (FALSE);

    /* cleanup */
    closedir(dir_p);

    return ret;

} /* n8_daemon_sks_init */


/*****************************************************************************
 * n8_daemon_sks_reset
 *****************************************************************************/
/** @ingroup n8_sks_daemon
 * @brief Perform SKS reset for a specific unit.
 *
 *  @param targetSKS           RO:  Unit number
 *
 * @par Externals
 *    None
 *
 * @return
 *    Status
 *
 * @par Errors
 *    N8_STATUS_OK on success.<br>
 *    N8_FILE_ERROR if errors occur while reading/writing files or
 *                  directories.<br>
 *    
 *
 * @par Assumptions
 *    <description of assumptions><br>
 *****************************************************************************/
N8_Status_t n8_daemon_sks_reset(N8_Unit_t targetSKS)
{
    DIR* dir_p = NULL;
    struct dirent* dirent_p;
    char key_entry[N8_DAEMON_MAX_PATH_LEN];
    char key_entry_path[N8_DAEMON_MAX_PATH_LEN];
    N8_Status_t ret = N8_STATUS_OK;

    DBG(("Reset :\n"));
    snprintf(key_entry, sizeof(key_entry), "%s%i/",
	SKS_KEY_NODE_PATH, targetSKS);

    /* Find out which, if any, key entries exist. Then blast 'em. */
    DBG(("Resetting SKS %i.\n", targetSKS));

    if ((dir_p = opendir(key_entry)) == NULL)
    {
        DBG(("Could not open '%s'.\n", key_entry));
        return N8_STATUS_OK;
    }

    if ((dirent_p = readdir(dir_p)) == NULL)
    {
        /* There are no SKS entries in the system. */
        DBG(("Empty. There are no SKS entries for the target SKS.\n"));
        /* clean up */
        closedir(dir_p);
        return N8_STATUS_OK;
    }
    else
    {
        /*
         * The SKS entries exist. If there are no key entries, it is assumed
         * that the PROM is empty!
         */

        while ((dirent_p = readdir(dir_p)) != NULL)
        {
            /* skip . and .. */
            if ((strcmp(dirent_p->d_name, ".") == 0) ||
                (strcmp(dirent_p->d_name, "..") == 0))
            {
                continue;
            }
            DBG(("%s : \n", dirent_p->d_name));

            snprintf(key_entry_path, sizeof(key_entry_path), "%s%i/%s", 
                    SKS_KEY_NODE_PATH,
                    targetSKS,
                    dirent_p->d_name);

            ret = n8_daemon_sks_delete(key_entry_path);
        }
    } /* End 'if-else' for read-dir. */

    /* clean up */
    closedir(dir_p);

    return ret;
} /* n8_daemon_sks_reset */



