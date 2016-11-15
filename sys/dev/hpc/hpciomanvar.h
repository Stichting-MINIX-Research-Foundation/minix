/*	$NetBSD: hpciomanvar.h,v 1.3 2008/04/28 20:23:48 martin Exp $ */

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#ifndef _DEV_HPC_HPCIOMANVAR_H_
#define _DEV_HPC_HPCIOMANVAR_H_

struct hpcioman_attach_args {
	hpcio_chip_t hma_hc;
	int hma_intr_mode;
	int hma_type;
	int hma_id;
	int hma_port;
	int hma_initvalue;
	int hma_on;
	int hma_off;
	int hma_connect;
};

#define cf_iochip	cf_loc[HPCIOMANCF_IOCHIP]
#define cf_port		cf_loc[HPCIOMANCF_PORT]
#define cf_type		cf_loc[HPCIOMANCF_EVTYPE]
#define cf_id		cf_loc[HPCIOMANCF_ID]
#define cf_level	cf_loc[HPCIOMANCF_LEVEL]
#define cf_edge		cf_loc[HPCIOMANCF_EDGE]
#define cf_active	cf_loc[HPCIOMANCF_ACTIVE]
#define cf_initvalue	cf_loc[HPCIOMANCF_INITVALUE]
#define cf_hold		cf_loc[HPCIOMANCF_HOLD]
#define cf_connect	cf_loc[HPCIOMANCF_CONNECT]

#endif /* !_DEV_HPC_HPCIOMANVAR_H_ */
