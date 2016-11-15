/*	$NetBSD: sir.h,v 1.8 2013/05/27 16:23:20 kiyohara Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and Tommy Bohlin
 * (tommy@gatespace.com).
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

/*
 * Framing originally written by Tommy Bohlin.
 */

/* Protocol constants */
#define SIR_EXTRA_BOF            0xff
#define SIR_BOF                  0xc0
#define SIR_CE                   0x7d
#define SIR_EOF                  0xc1

#define SIR_ESC_BIT              0x20

enum framefsmstate {
	FSTATE_END_OF_FRAME,
	FSTATE_START_OF_FRAME,
	FSTATE_IN_DATA,
	FSTATE_IN_END
};

enum frameresult {
	FR_IDLE,
	FR_INPROGRESS,
	FR_FRAMEOK,
	FR_FRAMEBADFCS,
	FR_FRAMEMALFORMED,
	FR_BUFFEROVERRUN
};

struct framestate {
	u_int8_t *buffer;
	size_t buflen;
	size_t bufindex;

	enum framefsmstate fsmstate;
	u_int escaped;
	u_int state_index;
};

#define deframe_isclear(fs) ((fs)->fsmstate == FSTATE_END_OF_FRAME)

int irda_sir_frame(u_int8_t *, u_int, struct uio *, u_int);
void deframe_init(struct framestate *, u_int8_t *, size_t);
void deframe_clear(struct framestate *);
enum frameresult deframe_process(struct framestate *, u_int8_t const **,
				 size_t *);

/*
 * CRC computation
 */
#define INITFCS 0xffff
#define GOODFCS 0xf0b8

extern const u_int16_t irda_fcstab[];

static __inline u_int16_t updateFCS(u_int16_t fcs, int c) {
	return (fcs >> 8) ^ irda_fcstab[(fcs^c) & 0xff];
}

u_int32_t crc_ccitt_16(u_int32_t, u_int8_t const*, size_t);
