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
 * @(#) n8_common.h 1.50@(#)
 *****************************************************************************/
        
/*****************************************************************************/
/** @file n8_common.h
 *  @brief Common definitions for NSP2000 Interface
 *
 * Common header file for root definitions for NSP2000 project
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 06/06/03 brr   Move N8_Hardware_t to n8_pub_common.
 * 05/15/03 brr   Eliminate obsolete typedef's.
 * 11/24/01 bac   Added conditional support for MemTrace.
 * 11/06/01 hml   Added #defines for structure verification and removed 
 *                unneeded FPGA define.
 * 10/30/01 hml   Changed NSP_2000 to NSP_2000_HW in N8_Hardware_t.
 * 10/23/01 dkm   Moved N8_ANY_UNIT to n8_pub_common.h.
 * 10/12/01 dkm   Moved public portion to n8_pub_common.h.
 * 10/01/01 hml   Removed RSA and DSA material structs.
 * 09/21/01 hml   Added N8_ANY_UNIT.
 * 09/14/01 msz   Moved some values from n8_packet.h to remove a circular
 *                include dependency.
 * 09/10/01 bac   Changed lengths in material structs to be unsigned ints.
 * 09/06/01 bac   Change enums to start with a value of 1.
 * 09/04/01 hml   Added FPGA_NUM_EA_CONTEXTS.
 * 08/07/01 hml   Added N8 semaphore types.
 * 07/31/01 bac   Removed COPY_IN and COPY_OUT macros as they are defined
 *                elsewhere. 
 * 07/30/01 bac   New macros and definitions for dynamic word size selection for
 *                FPGA. 
 * 07/20/01 bac   Added definition for enum N8_Hardware_t.
 * 07/10/01 mel   Added modulusLength to N8_DSAKeyMaterial_t;
 *                Deleted privateKeyLength and publicKeyLength. 
 * 07/06/01 mel   Added Diffie-Hellman Key Material structure
 * 06/22/01 bac   Changed to N8_DSAKeyMaterial_t from N8_DSA_KeyMaterial_t.
 * 06/18/01 dws   Conditionally #include sys/types.h to get N8_Boolean_t on
 *                Solaris platforms. Conditionally #include limits.h to get 
 *                INT_MAX on Solaris platforms.
 * 06/15/01 hml   No longer use copyin and copyout.
 * 06/13/01 hml   Changed N8_MAX to N8_NUM_COMPONENTS in N8_Component_t.
 * 05/24/01 dws   Modified the uint32 conversion macros to deal with less
 *                than ideal arguments.  The character pointers are now cast
 *                to unsigned char *.
 * 05/14/01 dws   Added swap_uint32 macro.
 * 05/08/01 dws   Defined uint32_t for BSDi systems and conditionally
 *                #include inttypes.h.  Extended the conditional handling of
 *                MIN, MAX and N8_Boolean_t to BDSi.
 * 05/07/01 dws   Made the definition of MIN and MAX conditional.
 * 05/04/01 bac   Corrected N8_Boolean_t conditional compilation.
 * 05/03/01 bac   Added definition for N8_Boolean_t since linux doesn't
 *                define it.
 * 05/02/01 dws   Removed BE2uint32 and uint322BE macros. Added LE_to_uint32
 *                and uint32_to_LE macros to force little endian loads and 
 *                stores.
 * 04/26/01 bac   Changed MALLOC and FREE macros to test/set null
 *                pointers.  Needs some review and revision.
 * 04/11/01 bac   More standardization.
 * 04/09/01 bac   Added _t to typedefs.
 * 04/05/01 bac   Fixed the comment for key lengths in N8_KeyMaterial.
 * 04/05/01 bac   Turned off debugging.  Minor formatting changes.
 * 04/03/01 bac   Removed the CRT value 'u' from key material.  As we
 *                have to sort p and q, and u is dependent upon their
 *                ordering, u must be computed at initialization.
 * 03/29/01 dws   Removed RNG-specific definitions.
 *                Removed uint32 - use uint32_t from inttypes.h.
 ****************************************************************************/

#ifndef N8_COMMON_H
#define N8_COMMON_H

#include "n8_pub_common.h"
#include "n8_pub_types.h"
#include "n8_OS_intf.h"

#ifdef MEMTRACE   
#include "memtrace.h"
#define calloc MemTrace_Calloc
#define malloc MemTrace_Malloc
#define free MemTrace_Free
#endif

#if !defined(TRUE) || ((TRUE) != 1) 
#define TRUE   1
#endif

#if !defined(FALSE) || ((FALSE) != 0) 
#define FALSE  0
#endif

#define N8_SINGLE_CHIP 0

#define SIMON_BITS_PER_DIGIT            128
#define BM_BITS_PER_DIGIT               128
#define FPGA_BITS_PER_DIGIT             64

#define SIMON_MULTIPLIER                1
#define BM_MULTIPLIER                   1
#define FPGA_MULTIPLIER                 2

#define CONTEXT_ENTRY_SIZE              512

#define N8_MS_BYTE        0
#define N8_LS_BYTE        4

/* enums used for structure type checking. These values are intended to
   be used in the upper half of an id word */
typedef enum
{
   N8_RSA_STRUCT =      1,
   N8_DSA_STRUCT =      2,
   N8_DH_STRUCT =       3,
   N8_CONTEXT_STRUCT =  4,
   N8_ENCRYPT_STRUCT =  5,
   N8_HASH_STRUCT =     6,
   N8_PACKET_STRUCT =   7
} N8_Struct_Types_t;

/* version numbers for structure type checking. These values are intended to
   be used in the lower half of an id word */
#define N8_RSA_STRUCT_VER           1
#define N8_DSA_STRUCT_VER           1
#define N8_DH_STRUCT_VER            1
#define N8_CONTEXT_STRUCT_VER       1
#define N8_ENCRYPT_STRUCT_VER       1
#define N8_HASH_STRUCT_VER          1
#define N8_PACKET_STRUCT_VER        1

/* id numbers for structure type checking. These values are composed of the
   structure type in the upper half and the version in the lower half of th
   word */
#define N8_RSA_STRUCT_ID     ((N8_RSA_STRUCT << 16) | N8_RSA_STRUCT_VER)
#define N8_DSA_STRUCT_ID     ((N8_DSA_STRUCT << 16) | N8_DSA_STRUCT_VER)
#define N8_DH_STRUCT_ID      ((N8_DH_STRUCT << 16) | N8_DH_STRUCT_VER)
#define N8_CONTEXT_STRUCT_ID ((N8_CONTEXT_STRUCT << 16) | N8_CONTEXT_STRUCT_VER)
#define N8_ENCRYPT_STRUCT_ID ((N8_ENCRYPT_STRUCT << 16) | N8_ENCRYPT_STRUCT_VER)
#define N8_HASH_STRUCT_ID    ((N8_HASH_STRUCT << 16) | N8_HASH_STRUCT_VER)
#define N8_PACKET_STRUCT_ID  ((N8_PACKET_STRUCT << 16) | N8_PACKET_STRUCT_VER)

/* Macro to force a big endian load of a 32-bit integer in
   either big or little endian machines */
#define BE_to_uint32(c) ((const uint32_t)(*(((const unsigned char *)(c))+3)       | \
                         (const uint32_t)*(((const unsigned char *)(c))+2) << 8  | \
                         (const uint32_t)*(((const unsigned char *)(c))+1) << 16 | \
                         (const uint32_t)*(((const unsigned char *)(c))  ) << 24))

/* Macro to force a big endian store from a 32-bit integer in
   either big or little endian machines */
#define uint32_to_BE(l,c) \
         do {                                         \
            unsigned char *_c = (unsigned char *)(c); \
            uint32_t       _l = (l);                  \
            *((_c)+3) = ((_l)      ) & 0xff;          \
            *((_c)+2) = ((_l) >>  8) & 0xff;          \
            *((_c)+1) = ((_l) >> 16) & 0xff;          \
            *((_c)  ) = ((_l) >> 24) & 0xff; } while (0)
            
/* Macro to force a little endian load of a 32-bit integer in
   either big or little endian machines */
#define LE_to_uint32(c) ((const uint32_t)*(((const unsigned char *)(c))  )       | \
                         (const uint32_t)*(((const unsigned char *)(c))+1) << 8  | \
                         (const uint32_t)*(((const unsigned char *)(c))+2) << 16 | \
                         (const uint32_t)*(((const unsigned char *)(c))+3) << 24)

/* Macro to force a little endian store from a 32-bit integer in
   either big or little endian machines */
#define uint32_to_LE(l,c) \
         do {                                         \
            unsigned char *_c = (unsigned char *)(c); \
            uint32_t       _l = (l);                  \
            *((_c)  ) = ((_l)      ) & 0xff;          \
            *((_c)+1) = ((_l) >>  8) & 0xff;          \
            *((_c)+2) = ((_l) >> 16) & 0xff;          \
            *((_c)+3) = ((_l) >> 24) & 0xff; } while (0)
                 
/* Macro to force a 32-bit byte swap. */
#define swap_uint32(l) \
           ((((l) & 0x000000ff) << 24) | \
            (((l) & 0x0000ff00) <<  8) | \
            (((l) & 0x00ff0000) >>  8) | \
            (((l) & 0xff000000) >> 24))

/*****************************************************************************
 * typedef N8_SystemSem_t
 *****************************************************************************/
/** @ingroup n8_semaphore
 *  @typedef N8_SystemSem_t
 *  @brief OS independent handle for system level semaphores.
 *
 *****************************************************************************/
typedef void  *N8_SystemSem_t;

/*****************************************************************************
 * typedef N8_SemKey_t
 *****************************************************************************/
/** @ingroup n8_semaphore
 *  @typedef N8_SemKey_t
 *  @brief OS independent key type for finding semaphores (not to be confused
 *         with the handle).
 *
 *****************************************************************************/
typedef void  *N8_SemKey_t;
#endif /* N8_COMMON_H */

