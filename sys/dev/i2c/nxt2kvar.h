/* $NetBSD: nxt2kvar.h,v 1.1 2011/07/11 00:30:23 jakllsch Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_I2C_NXT2KVAR_H_
#define _DEV_I2C_NXT2KVAR_H_

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvio.h>

struct nxt2k;

struct nxt2k * nxt2k_open(device_t, i2c_tag_t, i2c_addr_t, unsigned int);
void nxt2k_close(struct nxt2k *);
void nxt2k_enable(struct nxt2k *, bool);
int nxt2k_set_modulation(struct nxt2k *, fe_modulation_t);
fe_status_t nxt2k_get_dtv_status(struct nxt2k *);
uint16_t nxt2k_get_signal(struct nxt2k *);
uint16_t nxt2k_get_snr(struct nxt2k *);

#endif /* _DEV_I2C_NXT2KVAR_H_ */
