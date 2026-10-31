/*	$NetBSD: initplat.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

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

VOID
InitializeLibPlatform (
    IN EFI_HANDLE           ImageHandle EFI_UNUSED,
    IN EFI_SYSTEM_TABLE     *SystemTable EFI_UNUSED
    )
{
}
