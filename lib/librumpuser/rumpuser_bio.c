/*	$NetBSD: rumpuser_bio.c,v 1.10 2014/11/04 19:05:17 pooka Exp $	*/

/*-
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
__RCSID("$NetBSD: rumpuser_bio.c,v 1.10 2014/11/04 19:05:17 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#include "rumpuser_int.h"

struct rumpuser_bio {
	int bio_fd;
	int bio_op;
	void *bio_data;
	size_t bio_dlen;
	off_t bio_off;

	rump_biodone_fn bio_done;
	void *bio_donearg;
};

#define N_BIOS 128
static pthread_mutex_t biomtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t biocv = PTHREAD_COND_INITIALIZER;
static int bio_head, bio_tail;
static struct rumpuser_bio bios[N_BIOS];

static void
dobio(struct rumpuser_bio *biop)
{
	ssize_t rv;
	int error, dummy;

	assert(biop->bio_donearg != NULL);
	if (biop->bio_op & RUMPUSER_BIO_READ) {
		error = 0;
		rv = pread(biop->bio_fd, biop->bio_data,
		    biop->bio_dlen, biop->bio_off);
		if (rv < 0) {
			rv = 0;
			error = rumpuser__errtrans(errno);
		}
	} else {
		error = 0;
		rv = pwrite(biop->bio_fd, biop->bio_data,
		    biop->bio_dlen, biop->bio_off);
		if (rv < 0) {
			rv = 0;
			error = rumpuser__errtrans(errno);
		} else if (biop->bio_op & RUMPUSER_BIO_SYNC) {
#ifdef HAVE_FSYNC_RANGE
			fsync_range(biop->bio_fd, FDATASYNC,
			    biop->bio_off, biop->bio_dlen);
#else
			fsync(biop->bio_fd);
#endif
		}
	}
	rumpkern_sched(0, NULL);
	biop->bio_done(biop->bio_donearg, (size_t)rv, error);
	rumpkern_unsched(&dummy, NULL);

	/* paranoia */
	biop->bio_donearg = NULL;
}

static void *
biothread(void *arg)
{
	struct rumpuser_bio *biop;
	int rv;

	rumpuser__hyp.hyp_schedule();
	rv = rumpuser__hyp.hyp_lwproc_newlwp(0);
	assert(rv == 0);
	rumpuser__hyp.hyp_unschedule();
	NOFAIL_ERRNO(pthread_mutex_lock(&biomtx));
	for (;;) {
		while (bio_head == bio_tail)
			NOFAIL_ERRNO(pthread_cond_wait(&biocv, &biomtx));

		biop = &bios[bio_tail];
		pthread_mutex_unlock(&biomtx);

		dobio(biop);

		NOFAIL_ERRNO(pthread_mutex_lock(&biomtx));
		bio_tail = (bio_tail+1) % N_BIOS;
		pthread_cond_signal(&biocv);
	}

	/* unreachable */
	abort();
}

void
rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t doff,
	rump_biodone_fn biodone, void *bioarg)
{
	struct rumpuser_bio bio;
	static int inited = 0;
	static int usethread = 1;
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);

	if (!inited) {
		pthread_mutex_lock(&biomtx);
		if (!inited) {
			char buf[16];
			pthread_t pt;

			/*
			 * duplicates policy of rump kernel.  maybe a bit
			 * questionable, but since the setting is not
			 * used in normal circumstances, let's not care
			 */
			if (getenv_r("RUMP_THREADS", buf, sizeof(buf)) == 0)
				usethread = *buf != '0';

			if (usethread)
				pthread_create(&pt, NULL, biothread, NULL);
			inited = 1;
		}
		pthread_mutex_unlock(&biomtx);
		assert(inited);
	}

	bio.bio_fd = fd;
	bio.bio_op = op;
	bio.bio_data = data;
	bio.bio_dlen = dlen;
	bio.bio_off = (off_t)doff;
	bio.bio_done = biodone;
	bio.bio_donearg = bioarg;

	if (!usethread) {
		dobio(&bio);
	} else {
		pthread_mutex_lock(&biomtx);
		while ((bio_head+1) % N_BIOS == bio_tail)
			pthread_cond_wait(&biocv, &biomtx);

		bios[bio_head] = bio;
		bio_head = (bio_head+1) % N_BIOS;

		pthread_cond_signal(&biocv);
		pthread_mutex_unlock(&biomtx);
	}

	rumpkern_sched(nlocks, NULL);
}
