/*	$NetBSD: rumpfiber_bio.c,v 1.5 2014/11/04 19:05:17 pooka Exp $	*/

/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
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

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpfiber_bio.c,v 1.5 2014/11/04 19:05:17 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>

#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#include "rumpuser_int.h"

void
rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t doff,
	rump_biodone_fn biodone, void *bioarg)
{
	ssize_t rv;
	int error = 0;

	if (op & RUMPUSER_BIO_READ) {
		if ((rv = pread(fd, data, dlen, doff)) == -1)
			error = rumpuser__errtrans(errno);
	} else {
		if ((rv = pwrite(fd, data, dlen, doff)) == -1)
			error = rumpuser__errtrans(errno);
		if (error == 0 && (op & RUMPUSER_BIO_SYNC)) {
#ifdef HAVE_FSYNC_RANGE
			fsync_range(fd, FDATASYNC, doff, dlen);
#else
			fsync(fd);
#endif
		}
	}
	if (rv == -1)
		rv = 0;
	biodone(bioarg, (size_t)rv, error);
}
