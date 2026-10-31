/*	$NetBSD: math.c,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    math.c

Abstract:




Revision History

--*/

#include "lib.h"


//
// Declare runtime functions
//

#ifdef RUNTIME_CODE
#ifndef __GNUC__
#pragma RUNTIME_CODE(LShiftU64)
#pragma RUNTIME_CODE(RShiftU64)
#pragma RUNTIME_CODE(MultU64x32)
#pragma RUNTIME_CODE(DivU64x32)
#endif
#endif

//
//
//

UINT64
LShiftU64 (
    IN UINT64   Operand,
    IN UINTN    Count
    )
// Left shift 64bit by 32bit and get a 64bit result
{
#if defined(__GNUC__) || defined(_MSC_EXTENSIONS)
    return Operand << Count;
#else
    UINT64      Result;
    _asm {
        mov     eax, dword ptr Operand[0]
        mov     edx, dword ptr Operand[4]
        mov     ecx, Count
        and     ecx, 63

        shld    edx, eax, cl
        shl     eax, cl

        cmp     ecx, 32
        jc      short ls10

        mov     edx, eax
        xor     eax, eax

ls10:
        mov     dword ptr Result[0], eax
        mov     dword ptr Result[4], edx
    }

    return Result;
#endif
}

UINT64
RShiftU64 (
    IN UINT64   Operand,
    IN UINTN    Count
    )
// Right shift 64bit by 32bit and get a 64bit result
{
#if defined(__GNUC__) || defined(_MSC_EXTENSIONS)
    return Operand >> Count;
#else
    UINT64      Result;
    _asm {
        mov     eax, dword ptr Operand[0]
        mov     edx, dword ptr Operand[4]
        mov     ecx, Count
        and     ecx, 63

        shrd    eax, edx, cl
        shr     edx, cl

        cmp     ecx, 32
        jc      short rs10

        mov     eax, edx
        xor     edx, edx

rs10:
        mov     dword ptr Result[0], eax
        mov     dword ptr Result[4], edx
    }

    return Result;
#endif
}


UINT64
MultU64x32 (
    IN UINT64   Multiplicand,
    IN UINTN    Multiplier
    )
// Multiple 64bit by 32bit and get a 64bit result
{
#if defined(__GNUC__) || defined(_MSC_EXTENSIONS)
    return Multiplicand * Multiplier;
#else
    UINT64      Result;
    _asm {
        mov     eax, dword ptr Multiplicand[0]
        mul     Multiplier
        mov     dword ptr Result[0], eax
        mov     dword ptr Result[4], edx
        mov     eax, dword ptr Multiplicand[4]
        mul     Multiplier
        add     dword ptr Result[4], eax
    }

    return Result;
#endif
}

UINT64
DivU64x32 (
    IN UINT64   Dividend,
    IN UINTN    Divisor,
    OUT UINTN   *Remainder OPTIONAL
    )
// divide 64bit by 32bit and get a 64bit result
// N.B. only works for 31bit divisors!!
{
#if defined(__GNUC__) || defined(_MSC_EXTENSIONS)
    if (Remainder)
	*Remainder = Dividend % Divisor;
    return Dividend / Divisor;
#else
    UINT32      Rem;
    UINT32      bit;        

    ASSERT (Divisor != 0);
    ASSERT ((Divisor >> 31) == 0);

    //
    // For each bit in the dividend
    //

    Rem = 0;
    for (bit=0; bit < 64; bit++) {
        _asm {
            shl     dword ptr Dividend[0], 1    ; shift rem:dividend left one
            rcl     dword ptr Dividend[4], 1    
            rcl     dword ptr Rem, 1            

            mov     eax, Rem
            cmp     eax, Divisor                ; Is Rem >= Divisor?
            cmc                                 ; No - do nothing
            sbb     eax, eax                    ; Else, 
            sub     dword ptr Dividend[0], eax  ;   set low bit in dividen
            and     eax, Divisor                ; and
            sub     Rem, eax                    ;   subtract divisor 
        }
    }

    if (Remainder) {
        *Remainder = Rem;
    }

    return Dividend;
#endif
}
