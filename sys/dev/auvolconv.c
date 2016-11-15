/* $NetBSD: auvolconv.c,v 1.2 2014/11/23 12:23:25 jmcneill Exp $ */

/*-
 * Copyright (c) 2007 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvolconv.c,v 1.2 2014/11/23 12:23:25 jmcneill Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/condvar.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/endian.h>

#include <dev/audiovar.h>
#include <dev/auconv.h>
#include <dev/auvolconv.h>

int
auvolconv_slinear16_le_fetch_to(struct audio_softc *asc,
    stream_fetcher_t *self, audio_stream_t *dst, int max_used)
{
	auvolconv_filter_t *pf;
	stream_filter_t *this;
	int16_t j, *wp;
	int m, err;
	u_int vol;

	pf = (auvolconv_filter_t *)self;
	this = &pf->base;
	max_used = (max_used + 1) & ~1;
	vol = *pf->vol;

	if ((err = this->prev->fetch_to(asc, this->prev, this->src, max_used)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);
	FILTER_LOOP_PROLOGUE(this->src, 2, dst, 2, m) {
		j = le16dec(s);
		wp = (int16_t *)d;
		le16enc(wp, (j * vol) / 255);
	} FILTER_LOOP_EPILOGUE(this->src, dst);

	return 0;
}

int
auvolconv_slinear16_be_fetch_to(struct audio_softc *asc,
    stream_fetcher_t *self, audio_stream_t *dst, int max_used)
{
	auvolconv_filter_t *pf;
	stream_filter_t *this;
	int16_t j, *wp;
	int m, err;
	u_int vol;

	pf = (auvolconv_filter_t *)self;
	this = &pf->base;
	max_used = (max_used + 1) & ~1;
	vol = *pf->vol;

	if ((err = this->prev->fetch_to(asc, this->prev, this->src, max_used)))
		return err;
	m = (dst->end - dst->start) & ~1;
	m = min(m, max_used);
	FILTER_LOOP_PROLOGUE(this->src, 2, dst, 2, m) {
		j = be16dec(s);
		wp = (int16_t *)d;
		be16enc(wp, (j * vol) / 255);
	} FILTER_LOOP_EPILOGUE(this->src, dst);

	return 0;
}
