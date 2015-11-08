/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef __NetBSD__
#include <util.h>
#else
#define emalloc malloc
#define estrdup strdup
#define ecalloc calloc
#define erealloc realloc
#endif

#if STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#define setbuffer(f, b, s)	setvbuf((f), (b), (b) ? _IOFBF : _IONBF, (s))
#define memzero(a, b)		memset((a), 0, (b))
#else /* !STDC_HEADERS */
#ifndef HAVE_STRCHR
#define strchr(a, b)		index((a), (b))
#define strrchr(a, b)		rindex((a), (b))
#endif /* HAVE_STRCHR */
#ifdef HAVE_MEMCPY
#define memzero(a, b)		memset((a), 0, (b))
#else
#define memcpy(a, b, c)		bcopy((b), (a), (c))
#define memzero(a, b)		bzero((a), (b))
#define memcmp(a, b, c)		bcmp((a), (b), (c))
#endif /* HAVE_MEMCPY */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#else
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#endif
char *getenv();
caddr_t malloc();
#endif /* STDC_HEADERS */

/* If snprintf or vsnprintf aren't available, we substitute our own.
   But we have to include stdarg in order to be able to define them.
*/
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#ifndef HAVE_SNPRINTF
int ap_snprintf(char *buf, size_t len, const char *format,...);
#define snprintf ap_snprintf
#endif
#ifndef HAVE_VSNPRINTF
int ap_vsnprintf(char *buf, size_t len, const char *format,va_list ap);
#define vsnprintf ap_vsnprintf
#endif
#endif

#if !HAVE_PID_T
typedef long pid_t;
#endif
#if !HAVE_TIME_T
typedef long time_t;
#endif
#if !HAVE_UID_T
typedef long uid_t;
#endif

#ifndef INT_MAX
#define INT_MAX (0x7fffffff)
#endif

#ifndef UINT_MAX
#define UINT_MAX (0xffffffffU)
#endif

/* we must have both sighold and sigrelse to use them */
#if defined(HAVE_SIGHOLD) && !defined(HAVE_SIGRELSE)
#undef HAVE_SIGHOLD
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_OK		0	/* successful termination */
#define EX_USAGE	64	/* command line usage error */
#define EX_DATAERR	65	/* data format error */
#define EX_NOINPUT	66	/* cannot open input */
#define EX_NOUSER	67	/* addressee unknown */
#define EX_NOHOST	68	/* host name unknown */
#define EX_UNAVAILABLE	69	/* service unavailable */
#define EX_SOFTWARE	70	/* internal software error */
#define EX_OSERR	71	/* system error (e.g., can't fork) */
#define EX_OSFILE	72	/* critical OS file missing */
#define EX_CANTCREAT	73	/* can't create (user) output file */
#define EX_IOERR	74	/* input/output error */
#define EX_TEMPFAIL	75	/* temp failure; user is invited to retry */
#define EX_PROTOCOL	76	/* remote error in protocol */
#define EX_NOPERM	77	/* permission denied */
#define EX_CONFIG	78	/* configuration error */
#endif
