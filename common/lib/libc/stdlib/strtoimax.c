/* $DragonFly: src/lib/libc/stdlib/strtoimax.c,v 1.2 2008/08/19 15:50:24 joerg Exp $ */

/*-
 * Copyright (c) 2005 The DragonFly Project.  All rights reserved.
 * Copyright (c) 2003 Citrus Project,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: strtoimax.c,v 1.2 2013/12/02 12:20:44 joerg Exp $");

#ifdef _LIBC
#include "namespace.h"
#endif

#if defined(_KERNEL)
#include <sys/param.h>
#include <lib/libkern/libkern.h>
#elif defined(_STANDALONE)
#include <sys/param.h>
#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#else
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#endif

#define	_FUNCNAME	strtoimax
#define	__INT		intmax_t
#define	__INT_MIN	INTMAX_MIN
#define	__INT_MAX	INTMAX_MAX

#include "_strtol.h"

#ifdef _LIBC
__weak_alias(strtoimax, _strtoimax)
__weak_alias(strtoimax_l, _strtoimax_l)
#endif
