/*	$NetBSD: iconv.c,v 1.14 2014/11/15 18:49:04 nakayama Exp $	*/

/* Public domain */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iconv.c,v 1.14 2014/11/15 18:49:04 nakayama Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <netsmb/iconv.h>

/* stubs for iconv functions */
int iconv_open_stub(const char *, const char *, void **);
int iconv_close_stub(void *);
int iconv_conv_stub(void *, const char **, size_t *, char **, size_t *);
__weak_alias(iconv_open, iconv_open_stub);
__weak_alias(iconv_close, iconv_close_stub);
__weak_alias(iconv_conv, iconv_conv_stub);

int
iconv_open_stub(const char *to, const char *from,
    void **handle)
{
	return 0;
}

int
iconv_close_stub(void *handle)
{
	return 0;
}

int
iconv_conv_stub(void *handle, const char **inbuf,
    size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	if (inbuf == NULL)
		return(0); /* initial shift state */

	if (*inbytesleft > *outbytesleft)
		return(E2BIG);

	(void)memcpy(*outbuf, *inbuf, *inbytesleft);

	*outbytesleft -= *inbytesleft;

	*inbuf += *inbytesleft;
	*outbuf += *inbytesleft;

	*inbytesleft = 0;

	return 0;
}

char *
iconv_convstr(void *handle, char *dst, const char *src, size_t l)
{
	char *p = dst;
	size_t inlen, outlen;
	int error;

	if (handle == NULL) {
		strlcpy(dst, src, l);
		return dst;
	}
	inlen = strlen(src);
	outlen = l - 1;
	error = iconv_conv(handle, NULL, NULL, &p, &outlen);
	if (error)
		return NULL;
	error = iconv_conv(handle, &src, &inlen, &p, &outlen);
	if (error)
		return NULL;
	*p = 0;
	return dst;
}

void *
iconv_convmem(void *handle, void *dst, const void *src, int size)
{
	const char *s = src;
	char *d = dst;
	size_t inlen, outlen;
	int error;

	if (size == 0)
		return dst;
	if (handle == NULL) {
		memcpy(dst, src, size);
		return dst;
	}
	inlen = outlen = size;
	error = iconv_conv(handle, NULL, NULL, &d, &outlen);
	if (error)
		return NULL;
	error = iconv_conv(handle, &s, &inlen, &d, &outlen);
	if (error)
		return NULL;
	return dst;
}

int
iconv_lookupcp(const char **cpp, const char *s)
{
	for (; *cpp; cpp++)
		if (strcmp(*cpp, s) == 0)
			return 0;
	return ENOENT;
}
