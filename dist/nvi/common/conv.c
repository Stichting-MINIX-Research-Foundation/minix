/*	$NetBSD: conv.c,v 1.6 2009/01/18 03:45:50 lukem Exp $ */

/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: conv.c,v 1.27 2001/08/18 21:41:41 skimo Exp (Berkeley) Date: 2001/08/18 21:41:41";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

#ifdef USE_ICONV
#include <langinfo.h>
#include <iconv.h>

#define LANGCODESET	nl_langinfo(CODESET)
#else
typedef int	iconv_t;

#define LANGCODESET	""
#endif

#include <locale.h>

#ifdef USE_WIDECHAR
static int 
raw2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, size_t *tolen,
	const CHAR_T **dst)
{
    int i;
    CHAR_T **tostr = (CHAR_T **)(void *)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RETW(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = (u_char) str[i];

    *dst = cw->bp1;

    return 0;
}

#define CONV_BUFFER_SIZE    512
/* fill the buffer with codeset encoding of string pointed to by str
 * left has the number of bytes left in str and is adjusted
 * len contains the number of bytes put in the buffer
 */
#ifdef USE_ICONV
#define CONVERT(str, left, src, len)				    	\
    do {								\
	size_t outleft;							\
	char *bp = buffer;						\
	outleft = CONV_BUFFER_SIZE;					\
	errno = 0;							\
	if (iconv(id, (const char **)&str, &left, &bp, &outleft) == (size_t)-1 \
		/* && errno != E2BIG */)				\
	    goto err;							\
	if ((len = CONV_BUFFER_SIZE - outleft) == 0) {			\
	    error = -left;						\
	    goto err;							\
	}				    				\
	src = buffer;							\
    } while (0)
#else
#define CONVERT(str, left, src, len)
#endif

static int 
default_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, 
		size_t *tolen, const CHAR_T **dst, const char *enc)
{
    int j;
    size_t i = 0;
    CHAR_T **tostr = (CHAR_T **)(void *)&cw->bp1;
    size_t  *blen = &cw->blen1;
    mbstate_t mbs;
    size_t   n;
    ssize_t  nlen = len;
    const char *src = (const char *)str;
    iconv_t	id = (iconv_t)-1;
    char	buffer[CONV_BUFFER_SIZE];
    size_t	left = len;
    int		error = 1;

    MEMSET(&mbs, 0, 1);
    BINC_RETW(NULL, *tostr, *blen, nlen);

#ifdef USE_ICONV
    if (strcmp(nl_langinfo(CODESET), enc)) {
	id = iconv_open(nl_langinfo(CODESET), enc);
	if (id == (iconv_t)-1)
	    goto err;
	CONVERT(str, left, src, len);
    }
#endif

    for (i = 0, j = 0; j < len; ) {
	n = mbrtowc((*tostr)+i, src+j, len-j, &mbs);
	/* NULL character converted */
	if (n == (size_t)-2) error = -(len-j);
	if (n == (size_t)-1 || n == (size_t)-2) goto err;
	if (n == 0) n = 1;
	j += n;
	if (++i >= *blen) {
	    nlen += 256;
	    BINC_RETW(NULL, *tostr, *blen, nlen);
	}
	if (id != (iconv_t)-1 && j == len && left) {
	    CONVERT(str, left, src, len);
	    j = 0;
	}
    }
    *tolen = i;

    if (id != (iconv_t)-1)
	iconv_close(id);

    *dst = cw->bp1;

    return 0;
err:
    *tolen = i;
    if (id != (iconv_t)-1)
	iconv_close(id);
    *dst = cw->bp1;

    return error;
}

static int 
fe_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, 
	    size_t *tolen, const CHAR_T **dst)
{
    return default_char2int(sp, str, len, cw, tolen, dst, O_STR(sp, O_FILEENCODING));
}

static int 
ie_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, 
	    size_t *tolen, const CHAR_T **dst)
{
    return default_char2int(sp, str, len, cw, tolen, dst, O_STR(sp, O_INPUTENCODING));
}

static int 
cs_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, 
	    size_t *tolen, const CHAR_T **dst)
{
    return default_char2int(sp, str, len, cw, tolen, dst, LANGCODESET);
}

static int 
CHAR_T_int2char(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
	size_t *tolen, const char **dst)
{
    *tolen = len * sizeof(CHAR_T);
    *dst = (const char *)(const void *)str;

    return 0;
}

static int 
CHAR_T_char2int(SCR *sp, const char * str, ssize_t len, CONVWIN *cw, 
	size_t *tolen, const CHAR_T **dst)
{
    *tolen = len / sizeof(CHAR_T);
    *dst = (const CHAR_T*) str;

    return 0;
}

static int 
int2raw(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, size_t *tolen,
	const char **dst)
{
    int i;
    char **tostr = (char **)(void *)&cw->bp1;
    size_t  *blen = &cw->blen1;

    BINC_RETC(NULL, *tostr, *blen, len);

    *tolen = len;
    for (i = 0; i < len; ++i)
	(*tostr)[i] = str[i];

    *dst = cw->bp1;

    return 0;
}

static int 
default_int2char(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
		size_t *tolen, const char **pdst, const char *enc)
{
    size_t i, j;
    int offset = 0;
    char **tostr = (char **)(void *)&cw->bp1;
    size_t  *blen = &cw->blen1;
    mbstate_t mbs;
    size_t n;
    ssize_t  nlen = len + MB_CUR_MAX;
    char *dst;
    size_t buflen;
    char	buffer[CONV_BUFFER_SIZE];
    iconv_t	id = (iconv_t)-1;

/* convert first len bytes of buffer and append it to cw->bp
 * len is adjusted => 0
 * offset contains the offset in cw->bp and is adjusted
 * cw->bp is grown as required
 */
#ifdef USE_ICONV
#define CONVERT2(len, cw, offset)					\
    do {								\
	const char *bp = buffer;					\
	while (len != 0) {						\
	    size_t outleft = cw->blen1 - offset;			\
	    char *obp = (char *)cw->bp1 + offset;		    	\
	    if (cw->blen1 < offset + MB_CUR_MAX) {		    	\
		nlen += 256;						\
		BINC_RETC(NULL, cw->bp1, cw->blen1, nlen);		\
	    }						    		\
	    errno = 0;						    	\
	    if (iconv(id, &bp, &len, &obp, &outleft) == (size_t)-1 &&	\
		    errno != E2BIG)					\
		goto err;						\
	    offset = cw->blen1 - outleft;			        \
	}							        \
    } while (0)
#else
#define CONVERT2(len, cw, offset)
#endif


    MEMSET(&mbs, 0, 1);
    BINC_RETC(NULL, *tostr, *blen, nlen);
    dst = *tostr; buflen = *blen;

#ifdef USE_ICONV
    if (strcmp(nl_langinfo(CODESET), enc)) {
	id = iconv_open(enc, nl_langinfo(CODESET));
	if (id == (iconv_t)-1)
	    goto err;
	dst = buffer; buflen = CONV_BUFFER_SIZE;
    }
#endif

    for (i = 0, j = 0; i < (size_t)len; ++i) {
	n = wcrtomb(dst+j, str[i], &mbs);
	if (n == (size_t)-1) goto err;
	j += n;
	if (buflen < j + MB_CUR_MAX) {
	    if (id != (iconv_t)-1) {
		CONVERT2(j, cw, offset);
	    } else {
		nlen += 256;
		BINC_RETC(NULL, *tostr, *blen, nlen);
		dst = *tostr; buflen = *blen;
	    }
	}
    }

    n = wcrtomb(dst+j, L'\0', &mbs);
    j += n - 1;				/* don't count NUL at the end */
    *tolen = j;

    if (id != (iconv_t)-1) {
	CONVERT2(j, cw, offset);
	*tolen = offset;
    }

    *pdst = cw->bp1;

    return 0;
err:
    *tolen = j;

    *pdst = cw->bp1;

    return 1;
}

static int 
fe_int2char(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
	    size_t *tolen, const char **dst)
{
    return default_int2char(sp, str, len, cw, tolen, dst, O_STR(sp, O_FILEENCODING));
}

static int 
cs_int2char(SCR *sp, const CHAR_T * str, ssize_t len, CONVWIN *cw, 
	    size_t *tolen, const char **dst)
{
    return default_int2char(sp, str, len, cw, tolen, dst, LANGCODESET);
}

#endif


void
conv_init (SCR *orig, SCR *sp)
{
    if (orig != NULL)
	MEMCPY(&sp->conv, &orig->conv, 1);
    else {
	setlocale(LC_ALL, "");
#ifdef USE_WIDECHAR
	sp->conv.sys2int = cs_char2int;
	sp->conv.int2sys = cs_int2char;
	sp->conv.file2int = fe_char2int;
	sp->conv.int2file = fe_int2char;
	sp->conv.input2int = ie_char2int;
#endif
#ifdef USE_ICONV
	o_set(sp, O_FILEENCODING, OS_STRDUP, nl_langinfo(CODESET), 0);
	o_set(sp, O_INPUTENCODING, OS_STRDUP, nl_langinfo(CODESET), 0);
#endif
    }
}

int
conv_enc (SCR *sp, int option, const char *enc)
{
#if defined(USE_WIDECHAR) && defined(USE_ICONV)
    iconv_t id;
    char2wchar_t    *c2w;
    wchar2char_t    *w2c;

    switch (option) {
    case O_FILEENCODING:
	c2w = &sp->conv.file2int;
	w2c = &sp->conv.int2file;
	break;
    case O_INPUTENCODING:
	c2w = &sp->conv.input2int;
	w2c = NULL;
	break;
    default:
	c2w = NULL;
	w2c = NULL;
	break;
    }

    if (!*enc) {
	if (c2w) *c2w = raw2int;
	if (w2c) *w2c = int2raw;
	return 0;
    }

    if (!strcmp(enc, "WCHAR_T")) {
	if (c2w) *c2w = CHAR_T_char2int;
	if (w2c) *w2c = CHAR_T_int2char;
	return 0;
    }

    id = iconv_open(enc, nl_langinfo(CODESET));
    if (id == (iconv_t)-1)
	goto err;
    iconv_close(id);
    id = iconv_open(nl_langinfo(CODESET), enc);
    if (id == (iconv_t)-1)
	goto err;
    iconv_close(id);

    switch (option) {
    case O_FILEENCODING:
	*c2w = fe_char2int;
	*w2c = fe_int2char;
	break;
    case O_INPUTENCODING:
	*c2w = ie_char2int;
	break;
    }

    F_CLR(sp, SC_CONV_ERROR);
    F_SET(sp, SC_SCR_REFORMAT);

    return 0;
err:
    switch (option) {
    case O_FILEENCODING:
	msgq(sp, M_ERR,
	    "321|File encoding conversion not supported");
	break;
    case O_INPUTENCODING:
	msgq(sp, M_ERR,
	    "322|Input encoding conversion not supported");
	break;
    }
#endif
    return 1;
}

