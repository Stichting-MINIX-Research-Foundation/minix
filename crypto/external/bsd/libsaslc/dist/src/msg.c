/* $NetBSD: msg.c,v 1.2 2011/02/12 23:21:32 christos Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: msg.c,v 1.2 2011/02/12 23:21:32 christos Exp $");

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "msg.h"

/**
 * XXX: global debug flag.  This is unique as it is set as early as
 * possible by checking the environment (looking for SASLC_ENV_DEBUG)
 * and the context, mechanism, and session dictionaries (looking for
 * SASLC_PROP_DEBUG) as soon as they become available.  Hence, the
 * lookups are scattered in 4 places.  It's ugly.
 *
 * It's also global so it isn't tied to a session, but this makes it
 * easly to use debugging messages as you don't need a context
 * pointer.
 */
bool saslc_debug = false;

/**
 * @brief conditionally log a message via syslogd
 * @param flag log the message or not
 * @param priority syslogd priority to log with
 * @param fmt message format string
 * @param ... format parameters
 */
void
saslc__msg_syslog(bool flag, int priority, const char *fmt, ...)
{
	va_list ap;
	char *tmp;

	if (!flag)
		return;

	va_start(ap, fmt);
	if (asprintf(&tmp, "libsaslc: %s", fmt) != -1) {
		vsyslog(priority, tmp, ap);
		free(tmp);
	}
	va_end(ap);
}
