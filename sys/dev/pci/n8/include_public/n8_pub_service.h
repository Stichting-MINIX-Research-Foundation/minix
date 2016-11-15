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
 * @(#) n8_pub_service.h 1.13@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_service
 *  @brief Common type declarations used in public interface.
 *
 * Public header file for service functions for NSP2000 project.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 06/06/03 brr   Eliminate unused N8_HardwareType_t to avoid confusion with
 *                N8_Hardware_t.
 * 04/25/03 brr   Modified N8_EventPoll to return the number of completed 
 *                events.
 * 03/13/04 brr   Added prototype for N8_EventPoll.
 * 03/02/03 bac   Added N8_HARDWAREREVISION parameter to N8_GetSystemParameter.
 * 03/01/03 brr   Added support for API callbacks.
 * 07/08/02 brr   Added N8_FILEDESCRIPTOR parameter to N8_GetSystemParameter.
 * 04/05/02 brr   Added N8_SWVERSIONTEXT parameter to N8_GetSystemParameter.
 * 04/03/02 brr   Added prototype for N8_PrintSoftwareVersion.
 * 03/22/02 brr   Added N8_EVENT_NONE_AVAILABLE. (Bug 635)
 * 03/22/02 brr   Moved N8_EVENT constants from n8_event.h.
 * 10/31/01 hml   Added latest set of enums.
 * 10/12/01 dkm   Original version.
 ****************************************************************************/
#ifndef N8_PUB_SERVICE_H
#define N8_PUB_SERVICE_H

#ifdef __cplusplus
extern "C"
{
#endif
   
#include "n8_pub_common.h"

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/
typedef enum
{
    N8_EACOUNT,         /* Number of EA execution units. */
    N8_EATYPE,          /* An array of types of the EA units. The size n (number 
                           ofelements in the array) is equal to the value 
                           returned for N8_EACOUNT. Entry i in the array, 
                           0 <= i < n is the type of the EA unit */
    N8_PKCOUNT,         /* Number of PK execution units. */
    N8_PKTYPE,          /* An array of types of the PK units. The size n (number 
                           ofelements in the array) is equal to the value 
                           returned for N8_PKCOUNT. Entry i in the array, 
                           0 <= i < n is the type of the PK unit */
    N8_HPCOUNT,         /* Number of hash processor (HP) execution units. */
    N8_HPTYPE,          /* An array of types of the HP units. The size n (number 
                           ofelements in the array) is equal to the value 
                           returned for N8_HPCOUNT. Entry i in the array, 
                           0 <= i < n is the type of the HP unit */
    N8_HARDWAREVERSION, /* The version of the NSP2000 chip, from the 
                           configuration register. */
    N8_SOFTWAREVERSION, /* The version number of the API software, major 
                           minor, maintainence version and build number */
    N8_CONTEXTMEMSIZE,  /* The size in bytes of the E/A 
                           unit's context memory. The size n (number of 
                           elements in the array) is equal to the value 
                           returned for N8_EACOUNT. Entry i in the array, 
                           0 <= i < n is the number of context memory entries 
                           in the E/A of E/A unit i+1. */
    N8_SKSMEMSIZE,      /* The size in bytes of the Secure Key Storage. The 
                           size n (number of elements in the array) is equal 
                           to the value returned for N8_PKCOUNT. Entry i in 
                           the array, 0 <= i < n, is the number of context 
                           memoryentries in the SKS of PKP unit i+1 */
    N8_NUMBEROFCHIPS,   /* The number of NetOctave chips on the system */
    N8_SWVERSIONTEXT,   /* ASCII string describing the API software version */
    N8_FILEDESCRIPTOR,  /* The file descriptor for the NSP2000 device */
    N8_INITIALIZE_INFO, /* The configuration parameters used to initialize */
	                /* the API.                                        */
    N8_HARDWAREREVISION /* The revision id as reported by 'lspci' under Linux.
                         * The results are returned as an array of unsigned
                         * ints. */
} N8_Parameter_t;

/*****************************************************************************
 * Enumeration for hardware types. These are used with the N8_HARDWAREVERSION
 * query of the N8_GetSystemParameter call as well as the N8_EATYPE, N8_PKTYPE
 * and N8_HPTYPE queries.
 *****************************************************************************/
#define   N8_NSP2000           0xffffff00  /* The unit is implemented by an NSP2000 */
#define   N8_NSP2000EMULATED   0xffffff01  /* The unit is an NSP2000 emulator */

/*****************************************************************************
 * Defines for N8_Event functions
 *****************************************************************************/
#define N8_EVENT_MAX    (18 * 1024)  /* this value was arbitrarily selected */
#define N8_EVENT_SLEEP  (1000)       /* sleep 1 millisecond */
#define N8_EVENT_NONE_READY -1

/*****************************************************************************
 * Function prototypes
 *****************************************************************************/
N8_Status_t N8_GetSystemParameter(N8_Parameter_t parameter, void *value_p, size_t value_l);
N8_Status_t N8_InitializeAPI(N8_ConfigAPI_t *parameters_p);
N8_Status_t N8_EventCheck(N8_Event_t *events_p, const int count, int *ready_p);
N8_Status_t N8_EventWait (N8_Event_t *events_p, const int count, int *ready_p);
N8_Status_t N8_TerminateAPI(void);

#ifdef SUPPORT_CALLBACKS
int N8_EventPoll(void);
#endif

#ifdef __cplusplus
}
#endif

#endif


