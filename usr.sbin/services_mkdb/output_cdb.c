/*	$NetBSD: output_cdb.c,v 1.1 2010/04/25 00:54:46 joerg Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/endian.h>
#include <cdbw.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <unistd.h>

#include "extern.h"

static struct cdbw *cdbw;
static int cdbw_fd = -1;

int
cdb_open(const char *tname)
{

	if ((cdbw = cdbw_open()) == NULL)
		return -1;

	if ((cdbw_fd = open(tname, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		cdbw_close(cdbw);
		cdbw = NULL;
		return -1;
	}
	return 0;
}

void
cdb_add(StringList *sl, size_t port, const char *proto, size_t *cnt,
    int warndup)
{
	uint8_t key[255 * 2 + 2];
	uint8_t *data, *data_iter;
	size_t len, protolen, datalen, keylen;
	uint32_t idx;
	size_t i;

	protolen = strlen(proto);
	if (protolen == 0 || protolen > 255)
		errx(1, "Invalid protocol ``%s'', entry skipped", proto);

	datalen = 4 + protolen;
	for (i = 0; i < sl->sl_cur; ++i) {
		len = strlen(sl->sl_str[i]);
		if (len == 0 || len > 255)
			errx(1, "Service alias ``%s'' invalid", sl->sl_str[i]);
		datalen += len + 2;
	}

	data = malloc(datalen);
	if (data == NULL)
		err(1, "malloc failed");
	be16enc(data, port);
	data[2] = protolen;
	data_iter = data + 3;
	memcpy(data_iter, proto, protolen + 1);
	data_iter += protolen + 1;
	for (i = 0; i < sl->sl_cur; ++i) {
		len = strlen(sl->sl_str[i]);
		*data_iter++ = len;
		memcpy(data_iter, sl->sl_str[i], len + 1);
		data_iter += len + 1;
	}

	if (cdbw_put_data(cdbw, data, datalen, &idx))
		err(1, "cdbw_put_data failed");

	free(data);

	key[0] = 0;
	key[1] = protolen;
	be16enc(key + 2, port);
	memcpy(key + 4, proto, protolen);
	keylen = 4 + protolen;
	if (cdbw_put_key(cdbw, key, keylen, idx) && warndup)
		warnx("duplicate service: `%zu/%s'", port, proto);

	key[1] = 0;
	keylen = 4;
	if (cdbw_put_key(cdbw, key, keylen, idx) && warndup)
		warnx("duplicate service: `%zu'", port);

	/* add references for service and all aliases */
	for (i = 0; i < sl->sl_cur; i++) {
		len = strlen(sl->sl_str[i]);
		key[0] = len;
		key[1] = protolen;
		memcpy(key + 2, sl->sl_str[i], len);
		memcpy(key + 2 + len, proto, protolen);
		keylen = 2 + len + protolen;
		if (cdbw_put_key(cdbw, key, keylen, idx) && warndup)
			warnx("duplicate service: `%s/%s'", sl->sl_str[i], proto);

		key[1] = 0;
		keylen = 2 + len;
		if (cdbw_put_key(cdbw, key, keylen, idx) && warndup)
			warnx("duplicate service: `%s'", sl->sl_str[i]);
	}

	sl_free(sl, 1);
}

int
cdb_close(void)
{
	int rv, serrno;

	rv = 0;
	serrno = errno;

	if (cdbw_output(cdbw, cdbw_fd, "services(5)", NULL)) {
		rv = -1;
		serrno = errno;
	}

	cdbw_close(cdbw);
	cdbw = NULL;

	if (close(cdbw_fd)) {
		if (rv == 0)
			serrno = errno;
		rv = -1;
	}
	cdbw_fd = -1;

	errno = serrno;
	return rv;
}
