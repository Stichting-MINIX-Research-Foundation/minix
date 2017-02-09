/* $NetBSD: au8522mod_qam256.h,v 1.2 2011/07/10 00:47:34 jmcneill Exp $ */

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

static const struct au8522_modulation_table au8522_modulation_qam256[] = {
	{ 0x80a3, 0x09 },
	{ 0x80a4, 0x00 },
	{ 0x8081, 0xc4 },
	{ 0x80a5, 0x40 },
	{ 0x80aa, 0x77 },
	{ 0x80ad, 0x77 },
	{ 0x80a6, 0x67 },
	{ 0x8262, 0x20 },
	{ 0x821c, 0x30 },
	{ 0x80b8, 0x3e },
	{ 0x80b9, 0xf0 },
	{ 0x80ba, 0x01 },
	{ 0x80bb, 0x18 },
	{ 0x80bc, 0x50 },
	{ 0x80bd, 0x00 },
	{ 0x80be, 0xea },
	{ 0x80bf, 0xef },
	{ 0x80c0, 0xfc },
	{ 0x80c1, 0xbd },
	{ 0x80c2, 0x1f },
	{ 0x80c3, 0xfc },
	{ 0x80c4, 0xdd },
	{ 0x80c5, 0xaf },
	{ 0x80c6, 0x00 },
	{ 0x80c7, 0x38 },
	{ 0x80c8, 0x30 },
	{ 0x80c9, 0x05 },
	{ 0x80ca, 0x4a },
	{ 0x80cb, 0xd0 },
	{ 0x80cc, 0x01 },
	{ 0x80cd, 0xd9 },
	{ 0x80ce, 0x6f },
	{ 0x80cf, 0xf9 },
	{ 0x80d0, 0x70 },
	{ 0x80d1, 0xdf },
	{ 0x80d2, 0xf7 },
	{ 0x80d3, 0xc2 },
	{ 0x80d4, 0xdf },
	{ 0x80d5, 0x02 },
	{ 0x80d6, 0x9a },
	{ 0x80d7, 0xd0 },
	{ 0x8250, 0x0d },
	{ 0x8251, 0xcd },
	{ 0x8252, 0xe0 },
	{ 0x8253, 0x05 },
	{ 0x8254, 0xa7 },
	{ 0x8255, 0xff },
	{ 0x8256, 0xed },
	{ 0x8257, 0x5b },
	{ 0x8258, 0xae },
	{ 0x8259, 0xe6 },
	{ 0x825a, 0x3d },
	{ 0x825b, 0x0f },
	{ 0x825c, 0x0d },
	{ 0x825d, 0xea },
	{ 0x825e, 0xf2 },
	{ 0x825f, 0x51 },
	{ 0x8260, 0xf5 },
	{ 0x8261, 0x06 },
	{ 0x821a, 0x00 },
	{ 0x8546, 0x40 },
	{ 0x8210, 0x26 },
	{ 0x8211, 0xf6 },
	{ 0x8212, 0x84 },
	{ 0x8213, 0x02 },
	{ 0x8502, 0x01 },
	{ 0x8121, 0x04 },
	{ 0x8122, 0x04 },
	{ 0x852e, 0x10 },
	{ 0x80a4, 0xca },
	{ 0x80a7, 0x40 },
	{ 0x8526, 0x01 },
};

static const struct au8522_snr_table au8522_snr_qam256[] = {
	{ 16, 0 },
	{ 17, 400 },
	{ 18, 398 },
	{ 19, 396 },
	{ 20, 394 },
	{ 21, 392 },
	{ 22, 390 },
	{ 23, 388 },
	{ 24, 386 },
	{ 25, 384 },
	{ 26, 382 },
	{ 27, 380 },
	{ 28, 379 },
	{ 29, 378 },
	{ 30, 377 },
	{ 31, 376 },
	{ 32, 375 },
	{ 33, 374 },
	{ 34, 373 },
	{ 35, 372 },
	{ 36, 371 },
	{ 37, 370 },
	{ 38, 362 },
	{ 39, 354 },
	{ 40, 346 },
	{ 41, 338 },
	{ 42, 330 },
	{ 43, 328 },
	{ 44, 326 },
	{ 45, 324 },
	{ 46, 322 },
	{ 47, 320 },
	{ 48, 319 },
	{ 49, 318 },
	{ 50, 317 },
	{ 51, 316 },
	{ 52, 315 },
	{ 53, 314 },
	{ 54, 313 },
	{ 55, 312 },
	{ 56, 311 },
	{ 57, 310 },
	{ 58, 308 },
	{ 59, 306 },
	{ 60, 304 },
	{ 61, 302 },
	{ 62, 300 },
	{ 63, 298 },
	{ 65, 295 },
	{ 68, 294 },
	{ 70, 293 },
	{ 73, 292 },
	{ 76, 291 },
	{ 78, 290 },
	{ 79, 289 },
	{ 81, 288 },
	{ 82, 287 },
	{ 83, 286 },
	{ 84, 285 },
	{ 85, 284 },
	{ 86, 283 },
	{ 88, 282 },
	{ 89, 281 },
	{ 256, 280 },
};
