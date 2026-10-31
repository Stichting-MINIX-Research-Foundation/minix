/*	$NetBSD: math.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

/*
 * Copright (C) 2014 Linaro Ltd.
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

#include "lib.h"

UINT64
LShiftU64 (
    IN UINT64   Operand,
    IN UINTN    Count
    )
// Left shift 64bit by 32bit and get a 64bit result
{
    return Operand << Count;
}

UINT64
RShiftU64 (
    IN UINT64   Operand,
    IN UINTN    Count
    )
// Right shift 64bit by 32bit and get a 64bit result
{
    return Operand >> Count;
}


UINT64
MultU64x32 (
    IN UINT64   Multiplicand,
    IN UINTN    Multiplier
    )
// Multiply 64bit by 32bit and get a 64bit result
{
    return Multiplicand * Multiplier;
}

UINT64
DivU64x32 (
    IN UINT64   Dividend,
    IN UINTN    Divisor,
    OUT UINTN   *Remainder OPTIONAL
    )
{
    /*
     * GCC turns a division into a multiplication and shift with precalculated
     * constants if the divisor is constant and the dividend fits into a 32 bit
     * variable. Otherwise, it will turn this into calls into the 32-bit div
     * library functions.
     */
    if (Remainder)
        *Remainder = Dividend % Divisor;
    return Dividend / Divisor;
}
