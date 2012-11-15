/*	$NetBSD: getservbyname_r.c,v 1.9 2012/03/13 21:13:41 christos Exp $	*/

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
static char sccsid[] = "@(#)getservbyname.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getservbyname_r.c,v 1.9 2012/03/13 21:13:41 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <assert.h>
#include <cdbr.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include "servent.h"

#ifdef __weak_alias
__weak_alias(getservbyname_r,_getservbyname_r)
#endif

static struct servent *
_servent_getbyname(struct servent_data *sd, struct servent *sp,
    const char *name, const char *proto)
{

	if ((sd->flags & (_SV_CDB | _SV_PLAINFILE)) == 0)
		return NULL;

	if (sd->flags & _SV_CDB) {
		uint8_t buf[255 * 2 + 2];
		size_t namelen, protolen;
		const uint8_t *data, *data_end;
		const void *data_ptr;
		size_t datalen;

		namelen = strlen(name);
		if (namelen == 0 || namelen > 255)
			return NULL;
		if (proto != NULL) {
			protolen = strlen(proto);
			if (protolen == 0 || protolen > 255)
				return NULL;
		} else
			protolen = 0;
		if (namelen + protolen > 255)
			return NULL;

		buf[0] = (uint8_t)namelen;
		buf[1] = (uint8_t)protolen;
		memcpy(buf + 2, name, namelen);
		memcpy(buf + 2 + namelen, proto, protolen);

		if (cdbr_find(sd->cdb, buf, 2 + namelen + protolen,
		    &data_ptr, &datalen))
			return NULL;

		if (datalen < namelen + protolen + 6)
			return NULL;

		data = data_ptr;
		data_end = data + datalen;
		if (protolen) {
			if (data[2] != protolen)
				return NULL;
			if (memcmp(data + 3, proto, protolen + 1))
				return NULL;
		}
		data += 3 + data[2] + 1;
		if (data > data_end)
			return NULL;
		while (data != data_end) {
			if (*data == '\0')
				return NULL;
			if (data + data[0] + 2 > data_end)
				return NULL;
			if (data[0] == namelen &&
			    memcmp(data + 1, name, namelen + 1) == 0)
				return _servent_parsedb(sd, sp, data_ptr,
				    datalen);
			data += data[0] + 2;
		}
		return NULL;
	} else {
		while (_servent_getline(sd) != -1) {
			char **cp;
			if (_servent_parseline(sd, sp) == NULL)
				continue;

			if (strcmp(name, sp->s_name) == 0)
				goto gotname;

			for (cp = sp->s_aliases; *cp; cp++)
				if (strcmp(name, *cp) == 0)
					goto gotname;
			continue;
gotname:
			if (proto == NULL || strcmp(sp->s_proto, proto) == 0)
				return sp;
		}
		return NULL;
	}
}

struct servent *
getservbyname_r(const char *name, const char *proto, struct servent *sp,
    struct servent_data *sd)
{
	_DIAGASSERT(name != NULL);
	/* proto may be NULL */

	setservent_r(sd->flags & _SV_STAYOPEN, sd);
	sp = _servent_getbyname(sd, sp, name, proto);
	if (!(sd->flags & _SV_STAYOPEN))
		_servent_close(sd);
	return sp;
}
