/* $NetBSD: ls.h,v 1.1 2014/03/20 03:13:31 christos Exp $ */

/*-
 * Copyright (c) 2014
 *      The NetBSD Foundation, Inc. All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#define NELEM(x) (sizeof (x) / sizeof(*x))

typedef struct lsentry lsentry_t;

__compactcall void lsadd(lsentry_t **, const char *, const char *, size_t,
    uint32_t, const char *);
__compactcall void lsprint(lsentry_t *);
__compactcall void lsfree(lsentry_t *);
__compactcall void lsunsup(const char *);
#if defined(__minix)
__compactcall void load_modsunsup(const char *);
#endif /* defined(__minix) */
#if defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
__compactcall void lsapply(lsentry_t *, const char *, void (*)(char *), char *);
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
