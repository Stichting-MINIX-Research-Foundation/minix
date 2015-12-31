/*	$NetBSD: funcs.c,v 1.10 2015/01/02 21:15:32 christos Exp $	*/

/*
 * Copyright (c) Christos Zoulas 2003.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "file.h"

#ifndef	lint
#if 0
FILE_RCSID("@(#)$File: funcs.c,v 1.79 2014/12/16 20:52:49 christos Exp $")
#else
__RCSID("$NetBSD: funcs.c,v 1.10 2015/01/02 21:15:32 christos Exp $");
#endif
#endif	/* lint */

#include "magic.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(HAVE_WCHAR_H)
#include <wchar.h>
#endif
#if defined(HAVE_WCTYPE_H)
#include <wctype.h>
#endif
#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif

#ifndef SIZE_MAX
#define SIZE_MAX	((size_t)~0)
#endif

/*
 * Like printf, only we append to a buffer.
 */
protected int
file_vprintf(struct magic_set *ms, const char *fmt, va_list ap)
{
	int len;
	char *buf, *newstr;

	if (ms->event_flags & EVENT_HAD_ERR)
		return 0;
	len = vasprintf(&buf, fmt, ap);
	if (len < 0)
		goto out;

	if (ms->o.buf != NULL) {
		len = asprintf(&newstr, "%s%s", ms->o.buf, buf);
		free(buf);
		if (len < 0)
			goto out;
		free(ms->o.buf);
		buf = newstr;
	}
	ms->o.buf = buf;
	return 0;
out:
	file_error(ms, errno, "vasprintf failed");
	return -1;
}

protected int
file_printf(struct magic_set *ms, const char *fmt, ...)
{
	int rv;
	va_list ap;

	va_start(ap, fmt);
	rv = file_vprintf(ms, fmt, ap);
	va_end(ap);
	return rv;
}

/*
 * error - print best error message possible
 */
/*VARARGS*/
__attribute__((__format__(__printf__, 3, 0)))
private void
file_error_core(struct magic_set *ms, int error, const char *f, va_list va,
    size_t lineno)
{
	/* Only the first error is ok */
	if (ms->event_flags & EVENT_HAD_ERR)
		return;
	if (lineno != 0) {
		free(ms->o.buf);
		ms->o.buf = NULL;
		file_printf(ms, "line %" SIZE_T_FORMAT "u: ", lineno);
	}
	file_vprintf(ms, f, va);
	if (error > 0)
		file_printf(ms, " (%s)", strerror(error));
	ms->event_flags |= EVENT_HAD_ERR;
	ms->error = error;
}

/*VARARGS*/
protected void
file_error(struct magic_set *ms, int error, const char *f, ...)
{
	va_list va;
	va_start(va, f);
	file_error_core(ms, error, f, va, 0);
	va_end(va);
}

/*
 * Print an error with magic line number.
 */
/*VARARGS*/
protected void
file_magerror(struct magic_set *ms, const char *f, ...)
{
	va_list va;
	va_start(va, f);
	file_error_core(ms, 0, f, va, ms->line);
	va_end(va);
}

protected void
file_oomem(struct magic_set *ms, size_t len)
{
	file_error(ms, errno, "cannot allocate %" SIZE_T_FORMAT "u bytes",
	    len);
}

protected void
file_badseek(struct magic_set *ms)
{
	file_error(ms, errno, "error seeking");
}

protected void
file_badread(struct magic_set *ms)
{
	file_error(ms, errno, "error reading");
}

#ifndef COMPILE_ONLY
/*ARGSUSED*/
protected int
file_buffer(struct magic_set *ms, int fd, const char *inname __attribute__ ((__unused__)),
    const void *buf, size_t nb)
{
	int m = 0, rv = 0, looks_text = 0;
	int mime = ms->flags & MAGIC_MIME;
	const unsigned char *ubuf = CAST(const unsigned char *, buf);
	unichar *u8buf = NULL;
	size_t ulen;
	const char *code = NULL;
	const char *code_mime = "binary";
	const char *type = "application/octet-stream";
	const char *def = "data";
	const char *ftype = NULL;

	if (nb == 0) {
		def = "empty";
		type = "application/x-empty";
		goto simple;
	} else if (nb == 1) {
		def = "very short file (no magic)";
		goto simple;
	}

	if ((ms->flags & MAGIC_NO_CHECK_ENCODING) == 0) {
		looks_text = file_encoding(ms, ubuf, nb, &u8buf, &ulen,
		    &code, &code_mime, &ftype);
	}

#ifdef __EMX__
	if ((ms->flags & MAGIC_NO_CHECK_APPTYPE) == 0 && inname) {
		switch (file_os2_apptype(ms, inname, buf, nb)) {
		case -1:
			return -1;
		case 0:
			break;
		default:
			return 1;
		}
	}
#endif
#if HAVE_FORK
	/* try compression stuff */
	if ((ms->flags & MAGIC_NO_CHECK_COMPRESS) == 0)
		if ((m = file_zmagic(ms, fd, inname, ubuf, nb)) != 0) {
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void)fprintf(stderr, "zmagic %d\n", m);
			goto done_encoding;
		}
#endif
	/* Check if we have a tar file */
	if ((ms->flags & MAGIC_NO_CHECK_TAR) == 0)
		if ((m = file_is_tar(ms, ubuf, nb)) != 0) {
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void)fprintf(stderr, "tar %d\n", m);
			goto done;
		}

	/* Check if we have a CDF file */
	if ((ms->flags & MAGIC_NO_CHECK_CDF) == 0)
		if ((m = file_trycdf(ms, fd, ubuf, nb)) != 0) {
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void)fprintf(stderr, "cdf %d\n", m);
			goto done;
		}

	/* try soft magic tests */
	if ((ms->flags & MAGIC_NO_CHECK_SOFT) == 0)
		if ((m = file_softmagic(ms, ubuf, nb, 0, NULL, BINTEST,
		    looks_text)) != 0) {
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void)fprintf(stderr, "softmagic %d\n", m);
#ifdef BUILTIN_ELF
			if ((ms->flags & MAGIC_NO_CHECK_ELF) == 0 && m == 1 &&
			    nb > 5 && fd != -1) {
				/*
				 * We matched something in the file, so this
				 * *might* be an ELF file, and the file is at
				 * least 5 bytes long, so if it's an ELF file
				 * it has at least one byte past the ELF magic
				 * number - try extracting information from the
				 * ELF headers that cannot easily * be
				 * extracted with rules in the magic file.
				 */
				if ((m = file_tryelf(ms, fd, ubuf, nb)) != 0)
					if ((ms->flags & MAGIC_DEBUG) != 0)
						(void)fprintf(stderr,
						    "elf %d\n", m);
			}
#endif
			goto done;
		}

	/* try text properties */
	if ((ms->flags & MAGIC_NO_CHECK_TEXT) == 0) {

		if ((m = file_ascmagic(ms, ubuf, nb, looks_text)) != 0) {
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void)fprintf(stderr, "ascmagic %d\n", m);
			goto done;
		}
	}

simple:
	/* give up */
	m = 1;
	if ((!mime || (mime & MAGIC_MIME_TYPE)) &&
	    file_printf(ms, "%s", mime ? type : def) == -1) {
	    rv = -1;
	}
 done:
	if ((ms->flags & MAGIC_MIME_ENCODING) != 0) {
		if (ms->flags & MAGIC_MIME_TYPE)
			if (file_printf(ms, "; charset=") == -1)
				rv = -1;
		if (file_printf(ms, "%s", code_mime) == -1)
			rv = -1;
	}
#if HAVE_FORK
 done_encoding:
#endif
	free(u8buf);
	if (rv)
		return rv;

	return m;
}
#endif

protected int
file_reset(struct magic_set *ms)
{
	if (ms->mlist[0] == NULL) {
		file_error(ms, 0, "no magic files loaded");
		return -1;
	}
	if (ms->o.buf) {
		free(ms->o.buf);
		ms->o.buf = NULL;
	}
	if (ms->o.pbuf) {
		free(ms->o.pbuf);
		ms->o.pbuf = NULL;
	}
	ms->event_flags &= ~EVENT_HAD_ERR;
	ms->error = -1;
	return 0;
}

#define OCTALIFY(n, o)	\
	/*LINTED*/ \
	(void)(*(n)++ = '\\', \
	*(n)++ = (((uint32_t)*(o) >> 6) & 3) + '0', \
	*(n)++ = (((uint32_t)*(o) >> 3) & 7) + '0', \
	*(n)++ = (((uint32_t)*(o) >> 0) & 7) + '0', \
	(o)++)

protected const char *
file_getbuffer(struct magic_set *ms)
{
	char *pbuf, *op, *np;
	size_t psize, len;

	if (ms->event_flags & EVENT_HAD_ERR)
		return NULL;

	if (ms->flags & MAGIC_RAW)
		return ms->o.buf;

	if (ms->o.buf == NULL)
		return NULL;

	/* * 4 is for octal representation, + 1 is for NUL */
	len = strlen(ms->o.buf);
	if (len > (SIZE_MAX - 1) / 4) {
		file_oomem(ms, len);
		return NULL;
	}
	psize = len * 4 + 1;
	if ((pbuf = CAST(char *, realloc(ms->o.pbuf, psize))) == NULL) {
		file_oomem(ms, psize);
		return NULL;
	}
	ms->o.pbuf = pbuf;

#if defined(HAVE_WCHAR_H) && defined(HAVE_MBRTOWC) && defined(HAVE_WCWIDTH)
	{
		mbstate_t state;
		wchar_t nextchar;
		int mb_conv = 1;
		size_t bytesconsumed;
		char *eop;
		(void)memset(&state, 0, sizeof(mbstate_t));

		np = ms->o.pbuf;
		op = ms->o.buf;
		eop = op + len;

		while (op < eop) {
			bytesconsumed = mbrtowc(&nextchar, op,
			    (size_t)(eop - op), &state);
			if (bytesconsumed == (size_t)(-1) ||
			    bytesconsumed == (size_t)(-2)) {
				mb_conv = 0;
				break;
			}

			if (iswprint(nextchar)) {
				(void)memcpy(np, op, bytesconsumed);
				op += bytesconsumed;
				np += bytesconsumed;
			} else {
				while (bytesconsumed-- > 0)
					OCTALIFY(np, op);
			}
		}
		*np = '\0';

		/* Parsing succeeded as a multi-byte sequence */
		if (mb_conv != 0)
			return ms->o.pbuf;
	}
#endif

	for (np = ms->o.pbuf, op = ms->o.buf; *op;) {
		if (isprint((unsigned char)*op)) {
			*np++ = *op++;
		} else {
			OCTALIFY(np, op);
		}
	}
	*np = '\0';
	return ms->o.pbuf;
}

protected int
file_check_mem(struct magic_set *ms, unsigned int level)
{
	size_t len;

	if (level >= ms->c.len) {
		len = (ms->c.len += 20) * sizeof(*ms->c.li);
		ms->c.li = CAST(struct level_info *, (ms->c.li == NULL) ?
		    malloc(len) :
		    realloc(ms->c.li, len));
		if (ms->c.li == NULL) {
			file_oomem(ms, len);
			return -1;
		}
	}
	ms->c.li[level].got_match = 0;
#ifdef ENABLE_CONDITIONALS
	ms->c.li[level].last_match = 0;
	ms->c.li[level].last_cond = COND_NONE;
#endif /* ENABLE_CONDITIONALS */
	return 0;
}

protected size_t
file_printedlen(const struct magic_set *ms)
{
	return ms->o.buf == NULL ? 0 : strlen(ms->o.buf);
}

protected int
file_replace(struct magic_set *ms, const char *pat, const char *rep)
{
	file_regex_t rx;
	int rc, rv = -1;

	rc = file_regcomp(&rx, pat, REG_EXTENDED);
	if (rc) {
		file_regerror(&rx, rc, ms);
	} else {
		regmatch_t rm;
		int nm = 0;
		while (file_regexec(&rx, ms->o.buf, 1, &rm, 0) == 0) {
			ms->o.buf[rm.rm_so] = '\0';
			if (file_printf(ms, "%s%s", rep,
			    rm.rm_eo != 0 ? ms->o.buf + rm.rm_eo : "") == -1)
				goto out;
			nm++;
		}
		rv = nm;
	}
out:
	file_regfree(&rx);
	return rv;
}

protected int
file_regcomp(file_regex_t *rx, const char *pat, int flags)
{
#ifdef USE_C_LOCALE
	rx->c_lc_ctype = newlocale(LC_CTYPE_MASK, "C", 0);
	assert(rx->c_lc_ctype != NULL);
	rx->old_lc_ctype = uselocale(rx->c_lc_ctype);
	assert(rx->old_lc_ctype != NULL);
#endif
	rx->pat = pat;

	return rx->rc = regcomp(&rx->rx, pat, flags);
}

protected int
file_regexec(file_regex_t *rx, const char *str, size_t nmatch,
    regmatch_t* pmatch, int eflags)
{
	assert(rx->rc == 0);
	return regexec(&rx->rx, str, nmatch, pmatch, eflags);
}

protected void
file_regfree(file_regex_t *rx)
{
	if (rx->rc == 0)
		regfree(&rx->rx);
#ifdef USE_C_LOCALE
	(void)uselocale(rx->old_lc_ctype);
	freelocale(rx->c_lc_ctype);
#endif
}

protected void
file_regerror(file_regex_t *rx, int rc, struct magic_set *ms)
{
	char errmsg[512];

	(void)regerror(rc, &rx->rx, errmsg, sizeof(errmsg));
	file_magerror(ms, "regex error %d for `%s', (%s)", rc, rx->pat,
	    errmsg);
}

protected file_pushbuf_t *
file_push_buffer(struct magic_set *ms)
{
	file_pushbuf_t *pb;

	if (ms->event_flags & EVENT_HAD_ERR)
		return NULL;

	if ((pb = (CAST(file_pushbuf_t *, malloc(sizeof(*pb))))) == NULL)
		return NULL;

	pb->buf = ms->o.buf;
	pb->offset = ms->offset;

	ms->o.buf = NULL;
	ms->offset = 0;

	return pb;
}

protected char *
file_pop_buffer(struct magic_set *ms, file_pushbuf_t *pb)
{
	char *rbuf;

	if (ms->event_flags & EVENT_HAD_ERR) {
		free(pb->buf);
		free(pb);
		return NULL;
	}

	rbuf = ms->o.buf;

	ms->o.buf = pb->buf;
	ms->offset = pb->offset;

	free(pb);
	return rbuf;
}

/*
 * convert string to ascii printable format.
 */
protected char *
file_printable(char *buf, size_t bufsiz, const char *str)
{
	char *ptr, *eptr;
	const unsigned char *s = (const unsigned char *)str;

	for (ptr = buf, eptr = ptr + bufsiz - 1; ptr < eptr && *s; s++) {
		if (isprint(*s)) {
			*ptr++ = *s;
			continue;
		}
		if (ptr >= eptr - 3)
			break;
		*ptr++ = '\\';
		*ptr++ = ((CAST(unsigned int, *s) >> 6) & 7) + '0';
		*ptr++ = ((CAST(unsigned int, *s) >> 3) & 7) + '0';
		*ptr++ = ((CAST(unsigned int, *s) >> 0) & 7) + '0';
	}
	*ptr = '\0';
	return buf;
}
