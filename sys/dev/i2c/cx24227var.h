/* $NetBSD: cx24227var.h,v 1.1 2011/08/04 01:48:34 jakllsch Exp $ */

/*
 * Copyright (c) 2011 Jonathan A. Kollasch
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

#ifndef _DEV_I2C_CX24227VAR_H_
#define _DEV_I2C_CX24227VAR_H_

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvio.h>

struct cx24227;

struct cx24227 * cx24227_open(device_t, i2c_tag_t, i2c_addr_t);
void cx24227_close(struct cx24227 *);
void cx24227_enable(struct cx24227 *, bool);
int cx24227_set_modulation(struct cx24227 *, fe_modulation_t);
fe_status_t cx24227_get_dtv_status(struct cx24227 *);
uint16_t cx24227_get_signal(struct cx24227 *);
uint16_t cx24227_get_snr(struct cx24227 *);

#endif /* _DEV_I2C_CX24227VAR_H_ */
