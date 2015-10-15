/*	$NetBSD: sockstartup_w32.c,v 1.1.1.2 2014/04/24 12:45:52 pettai Exp $	*/

/***********************************************************************
 * Copyright (c) 2009, Secure Endpoints Inc.
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

#include<config.h>

#include <krb5/roken.h>

#ifndef _WIN32
#error Only implemented for Windows
#endif

volatile LONG _startup_count = 0;

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
rk_WSAStartup(void)
{
    WSADATA wsad;

    if (!WSAStartup( MAKEWORD(2, 2), &wsad )) {
	if (wsad.wVersion != MAKEWORD(2, 2)) {
	    /* huh? We can't use 2.2? */
	    WSACleanup();
	    return -1;
	}

	InterlockedIncrement(&_startup_count);
	return 0;
    }

    return -1;
}


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
rk_WSACleanup(void)
{
    LONG l;

    if ((l = InterlockedDecrement(&_startup_count)) < 0) {
	l = InterlockedIncrement(&_startup_count) - 1;
    }

    if (l >= 0) {
	return WSACleanup();
    }
    return -1;
}
