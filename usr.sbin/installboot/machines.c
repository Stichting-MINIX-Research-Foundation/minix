/*	$NetBSD: machines.c,v 1.43 2020/06/21 17:17:02 thorpej Exp $	*/

/*-
 * Copyright (c) 2002-2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn of Wasabi Systems.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: machines.c,v 1.43 2020/06/21 17:17:02 thorpej Exp $");
#endif	/* !__lint */

#include <sys/types.h>
#include "installboot.h"

struct ib_mach * const machines[] = {
#ifdef	SINGLE_ARCH
    &SINGLE_ARCH,
#else
//__MINIX
//    &ib_mach_alpha,
    &ib_mach_amd64,
//    &ib_mach_amiga,
//    &ib_mach_emips,
//    &ib_mach_evbarm,
//    &ib_mach_evbmips,
//    &ib_mach_ews4800mips,
//    &ib_mach_hp300,
//    &ib_mach_hppa,
    &ib_mach_i386,
//    &ib_mach_landisk,
//    &ib_mach_macppc,
//    &ib_mach_news68k,
//    &ib_mach_newsmips,
//    &ib_mach_next68k,
//    &ib_mach_pmax,
//    &ib_mach_sparc,
//    &ib_mach_sparc64,
//    &ib_mach_sun2,
//    &ib_mach_sun3,
//    &ib_mach_vax,
//    &ib_mach_x68k,
#endif
    NULL
};

#if 0
	{ "shark",	no_setboot,	no_clearboot,	no_editboot, 0 },
#endif
