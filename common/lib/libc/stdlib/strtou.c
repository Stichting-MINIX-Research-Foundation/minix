/*	$NetBSD: strtou.c,v 1.2 2015/05/01 14:17:56 christos Exp $	*/

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
 *
 * Created by Kamil Rytarowski, based on ID:
 * NetBSD: src/common/lib/libc/stdlib/strtoul.c,v 1.3 2008/08/20 19:58:34 oster Exp
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: strtou.c,v 1.2 2015/05/01 14:17:56 christos Exp $");

#ifdef _LIBC
#include "namespace.h"
#endif

#if defined(_KERNEL)
#include <sys/param.h>
#include <sys/types.h>
#include <lib/libkern/libkern.h>
#elif defined(_STANDALONE)
#include <sys/param.h>
#include <sys/types.h>
#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#else
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#endif

#define	_FUNCNAME	strtou
#define	__TYPE		uintmax_t
#define	__WRAPPED	strtoumax

#include "_strtoi.h"

#ifdef _LIBC
__weak_alias(strtou, _strtou)
__weak_alias(strtou_l, _strtou_l)
#endif
