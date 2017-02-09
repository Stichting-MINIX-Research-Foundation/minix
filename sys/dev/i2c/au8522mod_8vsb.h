/* $NetBSD: au8522mod_8vsb.h,v 1.2 2011/07/10 00:47:34 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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

static const struct au8522_modulation_table au8522_modulation_8vsb[] = {
	{ 0x8090, 0x84 },
	{ 0x4092, 0x11 },
	{ 0x2005, 0x00 },
	{ 0x8091, 0x80 },
	{ 0x80a3, 0x0c },
	{ 0x80a4, 0xe8 },
	{ 0x8081, 0xc4 },
	{ 0x80a5, 0x40 },
	{ 0x80a7, 0x40 },
	{ 0x80a6, 0x67 },
	{ 0x8262, 0x20 },
	{ 0x821c, 0x30 },
	{ 0x80d8, 0x1a },
	{ 0x8227, 0xa0 },
	{ 0x8121, 0xff },
	{ 0x80a8, 0xf0 },
	{ 0x80a9, 0x05 },
	{ 0x80aa, 0x77 },
	{ 0x80ab, 0xf0 },
	{ 0x80ac, 0x05 },
	{ 0x80ad, 0x77 },
	{ 0x80ae, 0x41 },
	{ 0x80af, 0x66 },
	{ 0x821b, 0xcc },
	{ 0x821d, 0x80 },
	{ 0x80a4, 0xe8 },
	{ 0x8231, 0x13 },
};

static const struct au8522_snr_table au8522_snr_8vsb[] = {
	{ 0, 270 },
	{ 2, 250 },
	{ 3, 240 },
	{ 5, 230 },
	{ 7, 220 },
	{ 9, 210 },
	{ 12, 200 },
	{ 13, 195 },
	{ 15, 190 },
	{ 17, 185 },
	{ 19, 180 },
	{ 21, 175 },
	{ 24, 170 },
	{ 27, 165 },
	{ 31, 160 },
	{ 32, 158 },
	{ 33, 156 },
	{ 36, 152 },
	{ 37, 150 },
	{ 39, 148 },
	{ 40, 146 },
	{ 41, 144 },
	{ 43, 142 },
	{ 44, 140 },
	{ 48, 135 },
	{ 50, 130 },
	{ 43, 142 },
	{ 53, 125 },
	{ 56, 120 },
	{ 256, 115 },
};
