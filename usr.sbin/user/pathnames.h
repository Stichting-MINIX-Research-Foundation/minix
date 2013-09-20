/* $NetBSD: pathnames.h,v 1.1 2011/12/01 00:34:05 dholland Exp $ */

/*
 * Copyright (c) 1999 Alistair G. Crooks.  All rights reserved.
 * Copyright (c) 2005 Liam J. Foy.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <paths.h>

/* Full paths of programs used here */
#define _PATH_CHMOD		"/bin/chmod"
#define _PATH_CHOWN		"/usr/sbin/chown"
#define	_PATH_LOGINCONF		"/etc/login.conf"
#define _PATH_MKDIR		"/bin/mkdir"
#define _PATH_MV		"/bin/mv"
/* note that there's a _PATH_NOLOGIN in <paths.h> that's for /etc/nologin */
#define _PATH_SBIN_NOLOGIN	"/sbin/nologin"
#define _PATH_PAX		"/bin/pax"
#define _PATH_RM		"/bin/rm"

