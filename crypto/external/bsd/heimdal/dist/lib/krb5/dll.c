/*	$NetBSD: dll.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

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

#include<windows.h>

extern void heim_w32_service_thread_detach(void *);

HINSTANCE _krb5_hInstance = NULL;

#if NTDDI_VERSION >= NTDDI_VISTA
extern BOOL WINAPI
_hc_w32crypto_DllMain(HINSTANCE hinstDLL,
		      DWORD fdwReason,
		      LPVOID lpvReserved);
#endif

BOOL WINAPI DllMain(HINSTANCE hinstDLL,
		    DWORD fdwReason,
		    LPVOID lpvReserved)
{
#if NTDDI_VERSION >= NTDDI_VISTA
    BOOL ret;

    ret = _hc_w32crypto_DllMain(hinstDLL, fdwReason, lpvReserved);
    if (!ret)
	return ret;
#endif

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:

	_krb5_hInstance = hinstDLL;
	return TRUE;

    case DLL_PROCESS_DETACH:
	return FALSE;

    case DLL_THREAD_ATTACH:
	return FALSE;

    case DLL_THREAD_DETACH:
        heim_w32_service_thread_detach(NULL);
	return FALSE;
    }

    return FALSE;
}

