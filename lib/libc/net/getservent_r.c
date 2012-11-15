/*	$NetBSD: getservent_r.c,v 1.11 2011/10/15 23:00:02 christos Exp $	*/

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
static char sccsid[] = "@(#)getservent.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getservent_r.c,v 1.11 2011/10/15 23:00:02 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <cdbr.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "servent.h"

#ifdef __weak_alias
__weak_alias(endservent_r,_endservent_r)
__weak_alias(getservent_r,_getservent_r)
__weak_alias(setservent_r,_setservent_r)
#endif

int
_servent_open(struct servent_data *sd)
{
	if (sd->flags & (_SV_CDB | _SV_PLAINFILE)) {
		sd->flags |= _SV_FIRST;
		return 0;
	}

	free(sd->line);
	sd->line = NULL;
	free(sd->cdb_buf);
	sd->cdb_buf = NULL;
	sd->cdb_buf_len = 0;
	free(sd->aliases);
	sd->aliases = NULL;
	sd->maxaliases = 0;
	sd->flags |= _SV_FIRST;

	sd->cdb = cdbr_open(_PATH_SERVICES_CDB, CDBR_DEFAULT);
	if (sd->cdb != NULL) {
		sd->flags |= _SV_CDB;
		return 0;
	}
		
	sd->plainfile = fopen(_PATH_SERVICES, "re");
	if (sd->plainfile != NULL) {
		sd->flags |= _SV_PLAINFILE;
		return 0;
	}
	return -1;
}

void
_servent_close(struct servent_data *sd)
{
	if (sd->flags & _SV_CDB) {
		cdbr_close(sd->cdb);
		sd->cdb = NULL;
		sd->flags &= ~_SV_CDB;
	}

	if (sd->flags & _SV_PLAINFILE) {
		(void)fclose(sd->plainfile);
		sd->plainfile = NULL;
		sd->flags &= ~_SV_PLAINFILE;
	}
	sd->flags &= ~_SV_STAYOPEN;
}


int
_servent_getline(struct servent_data *sd)
{

	if (sd->flags & _SV_CDB)
		return -1;

	if ((sd->flags & _SV_PLAINFILE) == 0)
		return -1;

	free(sd->line);
	sd->line = NULL;

	if (sd->flags & _SV_FIRST) {
		(void)rewind((FILE *)sd->plainfile);
		sd->flags &= ~_SV_FIRST;
	}
	sd->line = fparseln(sd->plainfile, NULL, NULL, NULL,
	    FPARSELN_UNESCALL);
	return sd->line == NULL ? -1 : 0;
}

struct servent *
_servent_parseline(struct servent_data *sd, struct servent *sp)
{
	size_t i = 0;
	int oerrno;
	char *p, *cp, **q;

	if (sd->line == NULL)
		return NULL;

	sp->s_name = p = sd->line;
	p = strpbrk(p, " \t");
	if (p == NULL)
		return NULL;
	*p++ = '\0';
	while (*p == ' ' || *p == '\t')
		p++;
	cp = strpbrk(p, ",/");
	if (cp == NULL)
		return NULL;
	*cp++ = '\0';
	sp->s_port = htons((u_short)atoi(p));
	sp->s_proto = cp;
	if (sd->aliases == NULL) {
		sd->maxaliases = 10;
		sd->aliases = calloc(sd->maxaliases, sizeof(*sd->aliases));
		if (sd->aliases == NULL) {
			oerrno = errno;
			endservent_r(sd);
			errno = oerrno;
			return NULL;
		}
	}
	sp->s_aliases = sd->aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (i == sd->maxaliases - 2) {
			sd->maxaliases *= 2;
			q = realloc(sd->aliases, sd->maxaliases * sizeof(*q));
			if (q == NULL) {
				oerrno = errno;
				endservent_r(sd);
				errno = oerrno;
				return NULL;
			}
			sp->s_aliases = sd->aliases = q;
		}
		sp->s_aliases[i++] = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	sp->s_aliases[i] = NULL;
	return sp;
}

void
setservent_r(int f, struct servent_data *sd)
{
	(void)_servent_open(sd);
	sd->flags |= f ? _SV_STAYOPEN : 0;
}

void
endservent_r(struct servent_data *sd)
{
	_servent_close(sd);
	free(sd->aliases);
	sd->aliases = NULL;
	sd->maxaliases = 0;
	free(sd->line);
	sd->line = NULL;
	free(sd->cdb_buf);
	sd->cdb_buf = NULL;
	sd->cdb_buf_len = 0;
}

struct servent *
getservent_r(struct servent *sp, struct servent_data *sd)
{

	if ((sd->flags & (_SV_CDB | _SV_PLAINFILE)) == 0 &&
	    _servent_open(sd) == -1)
		return NULL;

	if (sd->flags & _SV_CDB) {
		const void *data;
		size_t len;

		if (sd->flags & _SV_FIRST) {
			sd->cdb_index = 0;
			sd->flags &= ~_SV_FIRST;
		}

		if (cdbr_get(sd->cdb, sd->cdb_index, &data, &len))
			return NULL;
		++sd->cdb_index;
		return _servent_parsedb(sd, sp, data, len);
	}
	if (sd->flags & _SV_PLAINFILE) {
		for (;;) {
			if (_servent_getline(sd) == -1)
				return NULL;
			if (_servent_parseline(sd, sp) == NULL)
				continue;
			return sp;
		}
	}
	return NULL;
}

struct servent *
_servent_parsedb(struct servent_data *sd, struct servent *sp,
    const uint8_t *data, size_t len)
{
	char **q;
	size_t i;
	int oerrno;

	if ((sd->flags & _SV_STAYOPEN) == 0) {
		if (len > sd->cdb_buf_len) {
			void *tmp = realloc(sd->cdb_buf, len);
			if (tmp == NULL)
				goto fail;
			sd->cdb_buf = tmp;
			sd->cdb_buf_len = len;
		}
		memcpy(sd->cdb_buf, data, len);
		data = sd->cdb_buf;
	}

	if (len < 2)
		goto fail;
	sp->s_port = htobe16(be16dec(data));
	data += 2;
	len -= 2;

	if (len == 0 || len < (size_t)data[0] + 2)
		goto fail;
	sp->s_proto = __UNCONST(data + 1);

	if (sp->s_proto[data[0]] != '\0')
		goto fail;

	len -= 2 + data[0];
	data += 2 + data[0];

	if (len == 0)
		goto fail;
	if (len < (size_t)data[0] + 2)
		goto fail;

	sp->s_name = __UNCONST(data + 1);
	len -= 2 + data[0];
	data += 2 + data[0];

	if (sd->aliases == NULL) {
		sd->maxaliases = 10;
		sd->aliases = malloc(sd->maxaliases * sizeof(char *));
		if (sd->aliases == NULL)
			goto fail;
	}
	sp->s_aliases = sd->aliases;
	i = 0;
	while (len) {
		if (len < (size_t)data[0] + 2)
			goto fail;
		if (i == sd->maxaliases - 2) {
			sd->maxaliases *= 2;
			q = realloc(sd->aliases, sd->maxaliases * sizeof(*q));
			if (q == NULL)
				goto fail;
			sp->s_aliases = sd->aliases = q;
		}
		sp->s_aliases[i++] = __UNCONST(data + 1);
		len -= 2 + data[0];
		data += 2 + data[0];
	}
	sp->s_aliases[i] = NULL;
	return sp;

fail:
	oerrno = errno;
	endservent_r(sd);
	errno = oerrno;
	return NULL;
}

