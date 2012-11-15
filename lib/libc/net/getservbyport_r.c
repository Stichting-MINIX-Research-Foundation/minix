/*	$NetBSD: getservbyport_r.c,v 1.9 2012/03/13 21:13:41 christos Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getservbyport.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getservbyport_r.c,v 1.9 2012/03/13 21:13:41 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <cdbr.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

#include "servent.h"

#ifdef __weak_alias
__weak_alias(getservbyport_r,_getservbyport_r)
#endif

static struct servent *
_servent_getbyport(struct servent_data *sd, struct servent *sp, int port,
    const char *proto)
{

	if ((sd->flags & (_SV_CDB | _SV_PLAINFILE)) == 0)
		return NULL;

	if (sd->flags & _SV_CDB) {
		uint8_t buf[255 + 4];
		size_t protolen;
		const uint8_t *data;
		const void *data_ptr;
		size_t datalen;

		port = be16toh(port);

		if (proto != NULL) {
			protolen = strlen(proto);
			if (protolen == 0 || protolen > 255)
				return NULL;
		} else
			protolen = 0;
		if (port < 0 || port > 65536)
			return NULL;

		buf[0] = 0;
		buf[1] = (uint8_t)protolen;
		be16enc(buf + 2, port);
		memcpy(buf + 4, proto, protolen);

		if (cdbr_find(sd->cdb, buf, 4 + protolen,
		    &data_ptr, &datalen))
			return NULL;

		if (datalen < protolen + 4)
			return NULL;

		data = data_ptr;
		if (be16dec(data) != port)
			return NULL;
		if (protolen) {
			if (data[2] != protolen)
				return NULL;
			if (memcmp(data + 3, proto, protolen + 1))
				return NULL;
		}
		return _servent_parsedb(sd, sp, data, datalen);
	} else {
		while (_servent_getline(sd) != -1) {
			if (_servent_parseline(sd, sp) == NULL)
				continue;
			if (sp->s_port != port)
				continue;
			if (proto == NULL || strcmp(sp->s_proto, proto) == 0)
				return sp;
		}
		return NULL;
	}
}

struct servent *
getservbyport_r(int port, const char *proto, struct servent *sp,
    struct servent_data *sd)
{
	setservent_r(sd->flags & _SV_STAYOPEN, sd);
	sp = _servent_getbyport(sd, sp, port, proto);
	if (!(sd->flags & _SV_STAYOPEN))
		_servent_close(sd);
	return sp;
}
