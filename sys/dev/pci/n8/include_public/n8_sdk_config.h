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
 * @(#) n8_sdk_config.h 1.0@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_sdk_config.h
 *  @brief Defines the options used to build the SDK.
 *
 * Public header to define options used to build the SDK.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 08/15/03 brr   Change default callback mechanism to interrupt context.
 * 07/28/03 brr   Added a seperate define, SUPPORT_USER_CALLBACK_PTHREAD to 
 *                turn on user space callbacks. The allows users to enable
 *                kernel callbacks with requiring application link with the
 *                pthread library.
 * 04/01/03 brr   Original version.
 ****************************************************************************/
#ifndef N8_SDK_CONFIG_H
#define N8_SDK_CONFIG_H

/*****************************************************************************
 * MAXIMUM NUMBER OF SUPPORTED HARDWARE INSTANCES -                          *
 * The SDK only supports up to this number of instances of NetOctave         *
 * devices.                                                                  *
 *****************************************************************************/

#define DEF_MAX_SIMON_INSTANCES        8

/* When using the SKS, also use the round-robin scheduling when in an multi-chip
 * (NSP2000) environment.  Enabling this flag assumes all SKS entries for each
 * chip are fully mirrored. */
#define N8_SKS_ROUND_ROBIN             1

/*****************************************************************************
 * CALLBACK CONFIGURATION SUPPORT-                                           *
 * The NetOctave API supports different callback options. If callbacks are   *
 * not required, leave SUPPORT_CALLBACKS undefined. If SUPPORT_CALLBACKS is  *
 * defined, then one of the callback options also needs to be defined.       *
 *****************************************************************************/

/* #define SUPPORT_CALLBACKS */
#ifdef SUPPORT_CALLBACKS

/* Only use this callback option if the API is called from the kernel *
 * and the callback function is safe to call from interrupt context.  */
#define SUPPORT_INT_CONTEXT_CALLBACKS

/* Use this callback option to spawn a thread to perform the callback *
 * function. This requires a configuration structure to be passed to  *
 * the N8_InitializeAPI function.                                     */
/* #define SUPPORT_CALLBACK_THREAD */

/* Use this callback option to spawn a pthread in user space to       *
 * perform the callback function. This requires all applications to   *
 * link with the pthread library. SUPPORT_CALLBACK_THREAD must also   *
 * be defined.                                                        */
/* #define SUPPORT_USER_CALLBACK_PTHREAD */
#endif



/* #define SUPPORT_DEVICE_POLL */

/* Used in "chip" field of functions to declare which RNG core  */
/* is being used for random number generation.                  */
/* If it is set to something other and zero, systems with fewer */
/* chips than this value will not support random number         */
/* generation.                                                  */
#define N8_RNG_UNIT                0




#endif


