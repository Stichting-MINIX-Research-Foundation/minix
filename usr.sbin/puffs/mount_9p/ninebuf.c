/*      $NetBSD: ninebuf.c,v 1.8 2012/11/04 22:38:19 christos Exp $	*/

/*
 * Copyright (c) 2006, 2007  Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ninebuf.c,v 1.8 2012/11/04 22:38:19 christos Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <util.h>
#include <unistd.h>

#include "ninepuffs.h"

#define CHECK(v) if (!(v)) abort()

uint8_t
p9pbuf_get_type(struct puffs_framebuf *pb)
{
	uint8_t val;

	puffs_framebuf_getdata_atoff(pb, 4, &val, 1);
	return val;
}

uint16_t
p9pbuf_get_tag(struct puffs_framebuf *pb)
{
	uint16_t val;

	puffs_framebuf_getdata_atoff(pb, 5, &val, 2);
	return le16toh(val);
}

static uint32_t
p9pbuf_get_len(struct puffs_framebuf *pb)
{
	uint32_t val;

	puffs_framebuf_getdata_atoff(pb, 0, &val, 4);
	return le32toh(val);
}

#define CUROFF(pb) (puffs_framebuf_telloff(pb))
int
p9pbuf_read(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	int fd, int *done)
{
	void *win;
	ssize_t n;
	size_t howmuch, winlen;
	int lenstate;

 the_next_level:
	if ((lenstate = (CUROFF(pb) < 4)))
		howmuch = 4 - CUROFF(pb);
	else
		howmuch = p9pbuf_get_len(pb) - CUROFF(pb);

	if (puffs_framebuf_reserve_space(pb, howmuch) == -1)
		return errno;

	while (howmuch) {
		winlen = howmuch;
		if (puffs_framebuf_getwindow(pb, CUROFF(pb), &win, &winlen)==-1)
			return errno;
		n = read(fd, win, winlen);
		switch (n) {
		case 0:
			return ECONNRESET;
		case -1:
			if (errno == EAGAIN)
				return 0;
			return errno;
		default:
			howmuch -= n;
			puffs_framebuf_seekset(pb, CUROFF(pb) + n);
			break;
		}
	}

	if (!lenstate) {
		puffs_framebuf_seekset(pb, 7);
		*done = 1;
		return 0;
	} else
		goto the_next_level;
}

int
p9pbuf_write(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	int fd, int *done)
{
	void *win;
	ssize_t n;
	size_t winlen, howmuch;

	if (CUROFF(pb) == 0) {
		uint32_t len;

		len = htole32(puffs_framebuf_tellsize(pb));
		puffs_framebuf_putdata_atoff(pb, 0, &len, 4);
	}

	howmuch = puffs_framebuf_tellsize(pb) - CUROFF(pb);
	while (howmuch) {
		winlen = howmuch;
		if (puffs_framebuf_getwindow(pb, CUROFF(pb), &win, &winlen)==-1)
			return errno;
		n = send(fd, win, winlen, MSG_NOSIGNAL);
		switch (n) {
		case 0:
			return ECONNRESET;
		case -1:
			if (errno == EAGAIN)
				return 0;
			return errno;
		default:
			howmuch -= n;
			puffs_framebuf_seekset(pb, CUROFF(pb) + n);
			break;
		}
	}

	*done = 1;
	return 0;
}
#undef CUROFF

int
p9pbuf_cmp(struct puffs_usermount *pu,
	struct puffs_framebuf *c1, struct puffs_framebuf *c2, int *notresp)
{

	return p9pbuf_get_tag(c1) != p9pbuf_get_tag(c2);
}

struct puffs_framebuf *
p9pbuf_makeout()
{
	struct puffs_framebuf *pb;

	pb = puffs_framebuf_make();
	puffs_framebuf_seekset(pb, 4);
	return pb;
}

void
p9pbuf_recycleout(struct puffs_framebuf *pb)
{

	puffs_framebuf_recycle(pb);
	puffs_framebuf_seekset(pb, 4);
}

void
p9pbuf_put_1(struct puffs_framebuf *pb, uint8_t val)
{
	int rv;

	rv = puffs_framebuf_putdata(pb, &val, 1);
	CHECK(rv == 0);
}

void
p9pbuf_put_2(struct puffs_framebuf *pb, uint16_t val)
{
	int rv;

	HTOLE16(val);
	rv = puffs_framebuf_putdata(pb, &val, 2);
	CHECK(rv == 0);
}

void
p9pbuf_put_4(struct puffs_framebuf *pb, uint32_t val)
{
	int rv;

	HTOLE32(val);
	rv = puffs_framebuf_putdata(pb, &val, 4);
	CHECK(rv == 0);
}

void
p9pbuf_put_8(struct puffs_framebuf *pb, uint64_t val)
{
	int rv;

	HTOLE64(val);
	rv = puffs_framebuf_putdata(pb, &val, 8);
	CHECK(rv == 0);
}

void
p9pbuf_put_data(struct puffs_framebuf *pb, const void *data, uint16_t dlen)
{
	int rv;

	p9pbuf_put_2(pb, dlen);
	rv = puffs_framebuf_putdata(pb, data, dlen);
	CHECK(rv == 0);
}

void
p9pbuf_put_str(struct puffs_framebuf *pb, const char *str)
{

	p9pbuf_put_data(pb, str, strlen(str));
}

void
p9pbuf_write_data(struct puffs_framebuf *pb, uint8_t *data, uint32_t dlen)
{
	int rv;

	rv = puffs_framebuf_putdata(pb, data, dlen);
	CHECK(rv == 0);
}

#define ERETURN(rv) return ((rv) == -1 ? errno : 0)

int
p9pbuf_get_1(struct puffs_framebuf *pb, uint8_t *val)
{

	ERETURN(puffs_framebuf_getdata(pb, val, 1));
}

int
p9pbuf_get_2(struct puffs_framebuf *pb, uint16_t *val)
{
	int rv;
	 
	rv = puffs_framebuf_getdata(pb, val, 2);
	LE16TOH(*val);

	ERETURN(rv);
}

int
p9pbuf_get_4(struct puffs_framebuf *pb, uint32_t *val)
{
	int rv;
	 
	rv = puffs_framebuf_getdata(pb, val, 4);
	LE32TOH(*val);

	ERETURN(rv);
}

int
p9pbuf_get_8(struct puffs_framebuf *pb, uint64_t *val)
{
	int rv;
	 
	rv = puffs_framebuf_getdata(pb, val, 8);
	LE64TOH(*val);

	ERETURN(rv);
}

int
p9pbuf_get_data(struct puffs_framebuf *pb, uint8_t **dp, uint16_t *dlenp)
{
        uint8_t *data;              
	uint16_t len;
	int rv;

	rv = p9pbuf_get_2(pb, &len);
	if (rv)
		return errno;

        if (puffs_framebuf_remaining(pb) < len)
                return EPROTO;
 
	if (dp) {
		data = emalloc(len+1);   
		rv = puffs_framebuf_getdata(pb, data, len);
		if (rv) {
			free(data);
			return errno;
		}
		data[len] = '\0';
		*dp = data;
	} else
		puffs_framebuf_seekset(pb, puffs_framebuf_telloff(pb)+len);

	if (dlenp)
		*dlenp = len;

	return 0;
}

int
p9pbuf_read_data(struct puffs_framebuf *pb, uint8_t *buf, uint32_t dlen)
{

	ERETURN(puffs_framebuf_getdata(pb, buf, dlen));
}

int
p9pbuf_get_str(struct puffs_framebuf *pb, char **dp, uint16_t *dlenp)
{

	return p9pbuf_get_data(pb, (uint8_t **)dp, dlenp);
}
