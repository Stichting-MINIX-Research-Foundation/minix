/*	$NetBSD: dlfcn_w32.c,v 1.1.1.2 2014/04/24 12:45:52 pettai Exp $	*/

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

#include <config.h>
#include <windows.h>
#include <dlfcn.h>
#include <strsafe.h>

#define ERR_STR_LEN 256

static volatile LONG dlfcn_tls = TLS_OUT_OF_INDEXES;

static DWORD get_tl_error_slot(void)
{
    if (dlfcn_tls == TLS_OUT_OF_INDEXES) {
        DWORD slot = TlsAlloc();
        DWORD old_slot;

        if (slot == TLS_OUT_OF_INDEXES)
            return dlfcn_tls;

        if ((old_slot = InterlockedCompareExchange(&dlfcn_tls, slot,
                                                   TLS_OUT_OF_INDEXES)) !=
            TLS_OUT_OF_INDEXES) {

            /* Lost a race */
            TlsFree(slot);
            return old_slot;
        } else {
            return slot;
        }
    }

    return dlfcn_tls;
}

static void set_error(const char * e)
{
    char * s;
    char * old_s;
    size_t len;

    DWORD slot = get_tl_error_slot();

    if (slot == TLS_OUT_OF_INDEXES)
        return;

    len = strlen(e) * sizeof(char) + sizeof(char);
    s = LocalAlloc(LMEM_FIXED, len);
    if (s == NULL)
        return;

    old_s = (char *) TlsGetValue(slot);
    TlsSetValue(slot, (LPVOID) s);

    if (old_s != NULL)
        LocalFree(old_s);
}

static void set_error_from_last(void) {
    DWORD slot = get_tl_error_slot();
    char * s = NULL;
    char * old_s;

    if (slot == TLS_OUT_OF_INDEXES)
        return;

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		  0, GetLastError(), 0,
		  (LPTSTR) &s, 0,
		  NULL);
    if (s == NULL)
        return;

    old_s = (char *) TlsGetValue(slot);
    TlsSetValue(slot, (LPVOID) s);

    if (old_s != NULL)
        LocalFree(old_s);
}

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
dlclose(void * vhm)
{
    BOOL brv;

    brv = FreeLibrary((HMODULE) vhm);
    if (!brv) {
	set_error_from_last();
    }
    return !brv;
}

ROKEN_LIB_FUNCTION char  * ROKEN_LIB_CALL
dlerror(void)
{
    DWORD slot = get_tl_error_slot();

    if (slot == TLS_OUT_OF_INDEXES)
        return NULL;

    return (char *) TlsGetValue(slot);
}

ROKEN_LIB_FUNCTION void  * ROKEN_LIB_CALL
dlopen(const char *fn, int flags)
{
    HMODULE hm;
    UINT    old_error_mode;

    /* We don't support dlopen(0, ...) on Windows.*/
    if ( fn == NULL ) {
	set_error("Not implemented");
	return NULL;
    }

    old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

    hm = LoadLibrary(fn);

    if (hm == NULL) {
	set_error_from_last();
    }

    SetErrorMode(old_error_mode);

    return (void *) hm;
}

ROKEN_LIB_FUNCTION DLSYM_RET_TYPE ROKEN_LIB_CALL
dlsym(void * vhm, const char * func_name)
{
    HMODULE hm = (HMODULE) vhm;

    return (DLSYM_RET_TYPE)(ULONG_PTR)GetProcAddress(hm, func_name);
}

