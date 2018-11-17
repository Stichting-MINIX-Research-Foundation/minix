/*	$NetBSD: rand.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

#ifdef HAVE_WIN32_RAND_S
static int hasRand_s = 1;
#endif

void ROKEN_LIB_FUNCTION
rk_random_init(void)
{
#if defined(HAVE_ARC4RANDOM)
    arc4random_stir();
#elif defined(HAVE_SRANDOMDEV)
    srandomdev();
#elif defined(HAVE_RANDOM)
    srandom(time(NULL));
#else
# ifdef HAVE_WIN32_RAND_S
    OSVERSIONINFO osInfo;

    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
    hasRand_s =
	(GetVersionEx(&osInfo)
	  && ((osInfo.dwMajorVersion > 5) ||
	       (osInfo.dwMajorVersion == 5) && (osInfo.dwMinorVersion >= 1)));
# endif
    srand (time(NULL));
#endif
}

#ifdef HAVE_WIN32_RAND_S
unsigned int ROKEN_LIB_FUNCTION
rk_random(void)
{
    if (hasRand_s) {
	unsigned int n;
	int code;

	code = rand_s(&n);
	if (code == 0)
	    return n;
    }

    return rand();
}
#endif
