/* $NetBSD: features.c,v 1.1 2014/02/27 09:37:02 matt Exp $ */
/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

/*
 * This file is used to have the compiler check for feature and then
 * have make set a variable based on that test.
 *
 * FEAT_EABI!=if ${COMPILE.c} -fsyntax-only -DEABI_TEST ${TESTFILE} >/dev/null 2>/dev/null; then echo yes; else echo no; fi
 * FEAT_LDREX!=if ${COMPILE.c} -fsyntax-only -DLDREX_TEST ${TESTFILE} >/dev/null 2>/dev/null; then echo yes; else echo no; fi
 * FEAT_LDRD!=if ${COMPILE.c} -fsyntax-only -DLDRD_TEST ${TESTFILE} >/dev/null 2>/dev/null; then echo yes; else echo no; fi
 * FEAT_THUMB2!=if ${COMPILE.c} -fsyntax-only -DTHUMB2_TEST ${TESTFILE} >/dev/null 2>/dev/null; then echo yes; else echo no; fi
 */

#if defined (__ARM_ARCH_8A__)  || defined (__ARM_ARCH_7__) || \
    defined (__ARM_ARCH_7A__)  || defined (__ARM_ARCH_7R__) || \
    defined (__ARM_ARCH_7M__)  || defined (__ARM_ARCH_7EM__) || \
    defined (__ARM_ARCH_6T2__)
#define HAVE_THUMB2
#endif

#if defined (HAVE_THUMB2) || defined (__ARM_ARCH_6__) || \
    defined (__ARM_ARCH_6J__)  || defined (__ARM_ARCH_6K__) || \
    defined (__ARM_ARCH_6Z__)  || defined (__ARM_ARCH_6ZK__) || \
    defined (__ARM_ARCH_6ZM__)
#define HAVE_LDREX
#endif

#if defined (HAVE_LDREX) || defined(__ARM_ARCH_5TE__) || \
    defined (__ARM_ARCH_5TEJ__)
#define HAVE_LDRD
#endif

#if defined(THUMB2_TEST) && !defined(HAVE_THUMB2)
#error no thumb2
#endif

#if defined(LDREX_TEST) && !defined(HAVE_LDREX)
#error no ldrex
#endif

#if defined(LDRD_TEST) && !defined(HAVE_LDRD)
#error no ldrd
#endif

#if defined(EABI_TEST) && !defined(__ARM_EABI__)
#error not eabi
#endif
