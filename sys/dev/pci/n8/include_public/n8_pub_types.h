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
 * @(#) n8_pub_types.h 1.4@(#)
 *****************************************************************************/

/*****************************************************************************/
/** @file n8_pub_types
 *  @brief Common type declarations used in public interface.
 *
 * Common header file for root definitions for NSP2000 project.
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 12/20/01 hml   Do not include types.h if you are doing a kernel build.
 * 10/12/01 dkm   Original version. Adapted from n8_types.h.
 ****************************************************************************/
#ifndef N8_PUB_TYPES_H
#define N8_PUB_TYPES_H

#ifdef __cplusplus
extern "C"
{
#endif

#if defined (VX_BUILD)
     #include <vxWorks.h>
#elif !defined(__KERNEL__)
     #include <sys/types.h>
#endif

/******************************************************************************
 * KERNEL COMPATIBILITY TYPES - CONDITIONALLY COMPILE FOR EACH OS             *
 ******************************************************************************/

#ifdef __sparc

     /*************************************************************************
      * Solaris                                                               *
      *************************************************************************/

     #include <inttypes.h>
   

#elif __bsdi__

     /*************************************************************************
      * BSDI                                                                  *
      *************************************************************************/

     #include <sys/types.h>

     typedef unsigned long   ULONG;
     typedef unsigned char   UCHAR;
     typedef char            BYTE;
     typedef unsigned short  WORD;
     typedef unsigned char  *PUCHAR;
     typedef unsigned char   UBYTE;
     typedef unsigned long  *PULONG;
     typedef void           *PVOID; 

     /* typedef ULONG ulong;  */
     typedef UCHAR           uchar;

     typedef unsigned char  uint8_t;
     typedef unsigned short uint16_t;
     typedef unsigned int   uint32_t;  

#elif __linux__

     /*************************************************************************
      * Linux                                                                 *
      *************************************************************************/

     #ifndef __KERNEL__
          #include <stdint.h>
     #else
          #include <linux/types.h>
          #include <linux/delay.h>
     #endif

#elif VX_BUILD

     /*************************************************************************
      * VxWorks                                                               *
      *************************************************************************/

     /* The following types are already defined by VxWorks */
     /* and just placed here for documentation.            */
     /*     typedef unsigned char           uint8_t;  */
     /*     typedef unsigned short          uint16_t; */
     /*     typedef unsigned int            uint32_t; */
     /*     typedef int                     pid_t;    */

     typedef unsigned long           ulong;

#endif

#ifdef __cplusplus
}
#endif

#endif

