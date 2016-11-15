/*	$NetBSD: audiobell.c,v 1.8 2009/05/12 10:22:31 cegger Exp $	*/

/*
 * Copyright (c) 1999 Richard Earnshaw
 * Copyright (c) 2004 Ben Harris
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the RiscBSD team.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/types.h>
__KERNEL_RCSID(0, "$NetBSD: audiobell.c,v 1.8 2009/05/12 10:22:31 cegger Exp $");

#include <sys/audioio.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/null.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <dev/audio_if.h>
#include <dev/audiobellvar.h>

extern dev_type_open(audioopen);
extern dev_type_write(audiowrite);
extern dev_type_close(audioclose);

/* Convert a %age volume to an amount to add to u-law values */
/* XXX Probably highly inaccurate -- should be regenerated */
static const uint8_t volmap[] = {
	0x7f, 0x67, 0x5b, 0x53, 0x49, 0x45, 0x41, 0x3e, 0x3a, 0x38,
	0x36, 0x32, 0x30, 0x2f, 0x2e, 0x2c, 0x2b, 0x2a, 0x28, 0x27,
	0x26, 0x25, 0x23, 0x22, 0x21, 0x1f, 0x1f, 0x1e, 0x1e, 0x1d,
	0x1c, 0x1c, 0x1b, 0x1a, 0x1a, 0x19, 0x18, 0x18, 0x17, 0x17,
	0x16, 0x15, 0x15, 0x14, 0x13, 0x13, 0x12, 0x11, 0x11, 0x10,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0d, 0x0d, 0x0d,
	0x0c, 0x0c, 0x0c, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x09,
	0x09, 0x09, 0x08, 0x08, 0x08, 0x07, 0x07, 0x07, 0x07, 0x06,
	0x06, 0x06, 0x05, 0x05, 0x05, 0x04, 0x04, 0x04,	0x03, 0x03,
	0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
	0x00
};

/* 1/4 cycle sine wave in u-law */
/* XXX Probably highly inaccurate -- should be regenerated */
static const uint8_t sinewave[] = {
	0xff, 0xd3, 0xc5, 0xbc, 0xb6, 0xb0, 0xad, 0xaa,
	0xa7, 0xa3, 0xa0, 0x9e, 0x9d, 0x9b, 0x9a, 0x98,
	0x97, 0x96, 0x94, 0x93, 0x91, 0x90, 0x8f, 0x8e,
	0x8e, 0x8d, 0x8c, 0x8c, 0x8b, 0x8b, 0x8a, 0x89,
	0x89, 0x88, 0x88, 0x87, 0x87, 0x86, 0x86, 0x85,
	0x85, 0x84, 0x84, 0x84, 0x83, 0x83, 0x83, 0x82,
	0x82, 0x82, 0x81, 0x81, 0x81, 0x81, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80,
};

static inline uint8_t
audiobell_ulawscale(uint8_t val, uint8_t vol)
{
	uint8_t result;

	result = val + vol;
	/* Spot underflow and just return silence */
	if ((result ^ val) & 0x80)
		return 0x7f;
	return result;
}

static inline void
audiobell_expandwave(uint8_t *buf, int volume)
{
	u_int i;
	int uvol;

	KASSERT(volume >= 0 && volume <= 100);
	uvol = volmap[volume];
	for (i = 0; i < 65; i++)
		buf[i] = audiobell_ulawscale(sinewave[i], uvol);
	for (i = 65; i < 128; i++)
		 buf[i] = buf[128 - i];
	for (i = 128; i < 256; i++)
		buf[i] = buf[i - 128] ^ 0x80;
}

/*
 * The algorithm here is based on that described in the RISC OS Programmer's
 * Reference Manual (pp1624--1628).
 */
static inline int
audiobell_synthesize(uint8_t *buf, u_int pitch, u_int period, u_int volume)
{
	uint8_t *wave;
	uint16_t phase;

	wave = malloc(256, M_TEMP, M_WAITOK);
	if (wave == NULL) return -1;
	audiobell_expandwave(wave, volume);
	pitch = pitch * 65536 / 8000;
	period = period * 8; /* 8000 / 1000 */
	phase = 0;

	for (; period != 0; period--) {
		*buf++ = wave[phase >> 8];
		phase += pitch;
	}

	free(wave, M_TEMP);
	return 0;
}

void
audiobell(void *arg, u_int pitch, u_int period, u_int volume, int poll)
{
	device_t audio = arg;
	uint8_t *buf;
	struct uio auio;
	struct iovec aiov;

	/* The audio system isn't built for polling. */
	if (poll) return;

	/* If not configured, we can't beep. */
	if (audioopen(AUDIO_DEVICE | device_unit(audio), FWRITE, 0, NULL) != 0)
		return;

	buf = malloc(period * 8, M_TEMP, M_WAITOK);
	if (buf == NULL) goto out;
	if (audiobell_synthesize(buf, pitch, period, volume) != 0) goto out;

	aiov.iov_base = (void *)buf;
	aiov.iov_len = period * 8;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_resid = period * 8;
	auio.uio_rw = UIO_WRITE;
	UIO_SETUP_SYSSPACE(&auio);

	audiowrite(AUDIO_DEVICE | device_unit(audio), &auio, 0);

out:
	if (buf != NULL) free(buf, M_TEMP);
	audioclose(AUDIO_DEVICE | device_unit(audio), FWRITE, 0, NULL);
}
