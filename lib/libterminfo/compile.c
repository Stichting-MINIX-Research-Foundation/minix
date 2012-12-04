/* $NetBSD: compile.c,v 1.8 2012/06/03 23:19:10 joerg Exp $ */

/*
 * Copyright (c) 2009, 2010, 2011 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: compile.c,v 1.8 2012/06/03 23:19:10 joerg Exp $");

#if !HAVE_NBTOOL_CONFIG_H || HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <term_private.h>
#include <term.h>

static void __printflike(2, 3)
dowarn(int flags, const char *fmt, ...)
{
	va_list va;

	errno = EINVAL;
	if (flags & TIC_WARNING) {
		va_start(va, fmt);
		vwarnx(fmt, va);
		va_end(va);
	}
}

char *
_ti_grow_tbuf(TBUF *tbuf, size_t len)
{
	char *buf;
	size_t l;

	_DIAGASSERT(tbuf != NULL);

	l = tbuf->bufpos + len;
	if (l > tbuf->buflen) {
		if (tbuf->buflen == 0)
			buf = malloc(l);
		else
			buf = realloc(tbuf->buf, l);
		if (buf == NULL)
			return NULL;
		tbuf->buf = buf;
		tbuf->buflen = l;
	}
	return tbuf->buf;
}

char *
_ti_find_cap(TBUF *tbuf, char type, short ind)
{
	size_t n;
	short num;
	char *cap;

	_DIAGASSERT(tbuf != NULL);

	cap = tbuf->buf;
	for (n = tbuf->entries; n > 0; n--) {
		num = le16dec(cap);
		cap += sizeof(uint16_t);
		if (num == ind)
			return cap;
		switch (type) {
		case 'f':
			cap++;
			break;
		case 'n':
			cap += sizeof(uint16_t);
			break;
		case 's':
			num = le16dec(cap);
			cap += sizeof(uint16_t);
			cap += num;
			break;
		}
	}
	
	errno = ESRCH;
	return NULL;
}

char *
_ti_find_extra(TBUF *tbuf, const char *code)
{
	size_t n;
	short num;
	char *cap;

	_DIAGASSERT(tbuf != NULL);
	_DIAGASSERT(code != NULL);

	cap = tbuf->buf;
	for (n = tbuf->entries; n > 0; n--) {
		num = le16dec(cap);
		cap += sizeof(uint16_t);
		if (strcmp(cap, code) == 0)
			return cap + num;
		cap += num;
		switch (*cap++) {
		case 'f':
			cap++;
			break;
		case 'n':
			cap += sizeof(uint16_t);
			break;
		case 's':
			num = le16dec(cap);
			cap += sizeof(uint16_t);
			cap += num;
			break;
		}
	}
	
	errno = ESRCH;
	return NULL;
}

size_t
_ti_store_extra(TIC *tic, int wrn, char *id, char type, char flag, short num,
    char *str, size_t strl, int flags)
{
	size_t l;

	_DIAGASSERT(tic != NULL);

	if (strcmp(id, "use") != 0) {
		if (_ti_find_extra(&tic->extras, id) != NULL)
			return 0;
		if (!(flags & TIC_EXTRA)) {
			if (wrn != 0)
				dowarn(flags, "%s: %s: unknown capability",
				    tic->name, id);
			return 0;
		}
	}
	
	l = strlen(id) + 1;
	if (l > UINT16_T_MAX) {
		dowarn(flags, "%s: %s: cap name is too long", tic->name, id);
		return 0;
	}
	
	if (!_ti_grow_tbuf(&tic->extras,
		l + strl + (sizeof(uint16_t) * 2) + 1))
		return 0;
	le16enc(tic->extras.buf + tic->extras.bufpos, l);
	tic->extras.bufpos += sizeof(uint16_t);
     	memcpy(tic->extras.buf + tic->extras.bufpos, id, l);
	tic->extras.bufpos += l;
	tic->extras.buf[tic->extras.bufpos++] = type;
	switch (type) {
	case 'f':
		tic->extras.buf[tic->extras.bufpos++] = flag;
		break;
	case 'n':
		le16enc(tic->extras.buf + tic->extras.bufpos, num);
		tic->extras.bufpos += sizeof(uint16_t);
		break;
	case 's':
		le16enc(tic->extras.buf + tic->extras.bufpos, strl);
		tic->extras.bufpos += sizeof(uint16_t);
		memcpy(tic->extras.buf + tic->extras.bufpos, str, strl);
		tic->extras.bufpos += strl;
		break;
	}
	tic->extras.entries++;
	return 1;
}

ssize_t
_ti_flatten(uint8_t **buf, const TIC *tic)
{
	size_t buflen, len, alen, dlen;
	uint8_t *cap;

	_DIAGASSERT(buf != NULL);
	_DIAGASSERT(tic != NULL);

	len = strlen(tic->name) + 1;
	if (tic->alias == NULL)
		alen = 0;
	else
		alen = strlen(tic->alias) + 1;
	if (tic->desc == NULL)
		dlen = 0;
	else
		dlen = strlen(tic->desc) + 1;
	buflen = sizeof(char) +
	    sizeof(uint16_t) + len +
	    sizeof(uint16_t) + alen +
	    sizeof(uint16_t) + dlen +
	    (sizeof(uint16_t) * 2) + tic->flags.bufpos +
	    (sizeof(uint16_t) * 2) + tic->nums.bufpos +
	    (sizeof(uint16_t) * 2) + tic->strs.bufpos +
	    (sizeof(uint16_t) * 2) + tic->extras.bufpos;
	*buf = malloc(buflen);
	if (*buf == NULL)
		return -1;
	
	cap = *buf;
	*cap++ = 1;
	le16enc(cap, len);
	cap += sizeof(uint16_t);
	memcpy(cap, tic->name, len);
	cap += len;
	
	le16enc(cap, alen);
	cap += sizeof(uint16_t);
	if (tic->alias != NULL) {
		memcpy(cap, tic->alias, alen);
		cap += alen;
	}
	le16enc(cap, dlen);
	cap += sizeof(uint16_t);
	if (tic->desc != NULL) {
		memcpy(cap, tic->desc, dlen);
		cap += dlen;
	}

	if (tic->flags.entries == 0) {
		le16enc(cap, 0);
		cap += sizeof(uint16_t);
	} else {
		le16enc(cap, (tic->flags.bufpos + sizeof(uint16_t)));
		cap += sizeof(uint16_t);
		le16enc(cap, tic->flags.entries);
		cap += sizeof(uint16_t);
		memcpy(cap, tic->flags.buf, tic->flags.bufpos);
		cap += tic->flags.bufpos;
	}
	
	if (tic->nums.entries == 0) {
		le16enc(cap, 0);
		cap += sizeof(uint16_t);
	} else {
		le16enc(cap, (tic->nums.bufpos + sizeof(uint16_t)));
		cap += sizeof(uint16_t);
		le16enc(cap, tic->nums.entries);
		cap += sizeof(uint16_t);
		memcpy(cap, tic->nums.buf, tic->nums.bufpos);
		cap += tic->nums.bufpos;
	}
	
	if (tic->strs.entries == 0) {
		le16enc(cap, 0);
		cap += sizeof(uint16_t);
	} else {
		le16enc(cap, (tic->strs.bufpos + sizeof(uint16_t)));
		cap += sizeof(uint16_t);
		le16enc(cap, tic->strs.entries);
		cap += sizeof(uint16_t);
		memcpy(cap, tic->strs.buf, tic->strs.bufpos);
		cap += tic->strs.bufpos;
	}
	
	if (tic->extras.entries == 0) {
		le16enc(cap, 0);
		cap += sizeof(uint16_t);
	} else {
		le16enc(cap, (tic->extras.bufpos + sizeof(uint16_t)));
		cap += sizeof(uint16_t);
		le16enc(cap, tic->extras.entries);
		cap += sizeof(uint16_t);
		memcpy(cap, tic->extras.buf, tic->extras.bufpos);
		cap += tic->extras.bufpos;
	}

	return cap - *buf;
}

static int
encode_string(const char *term, const char *cap, TBUF *tbuf, const char *str,
    int flags)
{
	int slash, i, num;
	char ch, *p, *s, last;
	
	if (_ti_grow_tbuf(tbuf, strlen(str) + 1) == NULL)
		return -1;
	p = s = tbuf->buf + tbuf->bufpos;
	slash = 0;
	last = '\0';
	/* Convert escape codes */
	while ((ch = *str++) != '\0') {
		if (slash == 0 && ch == '\\') {
			slash = 1;
			continue;
		}
		if (slash == 0) {
			if (last != '%' && ch == '^') {
				ch = *str++;
				if (((unsigned char)ch) >= 128)
					dowarn(flags,
					    "%s: %s: illegal ^ character",
					    term, cap);
				if (ch == '\0')
					break;
				if (ch == '?')
					ch = '\177';
				else if ((ch &= 037) == 0)
					ch = (char)128;
			}
			*p++ = ch;
			last = ch;
			continue;
		}
		slash = 0;
		if (ch >= '0' && ch <= '7') {
			num = ch - '0';
			for (i = 0; i < 2; i++) {
				if (*str < '0' || *str > '7') {
					if (isdigit((unsigned char)*str))
						dowarn(flags,
						    "%s: %s: non octal"
						    " digit", term, cap);
					else
						break;
				}
				num = num * 8 + *str++ - '0';
			}
			if (num == 0)
				num = 0200;
			*p++ = (char)num;
			continue;
		}
		switch (ch) {
		case 'a':
			*p++ = '\a';
			break;
		case 'b':
			*p++ = '\b';
			break;
		case 'e': /* FALLTHROUGH */
		case 'E':
			*p++ = '\033';
			break;
		case 'f':
			*p++ = '\014';
			break;
		case 'l': /* FALLTHROUGH */
		case 'n':
			*p++ = '\n';
			break;
		case 'r':
			*p++ = '\r';
			break;
		case 's':
			*p++ = ' ';
			break;
		case 't':
			*p++ = '\t';
			break;
		default:
					
			/* We should warn here */
		case '^':
		case ',':
		case ':':
		case '|':
			*p++ = ch;
			break;
		}
		last = ch;
	}
	*p++ = '\0';
	tbuf->bufpos += p - s;
	return 0;
}

char *
_ti_get_token(char **cap, char sep)
{
	char esc, *token;

	while (isspace((unsigned char)**cap))
		(*cap)++;
	if (**cap == '\0')
		return NULL;

	/* We can't use stresep(3) as ^ we need two escape chars */
	esc = '\0';
	for (token = *cap;
	     **cap != '\0' && (esc != '\0' || **cap != sep);
	     (*cap)++)
	{
		if (esc == '\0') {
			if (**cap == '\\' || **cap == '^')
				esc = **cap;
		} else {
			/* termcap /E/ is valid */
			if (sep == ':' && esc == '\\' && **cap == 'E')
				esc = 'x';
			else
				esc = '\0';
		}
	}

	if (**cap != '\0')
		*(*cap)++ = '\0';

	return token;
}

TIC *
_ti_compile(char *cap, int flags)
{
	char *token, *p, *e, *name, *desc, *alias;
	signed char flag;
	long cnum;
	short num;
	ssize_t ind;
	size_t len;
	TBUF buf;
	TIC *tic;

	_DIAGASSERT(cap != NULL);	

	name = _ti_get_token(&cap, ',');
	if (name == NULL) {
		dowarn(flags, "no seperator found: %s", cap);
		return NULL;
	}
	desc = strrchr(name, '|');
	if (desc != NULL)
		*desc++ = '\0';
	alias = strchr(name, '|');
	if (alias != NULL)
		*alias++ = '\0';

	tic = calloc(sizeof(*tic), 1);
	if (tic == NULL)
		return NULL;

	buf.buf = NULL;
	buf.buflen = 0;

	tic->name = strdup(name);
	if (tic->name == NULL)
		goto error;
	if (alias != NULL && flags & TIC_ALIAS) {
		tic->alias = strdup(alias);
		if (tic->alias == NULL)
			goto error;
	}
	if (desc != NULL && flags & TIC_DESCRIPTION) {
		tic->desc = strdup(desc);
		if (tic->desc == NULL)
			goto error;
	}

	for (token = _ti_get_token(&cap, ',');
	     token != NULL && *token != '\0';
	     token = _ti_get_token(&cap, ','))
	{
		/* Skip commented caps */
		if (!(flags & TIC_COMMENT) && token[0] == '.')
			continue;

		/* Obsolete entries */
		if (token[0] == 'O' && token[1] == 'T') {
			if (!(flags & TIC_EXTRA))
				continue;
			token += 2;
		}

		/* str cap */
		p = strchr(token, '=');
		if (p != NULL) {
			*p++ = '\0';
			/* Don't use the string if we already have it */
			ind = _ti_strindex(token);
			if (ind != -1 &&
			    _ti_find_cap(&tic->strs, 's', ind) != NULL)
				continue;

			/* Encode the string to our scratch buffer */
			buf.bufpos = 0;
			if (encode_string(tic->name, token,
				&buf, p, flags) == -1)
				goto error;
			if (buf.bufpos > UINT16_T_MAX) {
				dowarn(flags, "%s: %s: string is too long",
				    tic->name, token);
				continue;
			}
			if (!VALID_STRING(buf.buf)) {
				dowarn(flags, "%s: %s: invalid string",
				    tic->name, token);
				continue;
			}

			if (ind == -1)
				_ti_store_extra(tic, 1, token, 's', -1, -2,
				    buf.buf, buf.bufpos, flags);
			else {
				if (!_ti_grow_tbuf(&tic->strs,
					(sizeof(uint16_t) * 2) + buf.bufpos))
					goto error;
				le16enc(tic->strs.buf + tic->strs.bufpos, ind);
				tic->strs.bufpos += sizeof(uint16_t);
				le16enc(tic->strs.buf + tic->strs.bufpos,
				    buf.bufpos);
				tic->strs.bufpos += sizeof(uint16_t);
				memcpy(tic->strs.buf + tic->strs.bufpos,
				    buf.buf, buf.bufpos);
				tic->strs.bufpos += buf.bufpos;
				tic->strs.entries++;
			}
			continue;
		}

		/* num cap */
		p = strchr(token, '#');
		if (p != NULL) {
			*p++ = '\0';
			/* Don't use the number if we already have it */
			ind = _ti_numindex(token);
			if (ind != -1 &&
			    _ti_find_cap(&tic->nums, 'n', ind) != NULL)
				continue;

			cnum = strtol(p, &e, 0);
			if (*e != '\0') {
				dowarn(flags, "%s: %s: not a number",
				    tic->name, token);
				continue;
			}
			if (!VALID_NUMERIC(cnum)) {
				dowarn(flags, "%s: %s: number out of range",
				    tic->name, token);
				continue;
			}
			num = (short)cnum;
			if (ind == -1)
				_ti_store_extra(tic, 1, token, 'n', -1,
				    num, NULL, 0, flags);
			else {
				if (_ti_grow_tbuf(&tic->nums,
					sizeof(uint16_t) * 2) == NULL)
					goto error;
				le16enc(tic->nums.buf + tic->nums.bufpos, ind);
				tic->nums.bufpos += sizeof(uint16_t);
				le16enc(tic->nums.buf + tic->nums.bufpos, num);
				tic->nums.bufpos += sizeof(uint16_t);
				tic->nums.entries++;
			}
			continue;
		}

		flag = 1;
		len = strlen(token) - 1;
		if (token[len] == '@') {
			flag = CANCELLED_BOOLEAN;
			token[len] = '\0';
		}
		ind = _ti_flagindex(token);
		if (ind == -1 && flag == CANCELLED_BOOLEAN) {
			if ((ind = _ti_numindex(token)) != -1) {
				if (_ti_find_cap(&tic->nums, 'n', ind) != NULL)
					continue;
				if (_ti_grow_tbuf(&tic->nums,
					sizeof(uint16_t) * 2) == NULL)
					goto error;
				le16enc(tic->nums.buf + tic->nums.bufpos, ind);
				tic->nums.bufpos += sizeof(uint16_t);
				le16enc(tic->nums.buf + tic->nums.bufpos,
					(uint16_t)CANCELLED_NUMERIC);
				tic->nums.bufpos += sizeof(uint16_t);
				tic->nums.entries++;
				continue;
			} else if ((ind = _ti_strindex(token)) != -1) {
				if (_ti_find_cap(&tic->strs, 's', ind) != NULL)
					continue;
				if (_ti_grow_tbuf(&tic->strs,
					(sizeof(uint16_t) * 2) + 1) == NULL)
					goto error;
				le16enc(tic->strs.buf + tic->strs.bufpos, ind);
				tic->strs.bufpos += sizeof(uint16_t);
				le16enc(tic->strs.buf + tic->strs.bufpos, 0);
				tic->strs.bufpos += sizeof(uint16_t);
				tic->strs.entries++;
				continue;
			}
		}
		if (ind == -1)
			_ti_store_extra(tic, 1, token, 'f', flag, 0, NULL, 0,
			    flags);
		else if (_ti_find_cap(&tic->flags, 'f', ind) == NULL) {
			if (_ti_grow_tbuf(&tic->flags, sizeof(uint16_t) + 1)
			    == NULL)
				goto error;
			le16enc(tic->flags.buf + tic->flags.bufpos, ind);
			tic->flags.bufpos += sizeof(uint16_t);
			tic->flags.buf[tic->flags.bufpos++] = flag;
			tic->flags.entries++;
		}
	}

	free(buf.buf);
	return tic;

error:
	free(buf.buf);
	_ti_freetic(tic);
	return NULL;
}

void
_ti_freetic(TIC *tic)
{

	if (tic != NULL) {
		free(tic->name);
		free(tic->alias);
		free(tic->desc);
		free(tic->extras.buf);
		free(tic->flags.buf);
		free(tic->nums.buf);
		free(tic->strs.buf);
		free(tic);
	}
}
