/*	$NetBSD: rtstr.c,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    str.c

Abstract:

    String runtime functions


Revision History

--*/

#include "lib.h"

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtAcquireLock)
#endif
INTN
RUNTIMEFUNCTION
RtStrCmp (
    IN CONST CHAR16   *s1,
    IN CONST CHAR16   *s2
    )
// compare strings
{
    while (*s1) {
        if (*s1 != *s2) {
            break;
        }

        s1 += 1;
        s2 += 1;
    }

    return *s1 - *s2;
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrCpy)
#endif
VOID
RUNTIMEFUNCTION
RtStrCpy (
    IN CHAR16   *Dest,
    IN CONST CHAR16   *Src
    )
// copy strings
{
    while (*Src) {
        *(Dest++) = *(Src++);
    }
    *Dest = 0;
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrnCpy)
#endif
VOID
RUNTIMEFUNCTION
RtStrnCpy (
    IN CHAR16   *Dest,
    IN CONST CHAR16   *Src,
    IN UINTN     Len
    )
// copy strings
{
    UINTN Size = RtStrnLen(Src, Len);
    if (Size != Len)
	RtSetMem(Dest + Len, '\0', (Len - Size) * sizeof(CHAR16));
    RtCopyMem(Dest, Src, Size * sizeof(CHAR16));
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrCpy)
#endif
CHAR16 *
RUNTIMEFUNCTION
RtStpCpy (
    IN CHAR16   *Dest,
    IN CONST CHAR16   *Src
    )
// copy strings
{
    while (*Src) {
        *(Dest++) = *(Src++);
    }
    *Dest = 0;
    return Dest;
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrnCpy)
#endif
CHAR16 *
RUNTIMEFUNCTION
RtStpnCpy (
    IN CHAR16   *Dest,
    IN CONST CHAR16   *Src,
    IN UINTN     Len
    )
// copy strings
{
    UINTN Size = RtStrnLen(Src, Len);
    if (Size != Len)
	RtSetMem(Dest + Len, '\0', (Len - Size) * sizeof(CHAR16));
    RtCopyMem(Dest, Src, Size * sizeof(CHAR16));
    return Dest + Size;
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrCat)
#endif
VOID
RUNTIMEFUNCTION
RtStrCat (
    IN CHAR16   *Dest,
    IN CONST CHAR16   *Src
    )
{
    RtStrCpy(Dest+StrLen(Dest), Src);
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrCat)
#endif
VOID
RUNTIMEFUNCTION
RtStrnCat (
    IN CHAR16   *Dest,
    IN CONST CHAR16   *Src,
    IN UINTN    Len
    )
{
    RtStrnCpy(Dest+StrLen(Dest), Src, Len);
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrLen)
#endif
UINTN
RUNTIMEFUNCTION
RtStrLen (
    IN CONST CHAR16   *s1
    )
// string length
{
    UINTN        len;

    for (len=0; *s1; s1+=1, len+=1) ;
    return len;
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrnLen)
#endif
UINTN
RUNTIMEFUNCTION
RtStrnLen (
    IN CONST CHAR16   *s1,
    IN UINTN           Len
    )
// copy strings
{
    UINTN i;
    for (i = 0; *s1 && i < Len; i++)
	    s1++;
    return i;
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtStrSize)
#endif
UINTN
RUNTIMEFUNCTION
RtStrSize (
    IN CONST CHAR16   *s1
    )
// string size
{
    UINTN        len;

    for (len=0; *s1; s1+=1, len+=1) ;
    return (len + 1) * sizeof(CHAR16);
}

#ifndef __GNUC__
#pragma RUNTIME_CODE(RtBCDtoDecimal)
#endif
UINT8
RUNTIMEFUNCTION
RtBCDtoDecimal(
    IN  UINT8 BcdValue
    )
{
    UINTN   High, Low;

    High    = BcdValue >> 4;
    Low     = BcdValue - (High << 4);

    return ((UINT8)(Low + (High * 10)));
}


#ifndef __GNUC__
#pragma RUNTIME_CODE(RtDecimaltoBCD)
#endif
UINT8
RUNTIMEFUNCTION
RtDecimaltoBCD (
    IN  UINT8 DecValue
    )
{
    UINTN   High, Low;

    High    = DecValue / 10;
    Low     = DecValue - (High * 10);

    return ((UINT8)(Low + (High << 4)));
}


