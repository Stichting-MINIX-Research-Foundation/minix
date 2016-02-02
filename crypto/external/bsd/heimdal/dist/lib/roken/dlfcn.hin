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

#ifndef __dlfcn_h__
#define __dlfcn_h__

#ifndef ROKEN_LIB_FUNCTION
#ifdef _WIN32
#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL     __cdecl
#else
#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL
#endif
#endif

#ifdef _WIN32
typedef int (__stdcall *DLSYM_RET_TYPE)();
#else
#define DLSYM_RET_TYPE void *
#endif

#ifdef __cplusplus
extern "C" 
#endif

/* Implementation based on
   http://www.opengroup.org/onlinepubs/009695399/basedefs/dlfcn.h.html */

#define RTLD_LAZY   (1<<0)

#define RTLD_NOW    (1<<1)

#define RTLD_GLOBAL (1<<2)

#define RTLD_LOCAL  (1<<3)

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
dlclose(void *);

ROKEN_LIB_FUNCTION char  * ROKEN_LIB_CALL
dlerror(void);

ROKEN_LIB_FUNCTION void  * ROKEN_LIB_CALL
dlopen(const char *, int);

ROKEN_LIB_FUNCTION DLSYM_RET_TYPE ROKEN_LIB_CALL
dlsym(void *, const char *);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif	/* __dlfcn_h__ */
