/*	$NetBSD: ct.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#include <krb5/roken.h>

/**
 * Constant time compare to memory regions. The reason for making it
 * constant time is to make sure that timeing information leak from
 * where in the function the diffrence is.
 *
 * ct_memcmp() can't be used to order memory regions like memcmp(),
 * for example, use ct_memcmp() with qsort().
 *
 * We use volatile to avoid optimizations where the compiler and/or
 * linker turn this ct_memcmp() into a plain memcmp().  The pointers
 * themselves are also marked volatile (not just the memory pointed at)
 * because in some GCC versions there is a bug which can be worked
 * around by doing this.
 *
 * @param p1 memory region 1 to compare
 * @param p2 memory region 2 to compare
 * @param len length of memory
 *
 * @return 0 when the memory regions are equal, non zero if not
 *
 * @ingroup roken
 */

int
ct_memcmp(const volatile void * volatile p1,
          const volatile void * volatile p2,
          size_t len)
{
    /*
     * There's no need for s1 and s2 to be volatile; only p1 and p2 have
     * to be in order to work around GCC bugs.
     *
     * However, s1 and s2 do have to point to volatile, as we don't know
     * if the object was originally defined as volatile, and if it was
     * then we'd get undefined behavior here if s1/s2 were declared to
     * point to non-volatile memory.
     */
    const volatile unsigned char *s1 = p1;
    const volatile unsigned char *s2 = p2;
    size_t i;
    int r = 0;

    for (i = 0; i < len; i++)
	r |= (s1[i] ^ s2[i]);
    return !!r;
}
