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
 * @(#) n8_time.h 1.4@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_time.h
 *  @brief Time manipulation functions in a os independent manner.
 *
 * This file contains the data structures and function prototypes needed
 * to manipulate time values in an OS independent manner.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 03/12/03 jpw   Added n8_delay_ms
 * 08/17/01 msz   Original version.
 ****************************************************************************/
#ifndef _N8_TIME_H
#define _N8_TIME_H

#include "n8_pub_types.h"
#include "n8_pub_errors.h"

/*****************************************************************************
 * #defines 
 *****************************************************************************/

#define N8_MICROSECS_IN_SECOND  1000000

/*****************************************************************************
 * Structures/type definitions
 *****************************************************************************/

typedef struct
{
   uint32_t tv_sec;             /* Seconds.  */
   uint32_t tv_usec;            /* Microseconds.  */
} n8_timeval_t;


/*****************************************************************************
 * Function prototypes
 *****************************************************************************/

/* Get current timeval                                                  */
N8_Status_t
n8_gettime( n8_timeval_t *n8_timeResults );

/* Get seconds from timeval                                             */
uint32_t
n8_timesecs( n8_timeval_t *n8_timeResults );

/* Get microseconds from timeval                                        */
uint32_t
n8_timeusecs( n8_timeval_t *n8_timeResults );

/* Subtract two n8_timevals, n8_timeResults = n8_time2 - n8_time1       */
N8_Status_t
n8_subtime( n8_timeval_t *n8_timeResults,
            n8_timeval_t *n8_time2,
            n8_timeval_t *n8_time1 );

/* Add two n8_timevals, n8_timeResults = n8_time2 + n8_time1            */
N8_Status_t
n8_addtime( n8_timeval_t *n8_timeResults,
            n8_timeval_t *n8_time2,
            n8_timeval_t *n8_time1 );

/* Sleep usec seconds.                                                  */
N8_Status_t
n8_usleep(unsigned int usecs);

#if 0
/* Sleep milliseconds.                                                  */
N8_Status_t
n8_delay_ms(unsigned int milliseconds);
#endif

#endif
