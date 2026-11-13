/*	$NetBSD: efibind.h,v 1.2 2018/08/18 20:17:51 jmcneill Exp $	*/

/*
 * Copright (C) 2014 - 2015 Linaro Ltd.
 * Author: Ard Biesheuvel <ard.biesheuvel@linaro.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice and this list of conditions, without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 */

#if !defined(_MSC_VER) && (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L ))

// ANSI C 1999/2000 stdint.h integer width declarations

typedef unsigned long long  uint64_t;
typedef long long           int64_t;
typedef unsigned int        uint32_t;
typedef int                 int32_t;
typedef unsigned short      uint16_t;
typedef short               int16_t;
typedef unsigned char       uint8_t;
typedef signed char         int8_t;   // unqualified 'char' is unsigned on ARM

#elif defined(__NetBSD__)
#include <sys/stdint.h>
#else
#include <stdint.h>
#endif

/*
 * This prevents GCC from emitting GOT based relocations, and use R_ARM_REL32
 * relative relocations instead, which are more suitable for static binaries.
 */
#ifdef __GNUC__
#pragma GCC visibility push (hidden)
#endif

//
// Basic EFI types of various widths
//

#ifndef __WCHAR_TYPE__
# define __WCHAR_TYPE__ short
#endif

typedef uint64_t   UINT64;
typedef int64_t    INT64;

typedef uint32_t   UINT32;
typedef int32_t    INT32;

typedef uint16_t   UINT16;
typedef int16_t    INT16;
typedef uint8_t    UINT8;
typedef int8_t     INT8;
typedef __WCHAR_TYPE__ WCHAR;

#undef VOID
#define VOID    void

typedef int32_t    INTN;
typedef uint32_t   UINTN;

#define EFIERR(a)           (0x80000000 | a)
#define EFI_ERROR_MASK      0x80000000
#define EFIERR_OEM(a)       (0xc0000000 | a)

#define BAD_POINTER         0xFBFBFBFB
#define MAX_ADDRESS         0xFFFFFFFF

#define BREAKPOINT()        while (TRUE);

//
// Pointers must be aligned to these address to function
//

#define MIN_ALIGNMENT_SIZE  4

#define ALIGN_VARIABLE(Value ,Adjustment) \
            (UINTN)Adjustment = 0; \
            if((UINTN)Value % MIN_ALIGNMENT_SIZE) \
                (UINTN)Adjustment = MIN_ALIGNMENT_SIZE - ((UINTN)Value % MIN_ALIGNMENT_SIZE); \
            Value = (UINTN)Value + (UINTN)Adjustment


//
// Define macros to build data structure signatures from characters.
//

#define EFI_SIGNATURE_16(A,B)             ((A) | (B<<8))
#define EFI_SIGNATURE_32(A,B,C,D)         (EFI_SIGNATURE_16(A,B)     | (EFI_SIGNATURE_16(C,D)     << 16))
#define EFI_SIGNATURE_64(A,B,C,D,E,F,G,H) (EFI_SIGNATURE_32(A,B,C,D) | ((UINT64)(EFI_SIGNATURE_32(E,F,G,H)) << 32))

//
// EFIAPI - prototype calling convention for EFI function pointers
// BOOTSERVICE - prototype for implementation of a boot service interface
// RUNTIMESERVICE - prototype for implementation of a runtime service interface
// RUNTIMEFUNCTION - prototype for implementation of a runtime function that is not a service
// RUNTIME_CODE - pragma macro for declaring runtime code
//

#ifndef EFIAPI          // Forces EFI calling conventions reguardless of compiler options
#define EFIAPI          // Substitute expresion to force C calling convention
#endif

#define BOOTSERVICE
#define RUNTIMESERVICE
#define RUNTIMEFUNCTION


#define RUNTIME_CODE(a)         alloc_text("rtcode", a)
#define BEGIN_RUNTIME_DATA()    data_seg("rtdata")
#define END_RUNTIME_DATA()      data_seg("")

#define VOLATILE                volatile

#define MEMORY_FENCE            __sync_synchronize

//
// When build similiar to FW, then link everything together as
// one big module. For the MSVC toolchain, we simply tell the
// linker what our driver init function is using /ENTRY.
//
#if defined(_MSC_EXTENSIONS)
#define EFI_DRIVER_ENTRY_POINT(InitFunction) \
    __pragma(comment(linker, "/ENTRY:" # InitFunction))
#else
#define EFI_DRIVER_ENTRY_POINT(InitFunction)    \
    UINTN                                       \
    InitializeDriver (                          \
        VOID    *ImageHandle,                   \
        VOID    *SystemTable                    \
        )                                       \
    {                                           \
        return InitFunction(ImageHandle,        \
                SystemTable);                   \
    }                                           \
                                                \
    EFI_STATUS efi_main(                        \
        EFI_HANDLE image,                       \
        EFI_SYSTEM_TABLE *systab                \
        ) __attribute__((weak,                  \
                alias ("InitializeDriver")));
#endif

#define LOAD_INTERNAL_DRIVER(_if, type, name, entry)    \
        (_if)->LoadInternal(type, name, entry)


//
// Some compilers don't support the forward reference construct:
//  typedef struct XXXXX
//
// The following macro provide a workaround for such cases.

#define INTERFACE_DECL(x) struct x

#define uefi_call_wrapper(func, va_num, ...) func(__VA_ARGS__)
#define EFI_FUNCTION
