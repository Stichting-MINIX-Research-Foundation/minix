/*	$NetBSD: win32_alloc.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/***********************************************************************
 * Copyright (c) 2012, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************/
#include <config.h>
#include <krb5/roken.h>
#undef calloc
#undef malloc
#undef free
#undef strdup
#undef wcsdup

/*
 * Windows executables and dlls suffer when memory is
 * allocated with one allocator and deallocated with
 * another because each allocator is backed by a separate
 * heap.  Reduce the exposure by ensuring that all
 * binaries that are built using roken will build against
 * same allocator.
 */

ROKEN_LIB_FUNCTION void * ROKEN_LIB_CALL
rk_calloc(size_t elements, size_t size)
{
    return calloc( elements, size);
}

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
rk_free(void * ptr)
{
    free( ptr);
}

ROKEN_LIB_FUNCTION void * ROKEN_LIB_CALL
rk_malloc(size_t size)
{
    return malloc( size);
}

ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL
rk_strdup(const char *str)
{
    return strdup( str);
}

ROKEN_LIB_FUNCTION unsigned short * ROKEN_LIB_CALL
rk_wcsdup(const unsigned short *str)
{
    return wcsdup( str);
}
