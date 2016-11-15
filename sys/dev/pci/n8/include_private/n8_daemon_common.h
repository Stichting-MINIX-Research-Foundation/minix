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
 * @(#) n8_daemon_common.h 1.10@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_daemon_common.h
 *  @brief This file defines the message formats used by the
 *  N8 daemon ioctl mechanism, which enables the driver to
 *  depute a specific set of file system procedures to a
 *  dedicated user process or daemon.
 *
 *  This mechanism is split between the kernel driver and
 *  a userspace program.  This file should be included by both the driver
 *  and the dedicated user process.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 04/10/02 mmd   Created N8_DRIVER_DEVNODE.
 * 04/04/02 spm   Removed definition of daemon lock filename.
 * 02/05/02 spm   Changed n8DaemonStatus_t to N8_Status_t.
 * 01/22/02 spm   Added N8_DAEMON_LOCK_FILE so that it is globally
 *                defined.
 * 01/19/02 spm   Removed nUnits member from message structure,
 *                since now, the API will only ask the N8 daemon
 *                to initialize one (1) execution unit at a time.
 * 01/16/02 spm   Stole N8_KERN_CHECK_RETURN from n8_api_driver.h.
 *                This should be made more publicly available to
 *                the driver code.  Removed inline functions.
 * 12/17/01 spm   Original version.
 ****************************************************************************/
#ifndef _N8_DAEMON_COMMON_H
#define _N8_DAEMON_COMMON_H

/*****************************************************************************
 * #defines 
 *****************************************************************************/
 
/* devnode for driver */
#define N8_DRIVER_DEVNODE    "/dev/nsp2000"
 
/* this is the maximum size in bytes of the parm struct.  in the case of 
 * the key handles, this is way more memory than necessary.
 */
#define N8_DAEMON_MAX_PATH_LEN          N8_SKS_ENTRY_NAME_MAX_LENGTH+256 /* max full path length in chars */

/* below, len is the desired strlen(str) of str in characters 
 * (i.e. not including the terminating null charcter)
 * this allows one to force string delimiting after
 * a specified number of characters, thereby
 * avoiding nasty seg faults.
 */
#define N8_TRUNC_STR(str,len)   (((char *) (str))[(len)] = '\0')

#define CHECK_RETURN(RET)                    \
   if ((RET) != N8_STATUS_OK)                \
   {                                         \
       break;                                \
   }                                   


/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/
/* these enums define the specific set of file system procedures recognized
 * by the sks daemon ioctl mechanism
 */
typedef enum
{
  /* bit3 clear means start */
  N8_DAEMON_WRITE_START        = 0x00,
  N8_DAEMON_RESET_START        = 0x01,
  N8_DAEMON_DELETE_START       = 0x02,
  N8_DAEMON_READ_START         = 0x03,
  N8_DAEMON_INIT_START         = 0x04,
  N8_DAEMON_SHUTDOWN_START     = 0x05,

  /* bit3 set means finish */
  N8_DAEMON_WRITE_FINISH        = 0x08,
  N8_DAEMON_RESET_FINISH        = 0x09,
  N8_DAEMON_DELETE_FINISH       = 0x0A,
  N8_DAEMON_READ_FINISH         = 0x0B,
  N8_DAEMON_INIT_FINISH         = 0x0C,
  N8_DAEMON_SHUTDOWN_FINISH     = 0x0D

} n8_DaemonCmd_t;

/* If sizeof(PARAMSTRUCT_t) < sizeof(n8DaemonMsg_t)
 * we can't overlay n8DaemonMsg_t inside
 * PARMSTRUCT_t (kernel/userspace copying will not work
 * in N8_ioctl)
 */
typedef struct
{
    n8_DaemonCmd_t          cmd;
    N8_Status_t             status;
    N8_Unit_t               unit;
    N8_SKSKeyHandle_t       *keyHandle_p;
    N8_Buffer_t             *string_p;
    N8_Buffer_t             *SKS_Descriptor_p;
    
} n8_DaemonMsg_t;

   

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/



#endif /* _N8_DAEMON_COMMON_H */








