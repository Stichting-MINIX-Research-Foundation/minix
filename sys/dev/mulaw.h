/*	$NetBSD: mulaw.h,v 1.20 2008/04/28 20:23:47 martin Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl.
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

#ifndef _SYS_DEV_MULAW_H_
#define _SYS_DEV_MULAW_H_
#include <dev/audio_if.h>

/* Convert 8-bit mu-law to/from 16 bit unsigned/signed linear. */
extern stream_filter_factory_t mulaw_to_linear16;
extern stream_filter_factory_t linear16_to_mulaw;
/* Convert 8-bit mu-law to/from 8 bit unsigned/signed linear. */
extern stream_filter_factory_t mulaw_to_linear8;
extern stream_filter_factory_t linear8_to_mulaw;
/* Convert 8-bit A-law to/from 16 bit unsigned/signed linear. */
extern stream_filter_factory_t alaw_to_linear16;
extern stream_filter_factory_t linear16_to_alaw;
/* Convert 8-bit A-law to/from 8 bit unsigned/signed linear. */
extern stream_filter_factory_t alaw_to_linear8;
extern stream_filter_factory_t linear8_to_alaw;

#endif /* _SYS_DEV_MULAW_H_ */
